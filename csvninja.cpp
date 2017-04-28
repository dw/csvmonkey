
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <vector>
#include <fstream>
#include <cassert>
#include <iostream>


#ifdef NDEBUG
#   define DEBUG(x...) {}
#   define ENABLE_DEBUG() {}
#   define DEBUGON 0
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



class FdStreamCursor
    : public StreamCursor
{
    int fd_;
    char buf_[131072];
    std::streamsize size_;

    public:
    FdStreamCursor(int fd)
        : fd_(fd)
        , size_(0)
    {
    }

    const char *buf()
    {
        return buf_;
    }

    size_t size()
    {
        return size_;
    }

    virtual bool more(int shift=0)
    {
        assert(shift >= 0);
        assert(shift == 0 || shift < size_);
        memcpy(buf_, (buf_ + sizeof buf_) - shift, shift);
        size_ = shift;

        ssize_t rc = read(fd_, buf_ + shift, sizeof buf_ - shift);
        if(rc == -1) {
            return false;
        }

        size_ += rc;
        return size_;
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

    double as_double()
    {
        return 0.0;
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
        size_t cell_start = 0;
        int col = 0;

        auto buf = stream_.buf();
        auto size = stream_.size();

        void *state = &&cell_start;

#define DISPATCH() { \
    if(++pos >= size) \
        break; \
    ch = buf[pos]; \
    goto *state; \
}

        size_t pos = row_start_;
        if(pos >= size) {
            DEBUG("pos exceeds size");
            return false;
        }

        char ch = buf[pos];
        DEBUG("row start = %d", row_start_);
        DEBUG("size = %d", size);
        DEBUG("ch = %d %c", ch, ch);
        DEBUG("rest = %.10s", buf + pos);

        for(;;) {
            cell_start:
                if(ch == '\r') {
                } else if(ch == '"') {
                    state = &&in_quoted_cell;
                    cell_start = pos + 1;
                } else {
                    state = &&in_unquoted_cell;
                    cell_start = pos;
                }
                DISPATCH()

            in_quoted_cell:
                if(ch == '"') {
                    state = &&in_escape_or_end_of_cell;
                }
                DISPATCH()

            in_unquoted_cell:
                if(ch == ',' || ch == '\r' || ch == '\n') {
                    CsvCell &cell = row_.cells[col];
                    const char *p = &buf[cell_start];
                    state = &&cell_start;
                }
                DISPATCH()

            in_escape_or_end_of_cell:
                if(ch == ',' || ch == '\n') {
                    state = &&cell_start;
                    CsvCell &cell = row_.cells[col++];
                    row_.count = col;
                    cell.ptr = &buf[cell_start];
                    cell.size = pos - cell_start - 1;

                    if(ch == '\n') {
                        row_start_ = pos + 1;
                        return true;
                    }
                } else {
                    state = &&in_quoted_cell;
                }
                DISPATCH()
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


int main()
{
    chdir("/Users/dmw");

    int fd = open("ram.csv", O_RDONLY);
    assert(fd != -1);

    FdStreamCursor stream(fd);
    CsvReader reader(stream);
    int rows = 0;

    CsvCursor &row = reader.row();
    assert(reader.read_row());

    CsvCell *cost_cell;
    CsvCell *resource_id_cell;

    assert(row.by_value("Cost", cost_cell));
    assert(row.by_value("ResourceId", resource_id_cell));

    double total_cost;

    while(reader.read_row()) {
        if(DEBUGON && rows++ >= 160075) {
            ENABLE_DEBUG();
            for(int i = 0; i < row.count; i++) {
                auto cell = row.cells[i];
                printf("%d: %i: %.*s\n", rows, i, cell.size, cell.ptr);
            }
        }

        total_cost += cost_cell->as_double();
    }

    close(fd);
    return 0;
}
