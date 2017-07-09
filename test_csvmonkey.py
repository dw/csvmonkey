
import unittest

import csvmonkey


EXAMPLE_FILE = """
c0,c1,c2,c3
0,1,2,3
a,b,c,d
"""


class RowTest(unittest.TestCase):
    def reader(self):
        return csvmonkey.from_iter(iter([EXAMPLE_FILE]))

    def test_getitem_numeric_positive(self):
        reader = self.reader()
        row = next(reader)
        self.assertEquals("0", row[0])
        self.assertRaises(IndexError, lambda: row[5])

    def test_getitem_numeric_negative(self):
        reader = self.reader()
        row = next(reader)
        self.assertEquals("3", row[-1])
        self.assertRaises(IndexError, lambda: row[-5])

    def test_getitem_key(self):
        reader = self.reader()
        row = next(reader)
        self.assertEquals("0", row["c0"])
        self.assertRaises(KeyError, lambda: row["missing"])




if __name__ == '__main__':
    unittest.main()
