"""Read-only roof regression audit, including the actual runtime cottage copy.

Ordinary Python checks GLB PBR and split normals. Unreal Python additionally
checks native material factors, Nanite and LOD build settings. Neither mode
imports, saves assets, changes a map, or starts the village simulation.
"""
import hashlib
import importlib.util
import json
from pathlib import Path
import sys

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[3]
OUT = ROOT / 'Art/RoofMaterialAudit'
COTTAGE = ROOT / 'Art/HearthCottage'
RUNTIME = '/Game/ThreeHearths/Generated/HearthCottageRuntime/SM_HearthCottage_Polished.SM_HearthCottage_Polished'


def load_reader():
    spec = importlib.util.spec_from_file_location('roof_glb', COTTAGE / 'validate_polished_cottage.py')
    reader = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(reader)
    return reader


def audit_source(asset_id, path, family, reader):
    document, meshes = reader.read(path)
    names = ('Roof',) if asset_id.startswith('cottage') else ('VK_Timber_Shingle_',) if family == 'timber' else ('VK_Terracotta_', 'VK_SlateBlue_')
    materials = [m for m in document['materials'] if m['name'].startswith(names)]
    assert len(materials) == 4, (asset_id, 'roof material family missing')
    expected = [.78] * 4 if family == 'legacy' else [.48, .50, .51, .53] if family == 'timber' else [.34, .36, .38, .40]
    roughness = sorted(m['pbrMetallicRoughness'].get('roughnessFactor', 1) for m in materials)
    assert all(abs(a-b) < 1e-5 for a, b in zip(roughness, expected)), (asset_id, roughness)
    assert all(m['pbrMetallicRoughness'].get('metallicFactor', 1) == 0 for m in materials)
    selected = [v for k, v in meshes.items() if not asset_id.startswith('cottage') or k.startswith('Roof |')]
    smooth = sum(m['smooth_triangles'] for m in selected)
    triangles = sum(m['triangles'] for m in selected)
    assert triangles and (smooth == 0 if family == 'legacy' else smooth / triangles > .15), (asset_id, 'split normal regression')
    return {'id': asset_id, 'source': path.relative_to(ROOT).as_posix(),
            'source_sha256': hashlib.sha256(path.read_bytes()).hexdigest(), 'family': family,
            'roof_roughness': roughness, 'metallic': 0, 'roof_triangles': triangles,
            'smooth_triangles': smooth, 'smooth_triangle_fraction': smooth / triangles,
            'unit_normals_and_uv_verified': True, 'normal_texture': False,
            'source_tangents_exported': any('TANGENT' in p['attributes'] for m in document['meshes'] for p in m['primitives'])}


def audit_native(rows, ue):
    # Shared verification checks source hash-derived geometry, UV, full PBR and
    # disabled Nanite rather than merely accepting an old import report.
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    from import_resident_kit import verify_native
    kit = json.loads((ROOT / 'Art/VillageKit/UE_Import_Report.json').read_text(encoding='utf-8'))
    paths = {r['id']: r['mesh'] for r in kit['assets']}
    paths['cottage_polished'] = json.loads((COTTAGE / 'Polished_UE_Import.json').read_text(encoding='utf-8'))['assets'][0]['mesh']
    paths['cottage_runtime'] = RUNTIME
    native = []
    for source in rows:
        if source['family'] == 'legacy':
            continue
        ids = [source['id'], 'cottage_runtime'] if source['id'] == 'cottage_polished' else [source['id']]
        for asset_id in ids:
            row = {'id': asset_id, 'mesh': paths[asset_id]}
            verify_native(row, ROOT / source['source'])
            mesh = ue.load_asset(row['mesh'])
            subsystem = ue.get_editor_subsystem(ue.StaticMeshEditorSubsystem)
            row['native_build_settings_read'] = subsystem is not None
            if subsystem:
                settings = subsystem.get_lod_build_settings(mesh, 0)
                row['build_settings'] = {key: bool(settings.get_editor_property(key)) for key in
                                         ('recompute_normals', 'recompute_tangents', 'use_mikk_t_space')}
                if row['build_settings']['recompute_normals']:
                    row['observation'] = 'UE rebuilds normals. Source smooth normals are verified, but the final vertex normals are not byte-compared; this flag alone does not prove lost smoothing. Existing same-light UE images show continuous highlights on Polished.'
            else:
                row['build_settings_unavailable'] = 'UE5.8 commandlet has no StaticMeshEditorSubsystem; use -ExecutePythonScript in editor for this check.'
            row['material_parents'] = sorted({slot.get_editor_property('material_interface').get_editor_property('parent').get_path_name()
                                              for slot in mesh.get_editor_property('static_materials')})
            row['exposed_specular_parameters'] = {}
            for slot in mesh.get_editor_property('static_materials'):
                material = slot.get_editor_property('material_interface')
                parameters = {str(name): ue.MaterialEditingLibrary.get_material_instance_scalar_parameter_value(material, name)
                              for name in ue.MaterialEditingLibrary.get_scalar_parameter_names(material)
                              if 'specular' in str(name).lower()}
                row['exposed_specular_parameters'][material.get_name()] = parameters
            row['specular_scope'] = 'Named effective scalar values only; an empty map is not zero specular. Source uses glTF nonmetal dielectric defaults.'
            row['source_sha256'] = source['source_sha256']
            native.append(row)
    return native


def main():
    reader = load_reader()
    rows = [audit_source('cottage_legacy', COTTAGE / 'HearthCottage_SharedUV.glb', 'legacy', reader),
            audit_source('cottage_polished', COTTAGE / 'HearthCottage_SharedUV_Polished.glb', 'ceramic', reader)]
    catalog = json.loads((ROOT / 'Art/VillageKit/catalog.json').read_text(encoding='utf-8'))
    for module in catalog['modules']:
        if module['category'] in ('roof_slope', 'ridge', 'canopy'):
            rows.append(audit_source(module['id'], ROOT / 'Art/VillageKit' / module['asset_glb'],
                                     'timber' if 'timber' in module['id'] else 'ceramic', reader))
    report = {'status': 'passed', 'read_only': True, 'source_assets': rows,
              'scope': 'Source PBR, smooth split normals and UV; legacy baseline intentionally retained.',
              'native_verified': False, 'fresh_screenshot_taken': False}
    try:
        import unreal as ue
    except ImportError:
        ue = None
    OUT.mkdir(parents=True, exist_ok=True)
    target = OUT / ('native-audit.json' if ue else 'source-audit.json')
    try:
        if ue is not None:
            report.update(status='running', engine=ue.SystemLibrary.get_engine_version())
            target.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
            report['native_assets'] = audit_native(rows, ue)
            report.update(status='passed', native_verified=True)
            report['native_build_settings_read'] = all(r['native_build_settings_read'] for r in report['native_assets'])
            report['observations'] = [{'id': r['id'], 'note': r['observation']} for r in report['native_assets'] if 'observation' in r]
            report['scope'] += ' Native source PBR, geometry and Nanite, including runtime copy. Build settings are recorded, not a final native split-normal comparison.'
    except Exception as exc:
        report.update(status='failed', error=repr(exc))
        raise
    finally:
        target.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print('ROOF_MATERIAL_AUDIT_PASSED ' + str(target))


if __name__ == '__main__':
    try:
        main()
    finally:
        if 'unreal' in sys.modules:
            ue = sys.modules['unreal']
            if 'HearthRoofAuditQuit' in ue.SystemLibrary.get_command_line():
                ue.SystemLibrary.quit_editor()
