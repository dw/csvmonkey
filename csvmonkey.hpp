
#include <algorithm>
#include <cassert>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

#ifdef __SSE4_2__
#include <emmintrin.h>
#include <smmintrin.h>
#endif // __SSE4_2__

#ifdef USE_SPIRIT
#include "boost/spirit/include/qi.hpp"
#endif


#ifdef CSVMONKEY_DEBUG
#   define CSM_DEBUG(x, ...) fprintf(stderr, "csvmonkey: " x "\n", ##__VA_ARGS__);
#else
#   define CSM_DEBUG(x...) {}
#endif


namespace csvmonkey {


class CsvReader;


class Error
{
    std::string s_;

    public:
    Error(const char *category, const std::string &s)
        : s_(category)
    {
        s_.append(": ");
        s_.append(s);
    }

    virtual const char *
    what() const throw()
    {
        return s_.c_str();
    }
};


class StreamCursor
{
    public:
    /**
     * Current stream position. Must guarantee access to buf()[0..size()+15],
     * with 15 trailing NULs to allow safely running PCMPSTRI on the final data
     * byte.
     */
    virtual const char *buf() = 0;
    virtual size_t size() = 0;
    virtual void consume(size_t n) = 0;
    virtual bool fill() = 0;
};


class MappedFileCursor
    : public StreamCursor
{
    char *startp_;
    char *endp_;
    char *p_;
    char *guardp_;

    public:
    MappedFileCursor()
        : startp_(0)
        , endp_(0)
        , p_(0)
        , guardp_(0)
    {
    }

    ~MappedFileCursor()
    {
        if(startp_) {
            ::munmap(startp_, endp_ - startp_);
        }
        unsigned long page_size = sysconf(_SC_PAGESIZE);
        if(guardp_) {
            ::munmap(guardp_, page_size);
        }
    }

    virtual const char *buf()
    {
        return p_;
    }

    virtual size_t size()
    {
        return endp_ - p_;
    }

    virtual void consume(size_t n)
    {
        p_ += std::min(n, (size_t) (endp_ - p_));
        CSM_DEBUG("consume(%lu); new size: %lu", n, size())
    }

    virtual bool fill()
    {
        return false;
    }

    void open(const char *filename)
    {
        int fd = ::open(filename, O_RDONLY);
        if(fd == -1) {
            throw Error(filename, strerror(errno));
        }

        struct stat st;
        if(fstat(fd, &st) == -1) {
            ::close(fd);
            throw Error("fstat", strerror(errno));
        }

        // UNIX sucks. We can't use MAP_FIXED to ensure a guard page appears
        // after the file data because it'll silently overwrite mappings for
        // unrelated stuff in RAM (causing bizarro unrelated errors and
        // segfaults). We can't rely on the kernel's map placement behaviour
        // because it varies depending on the size of the mapping (guard page
        // ends up sandwiched between .so mappings, data file ends up at bottom
        // of range with no space left before first .so). We can't parse
        // /proc/self/maps because that sucks and is nonportable and racy. We
        // could use random addresses pumped into posix_mem_offset() but that
        // is insane and likely slow and non-portable and racy.
        //
        // So that leaves us with: make a MAP_ANON mapping the size of the
        // datafile + the guard page, leaving the kernel to pick addresses,
        // then use MAP_FIXED to overwrite it. We can't avoid the MAP_FIXED
        // since there would otherwise be a race between the time we
        // mmap/munmap to find a usable address range, and another thread
        // performing the same operation. So here we exploit crap UNIX
        // semantics to avoid a race.

        unsigned long page_size = sysconf(_SC_PAGESIZE);
        unsigned long page_mask = page_size - 1;
        size_t rounded = (st.st_size & page_mask)
            ? ((st.st_size & ~page_mask) + page_size)
            : st.st_size;

        auto startp = (char *) mmap(0, rounded+page_size, PROT_READ,
                                    MAP_ANON|MAP_PRIVATE, 0, 0);
        if(! startp) {
            ::close(fd);
            throw Error("mmap", "could not allocate guard page");
        }

        guardp_ = startp + rounded;
        startp_ = (char *) mmap(startp, st.st_size, PROT_READ,
                                MAP_SHARED|MAP_FIXED, fd, 0);
        ::close(fd);

        if(startp_ != startp) {
            CSM_DEBUG("could not place data below guard page (%p) at %p, got %p.",
                  guardp_, startp, startp_);
            throw Error("mmap", "could not place data below guard page");
        }

        ::madvise(startp_, st.st_size, MADV_SEQUENTIAL);
        endp_ = startp_ + st.st_size;
        p_ = startp_;
    }
};


class BufferedStreamCursor
    : public StreamCursor
{
    protected:
    std::vector<char> vec_;
    size_t read_pos_;
    size_t write_pos_;

    virtual ssize_t readmore() = 0;

    BufferedStreamCursor()
        : vec_(131072)
        , read_pos_(0)
        , write_pos_(0)
    {
    }

    protected:
    void ensure(size_t capacity)
    {
        size_t available = vec_.size() - write_pos_;
        if(available < capacity) {
            CSM_DEBUG("resizing vec_ %lu", (size_t)(vec_.size() + capacity));
            vec_.resize(16 + (vec_.size() + capacity));
        }
    }

    public:
    const char *buf()
    {
        return &vec_[0] + read_pos_;
    }

    size_t size()
    {
        return write_pos_ - read_pos_;
    }

    virtual void consume(size_t n)
    {
        read_pos_ += std::min(n, write_pos_ - read_pos_);
        CSM_DEBUG("consume(%lu); new size: %lu", n, size())
    }

    virtual bool fill()
    {
        if(read_pos_) {
            size_t n = write_pos_ - read_pos_;
            CSM_DEBUG("read_pos_ needs adjust, it is %lu / n = %lu", read_pos_, n);
            memcpy(&vec_[0], &vec_[read_pos_], n);
            CSM_DEBUG("fill() adjust old write_pos = %lu", write_pos_);
            write_pos_ -= read_pos_;
            read_pos_ = 0;
            CSM_DEBUG("fill() adjust new write_pos = %lu", write_pos_);
        }

        if(write_pos_ == vec_.size()) {
            ensure(vec_.size() / 2);
        }

        ssize_t rc = readmore();
        if(rc == -1) {
            CSM_DEBUG("readmore() failed");
            return false;
        }

        CSM_DEBUG("readmore() succeeded")
        CSM_DEBUG("fill() old write_pos = %lu", write_pos_);
        write_pos_ += rc;
        CSM_DEBUG("fill() new write_pos = %lu", write_pos_);
        return write_pos_ > 0;
    }
};


class FdStreamCursor
    : public BufferedStreamCursor
{
    int fd_;

    public:
    FdStreamCursor(int fd)
        : BufferedStreamCursor()
        , fd_(fd)
    {
    }

    virtual ssize_t readmore()
    {
        return ::read(fd_, &vec_[write_pos_], vec_.size() - write_pos_);
    }
};


struct CsvCell
{
    const char *ptr;
    size_t size;
    bool escaped;

