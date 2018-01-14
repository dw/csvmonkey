#!/usr/bin/env python

import argparse
import csv
import operator
import sys
from itertools import chain

import csvmonkey


parser = argparse.ArgumentParser(description='Process some integers.')
parser.add_argument('paths', nargs='*')
parser.add_argument('-H', '--no-header', action='store_true', default=False)
parser.add_argument('-f', '--fields', default='-')

args = parser.parse_args()

if args.fields == '-':
    slices = [slice(None)]
else:
    slices = []
    for bit in args.fields.split(','):
        left, sep, right = bit.partition('-')
        if sep:
            slices.append(slice(int(left) - 1, int(right)))
        else:
            slices.append(slice(int(left) -1, int(left)))

ig = operator.itemgetter(*slices)


writer = csv.writer(sys.stdout, quoting=csv.QUOTE_ALL)

readers = [
    csvmonkey.from_path(path, header=not args.no_header, yields='tuple')
    for path in args.paths
]
if not readers:
    it = iter(sys.stdin.readline, '')
    readers.append(csvmonkey.from_iter(it, header=not args.no_header, yields='tuple'))


for reader in readers:
    for row in reader:
        l = []
        for sl in slices:
            l.extend(row[sl])
        writer.writerow(l)
