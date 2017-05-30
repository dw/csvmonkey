
import unittest
import io

import csvmonkey


def parse(s):
    sio = io.BytesIO(s)
    it = csvmonkey.from_file(sio, yields='tuple')
    return list(it)


class BoundaryTest(unittest.TestCase):
    def test_col16(self):
        c = 'x' * 4094
        s = '%s,\n' % (c,)
        self.assertEquals([(c,)], parse(s))


class Test(unittest.TestCase):
    def test_empty0(self):
        self.assertEquals([], parse(''))

    def test_empty1(self):
        self.assertEquals([], parse('\n'))

    def test_empty2(self):
        self.assertEquals([], parse('\r\n'))

    def test_empty2(self):
        self.assertEquals([], parse('\r\n\n\r\r\r\n'))

    def test_unquoted_noeol(self):
        self.skipTest('failing')
        self.assertEquals([('a', 'b')], parse('a,b'))

    def test_unquoted_noeol2(self):
        self.skipTest('failing')
        self.assertEquals([('a', 'b'), ('c', 'd')], parse('a,b\n\rc,d'))

    def test_unquoted(self):
        self.assertEquals([('a', 'b')], parse('a,b\n'))

    def test_quoted_empty(self):
        self.assertEquals([('',)], parse('""\n'))

    def test_quoted_empty_unquoted(self):
        self.skipTest('failing')
        self.assertEquals([('', '')], parse('"",\n'))

    def test_unquoted_empty(self):
        self.skipTest('failing')
        self.assertEquals([('', '')], parse(',\n'))

if __name__ == '__main__':
    unittest.main()
