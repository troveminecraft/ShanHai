import pathlib,sys,tempfile,unittest
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[1]/'tools'))
from select_spawn import NativeQuery

class SpawnWorkerTest(unittest.TestCase):
 def test_queries_reuse_one_process(self):
  with tempfile.TemporaryDirectory(dir=pathlib.Path.cwd()) as temporary:
   script=pathlib.Path(temporary)/'worker.py'
   script.write_text('import sys,json\nfor i,line in enumerate(sys.stdin):\n print(json.dumps({"index":i,"request":json.loads(line)}),flush=True)\n')
   with NativeQuery(sys.executable,script) as worker:
    self.assertEqual(worker([1,2]),[dict(index=0,request=1),dict(index=1,request=2)])
    self.assertEqual(worker([3]),[dict(index=2,request=3)])
   self.assertEqual(worker.process.poll(),0)
 def test_exit_is_reported(self):
  with tempfile.TemporaryDirectory(dir=pathlib.Path.cwd()) as temporary:
   script=pathlib.Path(temporary)/'worker.py';script.write_text('import sys\nprint("bad recipe",file=sys.stderr)\nsys.exit(3)\n')
   with NativeQuery(sys.executable,script) as worker:
    with self.assertRaisesRegex(RuntimeError,'bad recipe'):worker([1])
if __name__=='__main__':unittest.main()
