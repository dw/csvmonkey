# csvmonkey - patched to work under Cygwin

This is a header-only vectorized, lazy-decoding, zero-copy CSV file parser.
Given appropriate input data and hardware, the C++ version can tokenize ~1.9
GiB/sec of input in one thread. For a basic summing task, the Python version is
~5x faster than `csv.reader` and ~10x faster than `csv.DictReader`, while
maintaining a similarly usable interface.

**This still requires a ton of work. For now it's mostly toy code.**

Requires a CPU supporting Intel SSE4.2 and a C++11 compiler that bundles
`smmintrin.h`. For non-SSE4.2 machines, a reasonable fallback implementation is
also provided.

As of writing, csvmonkey comfortably leads <a
href="https://bitbucket.org/ewanhiggs/csv-game">Ewan Higg's csv-game</a>
microbenchmark of 24 CSV parsers.


## How It Works

* **Vectorized**: scanning for values that change parser state is done using
  Intel SSE 4.2 PCMPISTRI instruction. PCMPISTRI can locate the first occurence
  of up to four values within a 16 byte vector, allowing searching 16 input
  bytes for end of line, escape, quote, or field separators in one instruction.

* **Zero Copy**: the user supplies the parser's input buffer. The output is an
  array of column offsets within a row, each flagged to indicate whether an
  escape character was detected. The highest throughput is achieved in
  combination with memory-mapped files, where none of the OS, application or
  parser make any bulk copies.

* **Lazy Decoding**: input is not copied or unquoted until requested. Since a
  flag indicates the presence of escapes, a fast path can avoid any bytewise
  decode in the usual case where no escape is present. Due to lazy decoding,
  csvmonkey is extremely effective at tasks that scan only a subset of data,
  for example one or two columns from a large many-columned CSV. This use case
  is the original motivation for the design.

* **Header Only**: the parser has no third-party dependencies, just some
  templates defined in ``csvmonkey.hpp``.


## Python Usage

You can install the library through pip:

```
pip install csvmonkey
```

If this fails on ubuntu, first install `clang`:

```
sudo apt-get install clang
```

Then run:

```
CC=clang pip install csvmonkey
```

You can also install the library locally by cloning this repo and running:

```
pip install -e .
```

Then you can use it likewise:

1. `import csvmonkey`
2. `csvmonkey.from_path()` for a memory-mapped file, `csvmonkey.from_file()`
   for any file-like object with a `read()` method, or `csvmonkey.from_iter()`
   for any iterable object that yields lines or file chunks, e.g.
   `from_iter(open("ram.csv"))`.

By default a magical `Row` object is yielded during iteration. This object is
only a window into the currently parsed data, and becomes invalid upon the next
iteration. Row data is accessed either by index or by key (if `header=True`)
using:

```
for row in csvmonkey.from_path("ram.csv", header=True):
    row[20]  # by index
    row["UnBlendedCost"]  # by header value
```

If your CSV contains a header, specify `header=True` to read it during
construction. If the CSV lacks a header but dict-like behaviour is desired,
pass a header explicitly as `header=("a", "b", "c", "d")`.

Element access causes the relevant chunk of the row to be copied to the heap
and returned as a Python string.

Rows may be converted to dicts via `row.asdict()`, tuples via
`row.astuple()` or lists via `row.aslist()`. If you want rows to be produced
directly in concrete form, pass `yields="list"`, `yields="tuple"`,
`yields="dict"` keyword arguments.


### Unicode

Unicode is supported for character sets where delimiters and line endings are
represented by one byte. To configure Unicode, pass an ``encoding`` parameter,
and optionally an ``errors`` parameter.

* "bytes": Return bytes (default on Python 2)
* "utf-8": Decode as UTF-8 (default on Python 3)
* "ascii": Decode as ASCII
* "latin1": Decode as LATIN1
* "locale": Decode according to the active C locale
* "...": Decode according some codec "..." known to Python

Where possible, prefer exact spelling and case matching one of the above
encodings, to ensure an associated fast path is used.


## Python Benchmark

ram.csv is 614MiB with 1,540,093 records of 22 columns and approximately 418
bytes per record. An anonymized version is checked into LFS as
``tests/data/anon-ram.csv.zstd``.

Python 2.7 Sum: convert to float and sum single column:

| Mode                     | Rate       | Ratio | i7-6700HQ | Xeon E5530 | Core i5-2435M |
|--------------------------|------------|-------|-----------|------------|---------------|
| csvmonkey lazy decode    | 1098 MiB/s | -     | 0.559s    | 0.9s       | 1.29s         |
| csvmonkey yields="tuple" | 642 MiB/s  | 1.7x  | 0.956s    | 1.87s      | 2.17s         |
| csvmonkey yields="dict"  | 281 MiB/s  | 3.8x  | 2.18s     | 4.57s      | 5.04s         |
| csv.reader               | 223 MiB/s  | 4.9x  | 2.75s     | 5.88s      | 11.1s         |
| csv.DictReader           | 85 MiB/s   | 12.7x | 7.15s     | 16.3s      | 25.0s         |

