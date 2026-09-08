"""Collect compact, local evidence from the organic village's actual checkpoints."""
import json
import shutil
from pathlib import Path

ART = Path(__file__).resolve().parents[1]
PROJECT = ART.parents[1]
SAVED = PROJECT / 'Saved' / 'ThreeHearths' / 'OrganicReview'
OUT = ART / 'RuntimeReview'


def read(path):
    data = path.read_bytes()
    return json.loads(data.decode('utf-16' if data[:2] in (b'\xff\xfe', b'\xfe\xff') else 'utf-8-sig'))


def world(name):
    wrapper = read(SAVED / name)
    return json.loads(wrapper['payload'])


def brief(data):
    names = {p['StableId']: p['Name'] for p in data['people']}
    return {
        'world_id': data['Id'], 'revision': data['Revision'], 'simulation_seconds': data['Elapsed'],
        'stock': {key: data[key] for key in ['Food', 'Planks', 'Beams', 'Stone', 'Tiles', 'TreasuryCoins']},
        'homes': [{
            'resident': names[h['resident_id']], 'recipe': h['current_recipe'],
            'target': h['target_recipe'], 'installed': len(h['installed_keys']),
            'reason': h.get('choice_reason', ''), 'source': h.get('source', ''),
        } for h in data['organic_homes']],
        'residents': [{key: p[key] for key in ['Name', 'Energy', 'Hunger', 'Coins', 'Task', 'LatestEvent']}
                      for p in data['people']],
        'organic_material_purchases': sum(t.get('Kind', t.get('kind')) == 'organic_material_purchase'
                                         for t in data['transactions']),
    }


def main():
    OUT.mkdir(exist_ok=True)
    baseline = brief(world('stalled-before-fix.json'))
    recovered = brief(world('recovery-world.json'))
    current = brief(world('v4-reviewed.json'))
    regression = read(SAVED / 'FinalRegression' / 'index.json')
    focused = read(SAVED / 'FinalPolicyTests' / 'index.json')
    report = {
        'date': '2026-09-08', 'engine': 'Unreal Engine 5.8',
        'runtime_meshes': 114, 'source_modules': 28, 'master_houses': 3, 'growth_stages': 2,
        'full_regression': {k: regression[k] for k in ['succeeded', 'succeededWithWarnings', 'failed', 'notRun']},
        'final_policy_and_runtime_tests': {k: focused[k] for k in ['succeeded', 'succeededWithWarnings', 'failed', 'notRun']},
        'same_world_recovered': baseline['world_id'] == recovered['world_id'],
        'stalled_before_fix': baseline, 'recovered_world': recovered, 'fresh_reviewed_world': current,
        'api_mode': 'HearthDisableApi: local rules only; zero paid API calls in this review',
        'initial_estate': 'The nine initial homes are declared founding property, not construction completed by NPCs during this run.',
        'limitations': [
            'Resident architecture decisions currently use local rules, not verified live Kimi decisions.',
            'Three master house families and two growth stages are not six independently authored masters.',
            'Marriage, children, arbitrary cross-family replacement, interior navigation and paint trading are not implemented.',
            'One-shot independent SceneCapture images underexpose shaded facades; delivered overview uses the actual game viewport.',
            'Initial plot placement still follows deterministic roads; fully emergent terrain-aware settlement growth remains future work.',
        ],
    }
    (OUT / 'validation.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    source = SAVED / 'game-overview.png'
    if source.exists():
        shutil.copy2(source, OUT / '游戏运行全景.png')
    print(json.dumps({'report': str(OUT / 'validation.json'), 'same_world': report['same_world_recovered'],
                      'homes': len(current['homes'])}, ensure_ascii=False))


if __name__ == '__main__':
    main()
