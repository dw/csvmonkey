# csvmonkey

This is a vectorized, lazy-decoding zero-copy CSV file parser. The C++ version can process ~1.7GB/sec of raw input, the Python version is ~5x faster than `csv.reader` and ~13x faster than `csv.DictReader`, while maintaining a similarly usable interface.

It still requires a ton of work. For now it's mostly toy code
