# csvmonkey

This is a header-only vectorized, lazy-decoding, zero-copy CSV file parser. The
C++ version can process ~1.7GB/sec of raw input in a single thread, the Python
version is ~5x faster than `csv.reader` and ~13x faster than `csv.DictReader`,
while maintaining a similarly usable interface.

Requires a CPU supporting Intel SSE4.2 and a C++11 compiler that bundles
`smmintrin.h`.

It still requires a ton of work. For now it's mostly toy code


## Python Usage

TODO


## C++ Usage

1. Copy `csvmonkey.hpp` to your project and include it.
1. `CFLAGS=-msse4.2 -O3`
1. See `Makefile` for an example of producing a profile-guided build (worth an
   extra few %).
1. Instantiate `MappedFileCursor` (zero copy) or `FdStreamCursor` (buffered), attach it to a `CsvReader`.
1. Invoke `read_row()` and use `row().by_value()` to pick out `CsvCell` pointers for your desired rows.
1. Pump `read_row()` in a loop and use cell's `ptr()`, `size()`, `as_str()`, `equals()` and `as_double()` methods while `read_row()` returns true.

