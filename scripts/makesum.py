
import hashlib
import csv

import csvmonkey

reader = csv.reader(open('ram.csv'))
h = hashlib.sha256()

for row in reader:
    for col in row:
        h.update(col)

assert h.hexdigest() == (
    "68187f51a11392551209d440710d835cdc167e2150eccb34e8cf9192bb8f9fc6"
)
