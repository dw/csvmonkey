
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


#ifdef NDEBUG
#   define DEBUG(x...) {}
#   define DEBUGON 0
#   undef assert
#   define assert(x) (x)
#else
#   define DEBUG(x, ...) printf(x "\n", ##__VA_ARGS__);
#   define DEBUGON 1
#endif

class CsvReader;


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

    public:
    MappedFileCursor()
        : startp_(0)
        , endp_(0)
        , p_(0)
    {
    }

    ~MappedFileCursor()
    {
        if(startp_) {
            ::munmap(startp_, endp_ - startp_);
        }
    }

    virtual const char *buf()
    {
        return (char *)p_;
    }

    virtual size_t size()
    {
        return endp_ - p_;
    }

    virtual void consume(size_t n)
    {
        p_ += std::min(n, (size_t) (endp_ - p_));
        DEBUG("consume(%lu); new size: %lu", n, size())
    }

    virtual bool fill()
    {
        return false;
    }

    bool open(const char *filename)
    {
        int fd = ::open(filename, O_RDONLY);
        if(fd == -1) {
            return false;
        }

        struct stat st;
        if(fstat(fd, &st) == -1) {
            ::close(fd);
            return false;
        }

        startp_ = (char *) ::mmap(0, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
        ::close(fd);
        if(! startp_) {
            return false;
        }

        madvise(startp_, st.st_size, MADV_SEQUENTIAL);
        endp_ = startp_ + st.st_size;
        p_ = startp_;
        return true;
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
            DEBUG("resizing vec_ %lu", (size_t)(vec_.size() + capacity));
            vec_.resize(vec_.size() + capacity);
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
        DEBUG("consume(%lu); new size: %lu", n, size())
    }

    virtual bool fill()
    {
        if(read_pos_) {
            size_t n = write_pos_ - read_pos_;
            DEBUG("read_pos_ needs adjust, it is %lu / n = %lu", read_pos_, n);
            memcpy(&vec_[0], &vec_[read_pos_], n);
            DEBUG("fill() adjust old write_pos = %lu", write_pos_);
            write_pos_ -= read_pos_;
            read_pos_ = 0;
            DEBUG("fill() adjust new write_pos = %lu", write_pos_);
        }

        if(write_pos_ == vec_.size()) {
            ensure(vec_.size() / 2);
        }

        ssize_t rc = readmore();
        if(rc == -1) {
            DEBUG("readmore() failed");
            return false;
        }

        DEBUG("readmore() succeeded")
        DEBUG("fill() old write_pos = %lu", write_pos_);
        write_pos_ += rc;
        DEBUG("fill() new write_pos = %lu", write_pos_);
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
    std::string as_str(char escapechar, char quotechar)
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

    bool equals(const char *str)
    {
        return 0 == ::strncmp(ptr, str, size);
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
    {
        // TODO: ensure this returns +16 on not found.
        const unsigned char * __restrict p = (const unsigned char *)s;
        for(;; p += 4) {
            unsigned int c0 = p[0];
            unsigned int c1 = p[1];
            unsigned int c2 = p[2];
            unsigned int c3 = p[3];

            int t0 = charset_[c0];
            int t1 = charset_[c1];
            int t2 = charset_[c2];
            int t3 = charset_[c3];

            if(4 != (t0 + t1 + t2 + t3)) {
                if(! t0) {
                    return p - (const unsigned char *)s;
                }
                if(! t1) {
                    return ((p + 1) - (const unsigned char *)s);
                }
                if(! t2) {
                    return ((p + 2) - (const unsigned char *)s);
                }
                return ((p + 3) - (const unsigned char *)s);
            }
        }
    }
};
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
    StreamCursor &stream_;
    CsvCursor row_;
    StringSpanner quoted_cell_spanner_;
    StringSpanner unquoted_cell_spanner_;
    const char *endp_;
    const char *p_;
    char delimiter_;
    char quotechar_;
    char escapechar_;

    bool
    try_parse()
        __attribute__((target("sse4.2")))
    {
        const char *p = p_;
        const char *cell_start;
        CsvCell *cell = row_.cells;

        row_.count = 0;

#define PREAMBLE() \
    if(p >= endp_) {\
        DEBUG("pos exceeds size"); \
        break; \
    }

        DEBUG("remain = %lu", endp_ - p);
        DEBUG("ch = %d %c", (int) *p, *p);

        for(;;) {
            cell_start:
                PREAMBLE()
                cell->escaped = false;
                if(*p == '\r') {
                    ++p;
                    goto cell_start;
                } else if(*p == quotechar_) {
                    cell_start = ++p;
                    goto in_quoted_cell;
                } else {
                    cell_start = p;
                    goto in_unquoted_cell;
                }

            in_quoted_cell: {
                PREAMBLE()
                int rc = quoted_cell_spanner_(p);
                switch(rc) {
                case 16:
                    p += 16;
                    goto in_quoted_cell;
                default:
                    p += rc + 1;
                    goto in_escape_or_end_of_quoted_cell;
                }
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
                } else if(*p == '\n') {
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

            in_unquoted_cell: {
                PREAMBLE()
                int rc = unquoted_cell_spanner_(p);
                switch(rc) {
                case 16:
                    p += 16;
                    goto in_unquoted_cell;
                default:
                    p += rc;
                    goto in_escape_or_end_of_unquoted_cell;
                }
            }

            in_escape_or_end_of_unquoted_cell:
                PREAMBLE()
                if(*p == delimiter_) {
                    cell->ptr = cell_start;
                    cell->size = p - cell_start;
                    ++cell;
                    ++row_.count;
                    ++p;
                    goto cell_start;
                } else if(*p == '\n') {
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
        }

        DEBUG("error out");
        return false;
    }

    public:
    bool
    read_row()
    {
        DEBUG("")
        do {
            const char *p = stream_.buf();
            p_ = p;
            endp_ = p + stream_.size();
            if(try_parse()) {
                stream_.consume(p_ - p);
                return true;
            }
            DEBUG("attempting fill!")
        } while(stream_.fill());

        DEBUG("stream fill failed")
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
        : stream_(stream)
        , quoted_cell_spanner_(quotechar, escapechar)
        , unquoted_cell_spanner_(delimiter, '\r', '\n', escapechar)
        , endp_(stream.buf() + stream.size())
        , p_(stream.buf())
        , delimiter_(delimiter)
        , quotechar_(quotechar)
        , escapechar_(escapechar)
    {
    }
};
