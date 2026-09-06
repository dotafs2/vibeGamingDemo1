"""Validate the imported GoodsKit meshes without loading or changing the village world."""
import hashlib
import json
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[3]
KIT = ROOT / 'Art/GoodsKit'


def main():
    imported = json.loads((KIT / 'UE_Import_Report.json').read_text(encoding='utf-8'))
    source = json.loads((KIT / 'validation.json').read_text(encoding='utf-8'))
    source_by_id = {row['id']: row for row in source['assets']}
    rows = []
    assert imported['status'] == 'passed'
    assert len(imported['assets']) == source['asset_count'] == 8
    for row in imported['assets']:
        asset_id = row['id']
        expected = source_by_id[asset_id]
        source_path = ROOT / row['source']
        assert hashlib.sha256(source_path.read_bytes()).hexdigest() == row['source_sha256'] == expected['sha256']
        mesh = ue.load_asset(row['mesh'])
        assert isinstance(mesh, ue.StaticMesh) and mesh.get_num_lods() > 0, asset_id
        bounds = mesh.get_bounds().box_extent
        source_min, source_max = expected['bounds_authoring_m']
        expected_extent = [(hi - lo) * 50 for lo, hi in zip(source_min, source_max)]
        actual_extent = [bounds.x, bounds.y, bounds.z]
        assert all(abs(actual - wanted) < 0.03 for actual, wanted in zip(actual_extent, expected_extent)), (asset_id, actual_extent, expected_extent)
        slots = mesh.get_editor_property('static_materials')
        assert len(slots) == row['material_slots'] > 0
        assert all(slot.get_editor_property('material_interface') is not None for slot in slots)
        rows.append({'id': asset_id, 'native_mesh': row['mesh'], 'source_hash_current': True,
                     'dimensions_cm': [value * 2 for value in actual_extent], 'material_slots': len(slots)})
    report = {'status': 'passed', 'engine': ue.SystemLibrary.get_engine_version(),
              'scope': 'native renderable meshes, source hashes, scale and material slots',
              'native_meshes': len(rows), 'inventory_binding_verified': False,
              'carry_animation_verified': False, 'assets': rows}
    (KIT / 'UE_Validation.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({key: value for key, value in report.items() if key != 'assets'}))


if __name__ == '__main__':
    main()
