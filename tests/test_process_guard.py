import json, os, pathlib, subprocess, sys, tempfile, time, unittest
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[1]/'tools'))
from process_guard import run_checked
from select_spawn import NativeQuery

class ProcessGuardTest(unittest.TestCase):
 def test_failure_detail(self):
  with self.assertRaisesRegex(RuntimeError,'bad recipe'):
   run_checked([sys.executable,'-c','import sys;print("bad recipe",file=sys.stderr);sys.exit(4)'],stage='fixture')
 def test_missing_executable(self):
  with self.assertRaisesRegex(RuntimeError,'cannot start'):
   run_checked(['shanhai-missing-test-executable-012'],timeout=1)
 def test_large_stderr_does_not_block(self):
  result=run_checked([sys.executable,'-c','import sys;sys.stderr.write("x"*200000);print("ok")'],capture_output=True,timeout=5)
  self.assertEqual(result.stdout.strip(),'ok');self.assertLessEqual(len(result.stderr),8192)
 def test_timeout_stops_descendant(self):
  with tempfile.TemporaryDirectory(dir=pathlib.Path.cwd()) as folder:
   marker=pathlib.Path(folder)/'child-finished'
   child=f'import time,pathlib;time.sleep(2);pathlib.Path({str(marker)!r}).write_text("alive")'
   parent='import subprocess,sys,time;subprocess.Popen([sys.executable,"-c",'+repr(child)+']);time.sleep(30)'
   with self.assertRaisesRegex(RuntimeError,'timed out'):
    run_checked([sys.executable,'-c',parent],timeout=.6)
   time.sleep(2)
   self.assertFalse(marker.exists(),'Owned descendant survived timeout')
 def test_stuck_spawn_reply(self):
  with tempfile.TemporaryDirectory(dir=pathlib.Path.cwd()) as folder:
   script=pathlib.Path(folder)/'worker.py';script.write_text('import time;time.sleep(30)')
   with NativeQuery(sys.executable,script,timeout=.2,total_timeout=1) as worker:
    with self.assertRaisesRegex(RuntimeError,'timed out'):worker([1])
    self.assertIsNotNone(worker.process.poll())
 def test_invalid_reply(self):
  with tempfile.TemporaryDirectory(dir=pathlib.Path.cwd()) as folder:
   script=pathlib.Path(folder)/'worker.py';script.write_text('import sys\nfor line in sys.stdin:print("bad json",flush=True)\n')
   with NativeQuery(sys.executable,script,timeout=2) as worker:
    with self.assertRaisesRegex(RuntimeError,'Terrain worker failed'):worker([1])
if __name__=='__main__':unittest.main()