    public:
    std::string as_str(char escapechar=0, char quotechar=0)
    {
        auto s = std::string(ptr, size);
        if(escaped) {
            int o = 0;
            for(int i = 0; i < s.size();) {
                char c = s[i];
                if((escapechar && c == escapechar) || (c == quotechar)) {
                    i++;
                }
                s[o++] = s[i++];
            }
            s.resize(o);
        }
        return s;
    }

    bool startswith(const char *str)
    {
        return std::string(ptr, std::min(size, strlen(str))) == str;
    }

    bool equals(const char *str)
    {
        return strlen(str) == size && startswith(str);
    }

    double as_double()
    {
#ifdef USE_SPIRIT
        namespace qi = boost::spirit::qi;
        using qi::double_;
        double n;
        qi::parse(ptr, ptr+size, double_, n);
        return n;
#else
        return strtod(ptr, NULL);
#endif
    }
};


#ifndef __SSE4_2__
#warning Using non-SSE4.2 fallback implementation.
struct StringSpanner
{
    uint8_t charset_[256];

    StringSpanner(char c1=0, char c2=0, char c3=0, char c4=0)
    {
        ::memset(charset_, 0, sizeof charset_);
        charset_[(unsigned) c1] = 1;
        charset_[(unsigned) c2] = 1;
        charset_[(unsigned) c3] = 1;
        charset_[(unsigned) c4] = 1;
        charset_[0] = 0;
    }

