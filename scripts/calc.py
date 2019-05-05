#!/usr/bin/env python

import os.path
import sys

print (os.path.getsize('ram.csv') / 1048576.0) / (float(sys.argv[1]) / 1e6), 'GiB/s'
