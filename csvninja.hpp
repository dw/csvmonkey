
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


typedef char v16qi __attribute__ ((__vector_size__ (16)));


#ifdef NDEBUG
#   define DEBUG(x...) {}
#   define ENABLE_DEBUG() {}
#   define DEBUGON 0
#   undef assert
#   define assert(x) (x)
#else
static int debug;
#   define DEBUG(x, y...) if(debug) printf(x "\n", #y);
#   define ENABLE_DEBUG() { debug = 1; }
#   define DEBUGON 1
#endif

class CsvReader;


class StreamCursor
{
    public:
    virtual const char *buf() = 0;
    virtual size_t size() = 0;
    virtual bool more(int shift=0) = 0;
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

    virtual bool more(int shift=0)
    {
        if(shift) {
            if(shift > remain_) {
                shift = remain_;
            }

            buf_ += shift;
            remain_ -= shift;
        } else {
            buf_ = 0;
            remain_ = 0;
        }
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

    virtual bool more(int shift=0)
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
        return strtod(ptr, NULL);
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


class CsvReader
{
    public:
    StreamCursor &stream_;
    size_t row_start_;
    CsvCursor row_;

    bool
    try_parse()
    {
        int col = 0;

        const char *buf = stream_.buf();
        size_t size = stream_.size();
        const char *startp = buf;
        const char *endp = buf + size;
        const char *cell_start = 0;

//__builtin_prefetch(buf + pos + 16);
//
#define DISPATCH0(state_) { \
    if(buf > endp) \
        break; \
    goto state_; \
}

        buf += row_start_;
        if(buf > endp) {
            DEBUG("pos exceeds size");
            return false;
        }

        DEBUG("row start = %d", row_start_);
        DEBUG("size = %d", size);
        DEBUG("ch = %d %c", *buf, *buf);
        DEBUG("rest = %.10s", buf);

        for(;;) {
            cell_start:
                if(*buf == '\r') {
                    ++buf;
                    DISPATCH0(cell_start)
                } else if(*buf == '"') {
                    cell_start = ++buf;
                    DISPATCH0(in_quoted_cell);
                } else {
                    cell_start = buf++;
                    DISPATCH0(in_unquoted_cell);
                }

            in_quoted_cell: {
                v16qi vtmp = __builtin_ia32_loaddqu(buf);
                int rc = __builtin_ia32_pcmpistri128((v16qi){'"'}, vtmp, 0);
                if(rc) {
                    buf += rc;
                    if(buf > endp) {
                        break;
                    }

                    v16qi vtmp = __builtin_ia32_loaddqu(buf);
                    int rc = __builtin_ia32_pcmpistri128((v16qi){'"'}, vtmp, 0);
                    if(rc) {
                        buf += rc;
                        DISPATCH0(in_quoted_cell);
                    } else {
                        ++buf;
                        DISPATCH0(in_escape_or_end_of_cell);
                    }
                } else {
                    ++buf;
                    DISPATCH0(in_escape_or_end_of_cell);
                }
            }

            in_unquoted_cell:
                if(*buf == ',' || *buf == '\r' || *buf == '\n') {
                    //state = &&cell_start;
                }
                ++buf;
                DISPATCH0(in_unquoted_cell)

            in_escape_or_end_of_cell:
                if(*buf == ',' || *buf == '\n') {
                    CsvCell &cell = row_.cells[col++];
                    row_.count = col;
                    cell.ptr = cell_start;
                    cell.size = buf - cell_start - 1;

                    if(*buf == '\n') {
                        row_start_ = (buf - startp) + 1;
                        return true;
                    }

                    ++buf;
                    DISPATCH0(cell_start);
                } else {
                    ++buf;
                    DISPATCH0(in_quoted_cell);
                }
        }

        return false;
    }

    public:

    bool
    read_row()
    {
        if(! try_parse()) {
            DEBUG("try_parse failed row_start_=%d stream_.size()=%d", row_start_, stream_.size());
            DEBUG("shift = %d", stream_.size() - row_start_);
            int rc = stream_.more(stream_.size() - row_start_);
            row_start_ = 0;
            if(! rc) {
                return false;
            }
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
        , row_start_(0)
    {
    }
};
