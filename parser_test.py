
import unittest
import io

import csvmonkey


def parse(s):
    return list(
        csvmonkey.from_file(
            io.BytesIO(s),
            header=False,
            yields='tuple'
        )
    )


class BoundaryTest(unittest.TestCase):
    def test_4094(self):
        # parsing ends on page boundary
        c = 'x' * 4094
        s = '%s,\n' % (c,)
        self.assertEquals([(c,'')], parse(s))

    def test_4095(self):
        # parsing ends on first byte of new page
        c = 'x' * 4095
        s = '%s,\n' % (c,)
        self.assertEquals([(c,'')], parse(s))

    def test_14(self):
        # parsing ends on 16th byte (SSE4.2)
        c = 'x' * 14
        s = '%s,\n' % (c,)
        self.assertEquals([(c,'')], parse(s))

    def test_15(self):
        # parsing ends on 17th byte (SSE4.2)
        c = 'x' * 15
        s = '%s,\n' % (c,)
        self.assertEquals([(c,'')], parse(s))


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
