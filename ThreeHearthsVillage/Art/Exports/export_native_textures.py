"""Export the eleven repository texture sources referenced by native FBXs."""
from pathlib import Path
import json
import unreal as ue

PROJECT=Path(__file__).resolve().parents[2]
OUT=PROJECT/'Saved/ThreeHearths/CombinedFbx/Textures'
OUT.mkdir(parents=True,exist_ok=True)
names={'Farmer_A','Gatherer_A','Miner_A','Woodcutter_A','Builder_A',
    'Axe_A','Basket_A','Crate_A','Hammer_A','Hoe_A','Pickaxe_A'}
registry=ue.AssetRegistryHelpers.get_asset_registry()
registry.search_all_assets(synchronous_search=True)
report=[]
for ad in registry.get_assets_by_path('/Game',recursive=True):
    name=str(ad.asset_name)
    if name not in names or str(ad.asset_class_path.asset_name)!='Texture2D':continue
    asset=ad.get_asset()
    task=ue.AssetExportTask()
    task.object=asset
    task.filename=str(OUT/(name+'.png'))
    task.exporter=ue.TextureExporterPNG()
    task.automated=True
    task.prompt=False
    task.replace_identical=True
    assert ue.Exporter.run_asset_export_task(task),name
    report.append({'asset':str(ad.package_name),'file':task.filename})
assert {Path(r['file']).stem for r in report}==names
(OUT/'texture-report.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
ue.log('FBX_TEXTURES_COMPLETE '+str(len(report)))
