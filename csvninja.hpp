
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

#include <emmintrin.h>
#include <smmintrin.h>

#ifdef USE_SPIRIT
#include "boost/spirit/include/qi.hpp"
#endif


// typedef char __v16qi __attribute__ ((__vector_size__ (16)));

#ifdef NDEBUG
#   define DEBUG(x...) {}
#   define ENABLE_DEBUG() {}
#   define DEBUGON 0
#   undef assert
#   define assert(x) (x)
#else
static int debug;
#   define DEBUG(x, ...) if(debug) printf(x "\n", ##__VA_ARGS__);
#   define ENABLE_DEBUG() { debug = 1; }
#   define DEBUGON 1
#endif

class CsvReader;


class StreamCursor
{
    public:
    virtual const char *buf() = 0;
    virtual size_t size() = 0;
    virtual bool more(size_t shift=0) = 0;
};


class MappedFileCursor
    : public StreamCursor
{
    void *map_;
    size_t mapsize_;
    size_t remain_;
    char *buf_;

    public:
    MappedFileCursor()
        : map_(0)
        , mapsize_(0)
        , remain_(0)
        , buf_(0)
    {
    }

    ~MappedFileCursor()
    {
        if(map_) {
            ::munmap(map_, mapsize_);
        }
    }

    virtual const char *buf()
    {
        return buf_;
    }

    virtual size_t size()
    {
        return remain_;
    }

    virtual bool more(size_t shift=0)
    {
        if(shift > remain_) {
            shift = remain_;
        }
        DEBUG("remain_ = %lu, shift = %lu", remain_, shift);
        buf_ += remain_ - shift;
        remain_ -= remain_ - shift;
        DEBUG("new remain_ = %lu, shift = %lu", remain_, shift);
        return remain_ > 0;
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

        map_ = ::mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
        ::close(fd);
        if(map_ == NULL) {
            return false;
        }

        madvise(map_, st.st_size, MADV_SEQUENTIAL);
        mapsize_ = st.st_size;
        remain_ = st.st_size;
        buf_ = (char *)map_;
        return true;
    }
};


class BufferedStreamCursor
    : public StreamCursor
{
    protected:
    std::vector<char> buf_;
    std::streamsize size_;
    virtual ssize_t readmore() = 0;

    BufferedStreamCursor()
        : buf_(131072)
        , size_(0)
    {
    }

    public:
    const char *buf()
    {
        return &buf_[0];
    }

    size_t size()
    {
        return size_;
    }

    virtual bool more(size_t shift=0)
    {
        assert(shift >= 0);
        assert(shift <= size_);
        memcpy(&buf_[0], (&buf_[0] + size_) - shift, shift);
        size_ = shift;

        ssize_t rc = readmore();
        if(rc == -1) {
            return false;
        }

        size_ += rc;
        return size_;
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
        return ::read(fd_, &buf_[0] + size_, buf_.size() - size_);
    }
};


struct CsvCell
{
    const char *ptr;
    size_t size;

    public:
    std::string as_str()
    {
        return std::string(ptr, size);
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
            if(value == cells[i].as_str()) {
                cell = &cells[i];
                return true;
            }
        }
        return false;
    }
};



template<typename... Args>
static int __attribute__((__always_inline__, target("sse4.2")))
strcspn16(const char *buf, Args... args)
{
    return _mm_cmpistri(
        (__m128i) (__v16qi) {args...},
        _mm_loadu_si128((__m128i *) buf),
        0
    );
}


class CsvReader
{
    public:
    StreamCursor &stream_;
    CsvCursor row_;

    bool
    try_parse()
        __attribute__((target("sse4.2")))
    {
        const char *p = p_;
        const char *cell_start;
        CsvCell *cell = row_.cells;

        row_.count = 0;

//__builtin_prefetch(buf + pos + 16);
//
#define DISPATCH0(state_) { \
    if(p >= endp_) \
        break; \
    goto state_; \
}

        if(p >= endp_) {
            DEBUG("pos exceeds size");
            return false;
        }

        DEBUG("remain = %lu", endp_ - p);
        DEBUG("ch = %d %c", (int) *p, *p);
        //DEBUG("rest = %.10s", *p);

        for(;;) {
            cell_start:
                if(*p == '\r') {
                    ++p;
                    DISPATCH0(cell_start)
                } else if(*p == '"') {
                    cell_start = ++p;
                    DISPATCH0(in_quoted_cell);
                } else {
                    cell_start = p;
                    DISPATCH0(in_unquoted_cell);
                }

            in_quoted_cell: {
                int rc = strcspn16(p, '"');
                if(rc) {
                    p += rc;
                    if(p >= endp_) {
                        break;
                    }

                    int rc = strcspn16(p, '"');
                    if(rc) {
                        p += rc;
                        DISPATCH0(in_quoted_cell);
                    } else {
                        ++p;
                        DISPATCH0(in_escape_or_end_of_cell);
                    }
                } else {
                    ++p;
                    DISPATCH0(in_escape_or_end_of_cell);
                }
            }

            in_unquoted_cell: {
                int rc = strcspn16(p, ',', '\r', '\n');
                if(rc) {
                    p += rc;
                    DISPATCH0(in_unquoted_cell);
                } else {
                    cell->ptr = cell_start;
                    cell->size = p - cell_start;
                    ++cell;
                    ++row_.count;

                    if(*p == '\n') {
                        p_ = p + 1;
                        return true;
                    }

                    ++p;
                    DISPATCH0(cell_start);
                }
            }

            in_escape_or_end_of_cell:
                if(*p == ',') {
                    cell->ptr = cell_start;
                    cell->size = p - cell_start - 1;
                    ++cell;
                    ++row_.count;

                    ++p;
                    DISPATCH0(cell_start);
                } else if(*p == '\n') {
                    cell->ptr = cell_start;
                    cell->size = p - cell_start - 1;
                    ++cell;
                    ++row_.count;

                    p_ = p + 1;
                    return true;
                } else {
                    ++p;
                    DISPATCH0(in_quoted_cell);
                }
        }

        DEBUG("error out");
        return false;
    }

    const char *endp_;
    const char *p_;

    public:

    bool refill()
    {
        size_t shift = (p_ <  endp_) ? (stream_.size() - (endp_ - p_)) : 0;
        DEBUG("endp_ = %#p  p_ = %#p  shift = %ld", endp_, p_, (long) shift)
        int rc = stream_.more(shift);
        p_ = stream_.buf();
        endp_ = p_ + stream_.size();
        DEBUG("refilled rc=%d p=%#p endp=%#p size=%lu",
              rc, p_, endp_, stream_.size())
        return rc;
    }

    bool
    read_row()
    {
        if(! try_parse()) {
            DEBUG("try_parse failed stream_.size()=%lu", stream_.size());
            if(! refill()) {
                DEBUG("refill failed");
                return false;
            }
            DEBUG("refill succeeded, size()=%lu", stream_.size());
            return try_parse();
        }
        return true;
    }

    CsvCursor &
    row()
    {
        return row_;
    }

    CsvReader(StreamCursor &stream)
        : stream_(stream)
        , p_(stream.buf())
        , endp_(stream.buf() + stream.size())
    {
    }
};
