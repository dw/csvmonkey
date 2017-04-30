
#include "csvninja.hpp"


int main()
{
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
                CsvCell &cell = row.cells[i];
                printf("%d: %i: %.*s\n", rows, i, cell.size, cell.ptr);
            }
        }

        total_cost += cost_cell->as_double();
    }

    close(fd);
    return 0;
}
