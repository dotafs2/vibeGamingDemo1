"""Import the validated GoodsKit GLBs into native Unreal assets.

Run with UnrealEditor-Cmd <project> -run=pythonscript -script=<this file>.
This creates renderable assets only; it does not change inventory or recipes.
"""
import hashlib
import json
from pathlib import Path
import sys

import unreal as ue

ROOT = Path(__file__).resolve().parents[3]
KIT = ROOT / 'Art/GoodsKit'
DEST = '/Game/ThreeHearths/Generated/GoodsKit'
REPORT = KIT / 'UE_Import_Report.json'
sys.path.insert(0, str(Path(__file__).resolve().parent))
from import_village_kit import import_one


def main():
    manifest = json.loads((KIT / 'artifact-manifest.json').read_text(encoding='utf-8'))
    artifacts = {row['file']: row for row in manifest['artifacts']}
    specs = json.loads((KIT / 'module-specs.json').read_text(encoding='utf-8'))
    sources = [(row['id'], KIT / 'modules' / (row['id'] + '.glb')) for row in specs['modules']]
    for _, source in sources:
        relative = source.relative_to(KIT).as_posix()
        artifact = artifacts[relative]
        assert source.stat().st_size == artifact['bytes'], relative
        assert hashlib.sha256(source.read_bytes()).hexdigest() == artifact['sha256'], relative

    previous = {}
    if REPORT.exists():
        prior = json.loads(REPORT.read_text(encoding='utf-8'))
        previous = {row['id']: row for row in prior.get('resume_assets', []) + prior.get('assets', [])}
    report = {
        'status': 'running',
        'engine': ue.SystemLibrary.get_engine_version(),
        'scope': 'native renderable goods; inventory, recipes and carry animation remain separate',
        'assets': [],
        'resume_assets': list(previous.values()),
    }
    try:
        for asset_id, source in sources:
            report['assets'].append(import_one(source, asset_id, previous, destination_root=DEST))
            REPORT.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
            ue.log('[GoodsKitImport] ' + asset_id + ' saved')
        report['status'] = 'passed'
        report.pop('resume_assets', None)
    except Exception as exc:
        report.update(status='failed', error=str(exc))
        raise
    finally:
        REPORT.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')


if __name__ == '__main__':
    main()