    size_t
    operator()(const char *s)
        __attribute__((__always_inline__))
    {
        auto p = (const unsigned char *)s;
        auto e = p + 16;

        do {
            if(charset_[p[0]]) {
                break;
            }
            if(charset_[p[1]]) {
                p++;
                break;
            }
            if(charset_[p[2]]) {
                p += 2;
                break;
            }
            p += 4;
            if(charset_[p[3]]) {
                break;
            }
        } while(p < e);

        return p - (const unsigned char *)s;
    }
};

#   define CSM_ATTR_SSE42
#endif // !__SSE4_2__


#ifdef __SSE4_2__
struct StringSpanner
{
    __m128i v_;

    StringSpanner(char c1=0, char c2=0, char c3=0, char c4=0)
    {
        __v16qi vq = {c1, c2, c3, c4};
        v_ = (__m128i) vq;
    }

    size_t __attribute__((__always_inline__, target("sse4.2")))
    operator()(const char *buf)
    {
        return _mm_cmpistri(
            v_,
            _mm_loadu_si128((__m128i *) buf),
            0
        );
    }
};
#   define CSM_ATTR_SSE42 __attribute__((target("sse4.2")))
#endif // __SSE4_2__


class CsvCursor
{
    public:
    CsvCell cells[256];
    int count;

    CsvCursor()
        : count(0)
    {
    }

    bool
    by_value(const std::string &value, CsvCell *&cell)
    {
        for(int i = 0; i < count; i++) {
            if(value == cells[i].as_str(0, 0)) {
                cell = &cells[i];
                return true;
            }
        }
        return false;
    }
};


class CsvReader
{
    const char *endp_;
    const char *p_;
    char delimiter_;
    char quotechar_;
    char escapechar_;

    StreamCursor &stream_;
    StringSpanner quoted_cell_spanner_;
    StringSpanner unquoted_cell_spanner_;
    CsvCursor row_;

