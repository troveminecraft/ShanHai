"""The shipped spawn worker must decode both persisted world heights strictly."""
import pathlib,struct,sys,tempfile,unittest
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[1]/'tools'))
from select_spawn import columns
from verify_water import decode

class SpawnColumnsTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.path=pathlib.Path(self.temp.name)/'chunk.bin'
    def tearDown(self):self.temp.cleanup()
    def chunk(self,height):return b'KW2\0'+bytes(256)+struct.pack('<HHH',1,height,2)*256
    def test_both_heights_and_last_cell(self):
        for height in (384,576):
            with self.subTest(height=height):
                self.path.write_bytes(self.chunk(height))
                result=columns(self.path,height)
                self.assertEqual(len(result),256);self.assertEqual(len(result[-1]),height)
                self.assertEqual(result[-1][-1],2)
                water=decode(self.path,height=height)
                self.assertEqual(water.shape,(16,16,height));self.assertEqual(int(water[-1,-1,-1]),2)
    def test_old_and_new_recipe_mismatch_rejected(self):
        for encoded,expected in ((384,576),(576,384)):
            self.path.write_bytes(self.chunk(encoded))
            for parser in (columns,lambda p,h:decode(p,height=h)):
                with self.subTest(encoded=encoded,expected=expected),self.assertRaises(ValueError):parser(self.path,expected)
    def test_mixed_height_and_truncated_runs_rejected(self):
        valid=self.chunk(576)
        broken=[valid[:260]+struct.pack('<HHH',1,384,2)+valid[266:],valid[:-1],valid+b'bad',
                valid[:260]+struct.pack('<HHH',1,0,2)+valid[266:]]
        for data in broken:
            self.path.write_bytes(data)
            for parser in (columns,lambda p,h:decode(p,height=h)):
                with self.subTest(size=len(data)),self.assertRaises(ValueError):parser(self.path,576)
    def test_legacy_height_remains_default(self):
        self.path.write_bytes(self.chunk(384))
        self.assertEqual(len(columns(self.path)[0]),384)
        self.assertEqual(decode(self.path).shape,(16,16,384))

if __name__=='__main__':unittest.main()
