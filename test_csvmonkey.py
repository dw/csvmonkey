
import unittest

import csvmonkey


EXAMPLE_FILE = """
c0,c1,c2,c3
0,1,2,3
a,b,c,d
"""


def make_reader(s, **kwargs):
    return csvmonkey.from_iter(iter([s]), **kwargs)


class ParseTest(unittest.TestCase):
    def test_bad_split(self):
        s = "2017-05-01T02:15:08.000Z 2 229340663981 eni-00589050 172.31.11.238 138.246.253.19 443 54503 6 1 44 1493604908 1493604966 ACCEPT OK\n"
        reader = make_reader(s, delimiter=' ', header=False)
        self.assertEquals(next(reader).astuple(), tuple(s.split()))



class RowTest(unittest.TestCase):
    def reader(self):
        return make_reader(EXAMPLE_FILE)

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