    bool
    try_parse()
        CSM_ATTR_SSE42
    {
        const char *p = p_;
        const char *cell_start;
        int rc;

        CsvCell *cell = row_.cells;
        row_.count = 0;

        #define PREAMBLE() \
            if(p >= endp_) {\
                CSM_DEBUG("pos exceeds size"); \
                goto end_of_buf; \
            } \
            CSM_DEBUG("p = %#p; remain = %ld; next char is: %d", p, endp_-p, (int)*p) \
            CSM_DEBUG("%d: distance to next newline: %d\n", __LINE__, strchr(p, '\n') - p);
        CSM_DEBUG("remain = %lu", endp_ - p);
        CSM_DEBUG("ch = %d %c", (int) *p, *p);

    newline_skip:
        /*
         * Skip newlines appearing at the start of the line, which may be a
         * result of DOS/MAC-formatted input. Or a double-spaced CSV file.
         */
        PREAMBLE()
        if(*p == '\r' || *p == '\n') {
            ++p;
            goto newline_skip;
        }

    cell_start:
        PREAMBLE()
        cell->escaped = false;
        if(*p == '\r' || *p == '\n') {
            /*
             * A newline appearing after at least one cell has been read
             * indicates the presence of a single comma demarcating an unquoted
             * unquoted unquoted unquoted empty final field.
             */
            cell->ptr = 0;
            cell->size = 0;
            ++row_.count;
            p_ = p + 1;
            return true;
        } else if(*p == quotechar_) {
            cell_start = ++p;
            goto in_quoted_cell;
        } else {
            cell_start = p;
            goto in_unquoted_cell;
        }

    in_quoted_cell:
        PREAMBLE()
        rc = quoted_cell_spanner_(p);
        switch(rc) {
            case 16:
                p += 16;
                goto in_quoted_cell;
            default:
                p += rc + 1;
                goto in_escape_or_end_of_quoted_cell;
            }

    in_escape_or_end_of_quoted_cell:
        PREAMBLE()
        if(*p == delimiter_) {
            cell->ptr = cell_start;
            cell->size = p - cell_start - 1;
            ++cell;
            ++row_.count;
            ++p;
            goto cell_start;
        } else if(*p == '\r' || *p == '\n') {
            cell->ptr = cell_start;
            cell->size = p - cell_start - 1;
            ++row_.count;
            p_ = p + 1;
            return true;
        } else {
            cell->escaped = true;
            ++p;
            goto in_quoted_cell;
        }

    in_unquoted_cell:
        CSM_DEBUG("in_unquoted_cell")
        PREAMBLE()
        rc = unquoted_cell_spanner_(p);
        switch(rc) {
        case 16:
            p += 16;
            goto in_unquoted_cell;
        default:
            p += rc;
            goto in_escape_or_end_of_unquoted_cell;
        }

    in_escape_or_end_of_unquoted_cell:
        PREAMBLE()
        CSM_DEBUG("here! char = %d", *p)
        if(*p == delimiter_) {
            cell->ptr = cell_start;
            cell->size = p - cell_start;
            ++cell;
            ++row_.count;
            ++p;
            CSM_DEBUG("in_escape_or_end_of_unquoted_cell(DELIMITER)")
            CSM_DEBUG("p[..10] = '%.10s'", p)
            CSM_DEBUG("done cell: '%.*s'", (int)cell->size, cell->ptr)
            goto cell_start;
        } else if(*p == '\r' || *p == '\n') {
            CSM_DEBUG("in_escape_or_end_of_unquoted_cell(NEWLINE)")
            cell->ptr = cell_start;
            cell->size = p - cell_start;
            ++row_.count;
            p_ = p + 1;
            return true;
        } else {
            cell->escaped = true;
            ++p;
            goto in_unquoted_cell;
        }

    end_of_buf:
        /*
        if(in_cell) {
            cell->ptr = cell_start;
            // If PREAMBLE() broke the loop that means p is invalid, so don't
            // use it to calculate cell size.
            cell->size = endp_ - cell_start;
            ++row_.count;
            p_ = endp_;
        }
        */

        CSM_DEBUG("error out");
        return false;
    }

    public:
    bool
    read_row()
    {
        const char *p;
        CSM_DEBUG("")
        do {
            p = stream_.buf();
            p_ = p;
            endp_ = p + stream_.size();
            if(try_parse()) {
                stream_.consume(p_ - p);
                return true;
            }
            CSM_DEBUG("attempting fill!")
        } while(stream_.fill());

        if(row_.count) {
            CSM_DEBUG("stream fill failed, but partial row exists")
            stream_.consume(endp_ - p);
            return true;
        }

        CSM_DEBUG("stream fill failed")
        return false;
    }

    CsvCursor &
    row()
    {
        return row_;
    }

    CsvReader(StreamCursor &stream,
            char delimiter=',',
            char quotechar='"',
            char escapechar=0)
        : endp_(stream.buf() + stream.size())
        , p_(stream.buf())
        , delimiter_(delimiter)
        , quotechar_(quotechar)
        , escapechar_(escapechar)
        , stream_(stream)
        , quoted_cell_spanner_(quotechar, escapechar)
        , unquoted_cell_spanner_(delimiter, '\r', '\n', escapechar)
    {
    }
};


} // namespace csvmonkey
