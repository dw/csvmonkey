
import unittest
import os

import csvmonkey


class LengthTest(unittest.TestCase):
    def test_mmap_last_field(self):
        dirname = os.path.join(os.path.dirname(__file__), 'data')
    
        for mod_length in range(15):
            filename = os.path.join(dirname, f'length-{mod_length}.csv')
            for row in csvmonkey.from_path(filename, header=True):
                f = row["ResourceId"]
            self.assertEqual(f, 'x' * (mod_length + 1))
            

if __name__ == '__main__':
    unittest.main()
