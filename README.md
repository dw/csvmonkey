# csvmonkey

This is a header-only vectorized, lazy-decoding, zero-copy CSV file parser. The
C++ version can process ~1.7GB/sec of raw input in a single thread, the Python
version is ~5x faster than `csv.reader` and ~13x faster than `csv.DictReader`,
while maintaining a similarly usable interface.

Requires a CPU supporting Intel SSE4.2 and a C++11 compiler that bundles
`smmintrin.h`.

It still requires a ton of work. For now it's mostly toy code


## Python Usage

1. `pip install -e . # or similar`
1. `import csvmonkey`
1. `csvmonkey.from_path()` for a memory-mapped file, `csvmonkey.from_iter()` for any iterable object that yields lines or file chunks, e.g. `from_iter(file("ram.csv"))`.

By default a file header is expected and read away during construction. If your CSV lacks a header, specify `header=False`.

By default a magical `Row` object is yielded during iteration. This object is only a window into the currently parsed data, and will become invalid upon the next iteration. Row data can be accessed either by index or by key (if `header=True`) using:

```
for row in reader:
    row[20]  # by index
    row["UnBlendedCost"]  # by header value
```

Element access causes the relevant chunk of the row to be copied to the heap and returned as a Python string.

Rows may be converted to dicts via `row.asdict()` or tuples using
`row.astuple()`. If you want rows to be produced directly as dict or tuple,
pass `yields="tuple"` or `yields="dict"` keyword arguments.


## Python Benchmark

ram.csv is 614MiB with 1,540,093 records of 22 columns and approximately 418 bytes per record.

csvmonkey:

```
$ python -m timeit -n 1 -r 1 -s 'import csvmonkey' 'sum(float(row["UnBlendedCost"]) for row in csvmonkey.from_path("ram.csv"))'
1 loops, best of 1: 1.42 sec per loop
```

csv module's DictReader:

```
$ python -m timeit -n 1 -r 1 -s 'import csv' 'sum(float(row["UnBlendedCost"]) for row in csv.DictReader(file("ram.csv")))'
1 loops, best of 1: 25 sec per loop
```

Plain csv.reader with hard-coded column index:

```
$ python -m timeit -n 1 -r 1 -s 'import csv' 'r = csv.reader(file("ram.csv")); next(r); sum(float(row[20]) for row in r)'
1 loops, best of 1: 11.1 sec per loop
```

Why? Because csvmonkey internally generates no copies or heap allocations of
any kind during iteration. Strings are only copied to the heap when they are
accessed, which makes jobs like the above that touch few of the available
columns much faster than with the csv module.

Even if your job requires access to all the columns as a regular tuple, performance is still good:

```
$ python -m timeit -n 1 -r 1 -s 'import csvmonkey' 'sum(float(row[20]) for row in csvmonkey.from_path("ram.csv", yields="tuple"))'
1 loops, best of 1: 2.51 sec per loop
```

And still compelling even with plain dicts:

```
$ python -m timeit -n 1 -r 1 -s 'import csvmonkey' 'sum(float(row["UnBlendedCost"]) for row in csvmonkey.from_path("ram.csv", yields="dict"))'
1 loops, best of 1: 5.38 sec per loop
```


## C++ Usage

1. Copy `csvmonkey.hpp` to your project and include it.
1. `CFLAGS=-msse4.2 -O3`
1. See `Makefile` for an example of producing a profile-guided build (worth an
   extra few %).
1. Instantiate `MappedFileCursor` (zero copy) or `FdStreamCursor` (buffered), attach it to a `CsvReader`.
1. Invoke `read_row()` and use `row().by_value()` to pick out `CsvCell` pointers for your desired rows.
1. Pump `read_row()` in a loop and use cell's `ptr()`, `size()`, `as_str()`, `equals()` and `as_double()` methods while `read_row()` returns true.

