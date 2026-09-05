"""Import the frozen SocietyKit sources into their own native UE asset directory."""
import hashlib
import json
from pathlib import Path
import sys
import unreal as ue

ROOT=Path(__file__).resolve().parents[3]
KIT=ROOT/'Art/SocietyKit'
DEST='/Game/ThreeHearths/Generated/SocietyKit'
REPORT=KIT/'UE_Import_Report.json'
sys.path.insert(0,str(Path(__file__).resolve().parent))
from import_village_kit import import_one

def main():
    manifest=json.loads((KIT/'artifact-manifest.json').read_text(encoding='utf-8'))
    for artifact in manifest['artifacts']:
        source=KIT/artifact['file']
        assert source.stat().st_size==artifact['bytes']
        assert hashlib.sha256(source.read_bytes()).hexdigest()==artifact['sha256'],source.name
    specs=json.loads((KIT/'module-specs.json').read_text(encoding='utf-8'))
    layouts=json.loads((KIT/'example-layouts.json').read_text(encoding='utf-8'))
    sources=[(m['id'],KIT/'modules'/(m['id']+'.glb')) for m in specs['modules']]
    sources.extend(('example__'+e['id'],KIT/'examples'/(e['id']+'.glb')) for e in layouts['examples'])
    previous={}
    if REPORT.exists():
        prior=json.loads(REPORT.read_text(encoding='utf-8'))
        previous={row['id']:row for row in prior.get('resume_assets',[])+prior.get('assets',[])}
    report={'status':'running','engine':ue.SystemLibrary.get_engine_version(),
        'scope':'native art assets; functional economy and navigation remain separate',
        'assets':[],'resume_assets':list(previous.values())}
    try:
        for asset_id,source in sources:
            report['assets'].append(import_one(source,asset_id,previous,destination_root=DEST))
            REPORT.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
            ue.log('[SocietyKitImport] '+asset_id+' saved')
        report['status']='passed';report.pop('resume_assets',None)
    except Exception as exc:
        report.update(status='failed',error=str(exc));raise
    finally:
        REPORT.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')

if __name__=='__main__':main()
