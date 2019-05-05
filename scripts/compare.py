
try:
    from itertools import izip
except ImportError:
    izip = zip

import csv
import csvmonkey


icsv = csv.reader(open('ram.csv'))
next(icsv)

imonkey = csvmonkey.from_path('ram.csv', yields='list', header=True)

for r1, r2 in izip(icsv, imonkey):
    print(r1)
    print(r2)
    assert r1 == r2
