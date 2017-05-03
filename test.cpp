
#include <chrono>

#include "csvninja.hpp"


int main(int argc, char **argv)
{
    chdir("/home/dmw/py-csvninja");

    const char *filename = "ram.csv";
    if(argc > 1) {
        filename = argv[1];
    }

#if 0
    int fd = open("ram.csv", O_RDONLY);
    assert(fd != -1);

    FdStreamCursor stream(fd);
    CsvReader reader(stream);
#else
    MappedFileCursor stream;
    assert(stream.open("ram.csv"));
    CsvReader reader(stream);
#endif

    int rows = 0;
    CsvCursor &row = reader.row();
    assert(reader.read_row());

    CsvCell *cost_cell;
    CsvCell *resource_id_cell;
    CsvCell *record_type_cell;

    assert(row.by_value("Cost", cost_cell));
    assert(row.by_value("ResourceId", resource_id_cell));
    assert(row.by_value("RecordType", record_type_cell));

    double total_cost = 0;
    auto start = std::chrono::high_resolution_clock::now();

    while(reader.read_row()) {
        if(DEBUGON && rows++ >= 160075) {
            ENABLE_DEBUG();
            for(int i = 0; i < row.count; i++) {
                CsvCell &cell = row.cells[i];
                printf("%d: %i: %.*s\n", rows, i, (int)cell.size, cell.ptr);
            }
        }

        if(1) {
            if(record_type_cell->equals("LineItem")) {
                total_cost += cost_cell->as_double();
            } else if(record_type_cell->equals("Rounding")) {
                total_cost += cost_cell->as_double();
            }
        } else {
            total_cost += 0.0;
        }
    }
    auto finish = std::chrono::high_resolution_clock::now();

    printf("%lf\n", total_cost);
    std::cout << (std::chrono::duration_cast<std::chrono::microseconds>(finish-start).count()) << " us\n";
    return 0;
}
