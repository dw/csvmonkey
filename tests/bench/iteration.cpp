#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <chrono>

#include "csvmonkey.hpp"

using csvmonkey::CsvCell;
using csvmonkey::CsvCursor;
using csvmonkey::CsvReader;
using csvmonkey::MappedFileCursor;
using std::chrono::duration_cast;
using std::chrono::high_resolution_clock;
using std::chrono::microseconds;


static void
die(const char *msg)
{
    fprintf(stderr, "%s\n", msg);
    exit(1);
}


static int
go(const char *path)
{
    MappedFileCursor stream;
    CsvReader<MappedFileCursor> reader(stream);

    stream.open(path);
    CsvCursor &row = reader.row();
    if(! reader.read_row()) {
        die("Cannot read header row");
    }

    CsvCell *cost_cell;
    if((! row.by_value("Cost", cost_cell)) &&
       (! row.by_value("UnBlendedCost", cost_cell))) {
        die("Cannot find Cost column");
    }

    CsvCell *resource_id_cell;
    if(! row.by_value("ResourceId", resource_id_cell)) {
        die("Cannot find ResourceId column");
    }

    CsvCell *record_type_cell;
    if(! row.by_value("RecordType", record_type_cell)) {
        die("Cannot find RecordType column");
    }

    auto now = [&] { return high_resolution_clock::now(); };
    double total = 0.0;
    auto start = now();

    while(reader.read_row()) {
        if(0) {
            if(record_type_cell->equals("LineItem")) {
                total += cost_cell->as_double();
            } else if(record_type_cell->equals("Rounding")) {
                total += cost_cell->as_double();
            }
        }
    }
    auto finish = now();

    printf("Total cost: %lf\n", total);
    auto usec = duration_cast<microseconds>(finish - start).count();

    struct stat st;
    stat(path, &st);

    std::cout << usec << " us\n";
    std::cout << (st.st_size / usec) << " bytes/us\n";
    std::cout << (
        (1e6 / (1024.0 * 1048576.0)) * (double) (st.st_size / usec) 
    ) << " GiB/s\n";
    return 0;
}


int main(int argc, char **argv)
{
    const char *path = "ram.csv";
    if(argc > 1) {
        path = argv[1];
    }
    for(int i = 0 ; i < 5; i++) {
        go(path);
    }
}