Python 2.7 No-op: Iterate complete file, no other processing:

| Mode                     | Rate       | Ratio | i7-6700HQ | Xeon E5530 |
|--------------------------|------------|-------|-----------|------------|
| csvmonkey lazy decode    | 1906 MiB/s | -     | 0.322s    | 0.444s     |
| csvmonkey yields="tuple" | 831 MiB/s  | 2.3x  | 0.738s    | 1.4s       |
| csvmonkey yields="dict"  | 318 MiB/s  | 6.0x  | 1.93s     | 4.26s      |
| csv.reader               | 248 MiB/s  | 7.6x  | 2.47s     | 5.31s      |
| csv.DictReader           | 92 MiB/s   | 20.5x | 6.62s     | 15.2s      |

Python 3.6 No-op: Iterate complete file, includes charset decoding

| Mode                                        | Rate       | Ratio      | i7-6700HQ  |
|---------------------------------------------|------------|------------|------------|
| csvmonkey lazy decode                       | 1906 MiB/s | -          | 0.322s     |
| csvmonkey yields="tuple", encoding="bytes"  | 833 MiB/s  | 2.3x       | 0.737s     |
| csvmonkey yields="tuple", encoding="latin1" | 579 MiB/s  | 3.3x       | 1.06s      |
| csvmonkey yields="tuple"                    | 495 MiB/s  | 3.8x       | 1.24s      |
| csvmonkey yields="dict"                     | 235 MiB/s  | 8.1x       | 2.61s      |
| csv.reader                                  | 121 MiB/s  | 15.7x      | 5.07s      |
| csv.DictReader                              | 55 MiB/s   | 34.4x      | 11.1s      |


### Command lines

Sum:

```
python -mtimeit -n1 -r3 -s 'import csvmonkey' 'sum(float(row["UnBlendedCost"]) for row in csvmonkey.from_path("ram.csv", header=True))'
python -mtimeit -n1 -r3 -s 'import csvmonkey' 'sum(float(row[20]) for row in csvmonkey.from_path("ram.csv", header=True, yields="tuple"))'
python -mtimeit -n1 -r3 -s 'import csvmonkey' 'sum(float(row["UnBlendedCost"]) for row in csvmonkey.from_path("ram.csv", header=True, yields="dict"))'
python -mtimeit -n1 -r3 -s 'import csv' 'r = csv.reader(open("ram.csv")); next(r); sum(float(row[20]) for row in r)'
python -mtimeit -n1 -r3 -s 'import csv' 'sum(float(row["UnBlendedCost"]) for row in csv.DictReader(open("ram.csv")))'
```

No-op:

```
python -mtimeit -n1 -r3 -s 'import csvmonkey' 'all(csvmonkey.from_path("ram.csv", header=True))'
python -mtimeit -n1 -r3 -s 'import csvmonkey' 'all(csvmonkey.from_path("ram.csv", header=True, yields="tuple"))'
python -mtimeit -n1 -r3 -s 'import csvmonkey' 'all(csvmonkey.from_path("ram.csv", header=True, yields="dict"))'
python -mtimeit -n1 -r3 -s 'import csv' 'all(csv.reader(open("ram.csv")))'
python -mtimeit -n1 -r3 -s 'import csv' 'all(csv.DictReader(open("ram.csv")))'
```


## C++ Usage

1. Copy `csvmonkey.hpp` to your project and include it.
1. `CFLAGS=-msse4.2 -O3`
1. See `Makefile` for an example of producing a profile-guided build (worth an
   extra few %).
1. Instantiate `MappedFileCursor` (zero copy) or `FdStreamCursor` (buffered), attach it to a `CsvReader`.
1. Invoke `read_row()` and use `row().by_value()` to pick out `CsvCell` pointers for your desired columns.
1. Pump `read_row()` in a loop and use cell's `ptr()`, `size()`, `as_str()`, `equals()` and `as_double()` methods while `read_row()` returns true.


# TODO

* COW pointer interface to `as_str()`.
* ~~Finish Python 3 support~~
* ~~Ensure Python ReaderObject is always 16-byte aligned~~
* Fix handling of last row when it:
    * lacks newline, or
    * is truncated after final quote, or
    * is truncated within a quote, or
    * is truncated within an escape
* Restartable: fix quadratic behaviour when `StreamCursor` yields lines and CSV
  rows span lines
* ~~Python `from_file()` that uses `read()` in preference to `__iter__()`.~~
* ~~Fix CRLF / LFCR handling.~~
* ~~`StreamCursor` error / exception propagation.~~
* ~~Remove hard 256 column limit & fix crash if it's exceeded.~~
* ~~Ensure non-SSE fallback return codes match SSE when not found.~~
* ~~Map single zero page after file pages in MappedFileCursor~~
* ~~Add trailing 16 NUL bytes to BufferedStreamCursor~~
* ~~Remove hard-coded page size~~
* ~~(Single byte separator) Unicode support.~~
* (Multi byte separator) Unicode support.
