#include <cstdio>

#include "../csvmonkey.hpp"
#include "../third_party/picosha2.h"


using namespace csvmonkey;


int main(int argc, char **argv)
{
    const char *path = "ram.csv";
    if(argc > 1) {
        path = argv[1];
    }

    MappedFileCursor stream;
    stream.open(path);
    CsvReader<MappedFileCursor> reader(stream);
    CsvCursor &row = reader.row();

    picosha2::hash256_one_by_one hasher;
    while(reader.read_row()) {
        for(size_t i = 0; i < row.count; i++) {
            CsvCell &cell = row.cells[i];
            std::string s = cell.as_str();
            hasher.process(s.begin(), s.end());
        }
    }

    hasher.finish();
    std::cout << picosha2::get_hash_hex_string(hasher) << "\n";
    return 0;
}
