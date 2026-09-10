import subprocess,sys,tempfile,unittest
from pathlib import Path
HERE=Path(__file__).parent
sys.path.insert(0,str(HERE))
import watch_sao_life_snapshot as w
class WatchTests(unittest.TestCase):
 def test_bad_paths(self):
  with tempfile.TemporaryDirectory() as d:
   world=Path(d)/"world.json"; world.write_text("{}",encoding="utf-8")
   with self.assertRaises(ValueError): w.safe_paths(world,world)
   with self.assertRaises(ValueError): w.safe_paths(world,Path(d)/"x.json")
 def test_deadline_does_not_export(self):
  with tempfile.TemporaryDirectory() as d:
   world=Path(d)/"world.json"; world.write_text("{}",encoding="utf-8")
   out=Path(d)/"out"/"s.json"
   p=subprocess.run([sys.executable,str(HERE/"watch_sao_life_snapshot.py"),"--world",str(world),"--output",str(out),"--stop-at-utc","2000-01-01T00:00:00Z"],capture_output=True,text=True)
   self.assertEqual(p.returncode,0)
   self.assertFalse(out.exists())
 def test_failed_export_keeps_old(self):
  with tempfile.TemporaryDirectory() as d:
   out=Path(d)/"s.json"; out.write_text("old",encoding="utf-8")
   world=Path(d)/"formal"/"world.json"; world.parent.mkdir(); world.write_text("{}",encoding="utf-8")
   result=w.run_once(world,out)
   self.assertNotEqual(result.returncode,0)
   self.assertEqual(out.read_text(encoding="utf-8"),"old")
if __name__=="__main__": unittest.main()
