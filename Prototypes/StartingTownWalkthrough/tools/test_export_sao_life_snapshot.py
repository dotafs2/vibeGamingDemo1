import copy, json, subprocess, sys, tempfile, unittest
from pathlib import Path
HERE=Path(__file__).parent
sys.path.insert(0,str(HERE))
import export_sao_life_snapshot as exporter
WORLD=Path(__file__).parents[3]/"ThreeHearthsVillage/Saved/ThreeHearths/AincradLevel0/world.json"
class ExportTests(unittest.TestCase):
  def run_export(self, world, out):
    return subprocess.run([sys.executable,str(HERE/"export_sao_life_snapshot.py"),"--world",str(world),"--output",str(out)],capture_output=True,text=True)
  def test_whitelist_identity_and_private_fields(self):
    with tempfile.TemporaryDirectory() as d:
      out=Path(d)/"s.json"; p=self.run_export(WORLD,out); self.assertEqual(p.returncode,0,p.stderr)
      data=json.loads(out.read_text(encoding="utf-8")); self.assertEqual(data["world_id"],"F4390752-4A07-7DE7-FACD-32BAC6F72C54"); self.assertEqual(len(data["life"]["residents"]),13); self.assertEqual(sum(r["active"] for r in data["life"]["residents"]),3)
      raw=json.dumps(data,ensure_ascii=False).lower(); self.assertNotIn("last_result_raw",raw); self.assertNotIn("api_config",raw); self.assertNotIn("pending_budget",raw)
      for r in data["life"]["residents"]: self.assertLessEqual(len(r["capability_requests"]),16)
  def test_public_resource_fixture_and_conservation(self):
    src=json.loads(WORLD.read_text(encoding="utf-8-sig")); src["foraging"]={"schema_version":1,"source_id":"starter_commons_berry_patch","source":"developer_ecosystem_bootstrap","stock":2,"capacity":3,"initial_stock":3,"produced_total":0,"harvested_total":1,"growth_remainder_seconds":12.5,"installed_at_life_seq":25,"work_position_cm":[0,465000,92],"visual_position_cm":[130,465000,0]}
    out=exporter.export_state(src); self.assertEqual(out["life"]["public_food_resource"]["stock"],2)
    src["foraging"]["stock"]=3
    with self.assertRaises(ValueError): exporter.export_state(src)
  def test_public_resource_absent_is_compatible(self):
    src=json.loads(WORLD.read_text(encoding="utf-8-sig")); src.pop("foraging",None); self.assertIsNone(exporter.export_state(src)["life"]["public_food_resource"])
  def test_bad_input_preserves_old_output(self):
    with tempfile.TemporaryDirectory() as d:
      out=Path(d)/"s.json"; out.write_text("old",encoding="utf-8"); bad=Path(d)/"bad.json"; bad.write_text(json.dumps({"world_id":"bad"}),encoding="utf-8"); p=self.run_export(bad,out); self.assertNotEqual(p.returncode,0); self.assertEqual(out.read_text(encoding="utf-8"),"old")
if __name__=="__main__": unittest.main()
