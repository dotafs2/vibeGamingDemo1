"""Build a source/native inventory and actionable planner handoff; no asset writes."""
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import sys

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[3]
TOOLS = Path(__file__).resolve().parent
KIT = ROOT / 'Art/VillageKit'
OUT = ROOT / 'Art/ResidentialVariants'
CPP = ROOT / 'Plugins/ThreeHearths/Source/ThreeHearths/Private'


def read(path):
    return json.loads(path.read_text(encoding='utf-8'))


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def disposition(module, active, adapter):
    asset_id, category = module['id'], module['category']
    if asset_id in active:
        return 'current_planner', []
    if asset_id in adapter:
        return 'native_ready_catalog_placement_needed', ['register measured bounds and support sockets', 'add placement and opening/roof clearance validation', 'add planner selection and persistent preference']
    if any(r in module['resource_inputs'] for r in ('plaster', 'tiles')):
        return 'native_ready_economy_and_binding_needed', ['implement authored plaster/tiles resource and haul accounting', 'register catalog/support/adapter recipe', 'persist resident material choice']
    if category in ('wall', 'wall_door', 'wall_window', 'gable'):
        return 'native_ready_binding_needed', ['register catalog bounds/support/adapter', 'choose validated runtime stone recipe; authored art recipe is not active balance', 'validate opening and thickness differences', 'persist resident material choice']
    if category == 'frame':
        return 'native_ready_assembly_policy_needed', ['choose atomic combined-frame install OR individual posts/beams, avoid duplicate structural seams', 'register support, cost and assembly dependencies']
    if category in ('stairs', 'floor_opening'):
        return 'native_ready_multistorey_navigation_needed', ['reserve real stairwell and floor opening', 'implement floor-to-floor navigation and clearance', 'register support and recipes']
    if category == 'carry':
        return 'native_ready_carry_visual', ['map real inventory cargo to this visual and grip', 'do not install the carry mesh as structure; keep quantities in economy']
    if category == 'fixture':
        return 'native_ready_fixture_binding_needed', ['define wall-relative attachment anchor', 'validate door swing or shutter clearance', 'register owner and install recipe']
    return 'native_ready_site_binding_needed', ['define installation anchor/support/clearance', 'register recipe and owner', 'choose from real resident or shared-facility need']


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    sys.path.insert(0, str(TOOLS))
    from validate_village_kit import inspect_asset
    spec = importlib.util.spec_from_file_location('variant_glb', ROOT / 'Art/ToolKit/validate_tool_kit.py')
    glb = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(glb)
    catalog = read(KIT / 'catalog.json')
    imports = {r['id']: r for r in read(KIT / 'UE_Import_Report.json')['assets']}
    native_catalog = (CPP / 'HearthStructureCatalog.cpp').read_text(encoding='utf-8')
    planner = (CPP / 'HearthResidentBuildingPlanner.cpp').read_text(encoding='utf-8')
    adapter_text = (CPP / 'HearthPlannedConstructionAdapter.cpp').read_text(encoding='utf-8')
    registered = set(re.findall(r'Entry\(TEXT\("([^"]+)"\)', native_catalog))
    recipes = {asset: {'recipe_id': recipe, 'material_id': material, 'quantity': 1}
               for recipe, asset, material in re.findall(r'Recipe\(TEXT\("([^"]+)"\),TEXT\("([^"]+)"\),TEXT\("([^"]+)"\)\)', planner)}
    active = registered & recipes.keys()
    adapter = set(re.findall(r'CatalogId\s*==\s*TEXT\("([^"]+)"\)', adapter_text))
    rows, palette = [], {}
    roof_audit = read(ROOT / 'Art/RoofMaterialAudit/native-audit.json')
    assert roof_audit['status'] == 'passed' and roof_audit['native_verified']
    roof_sources = {r['id']: r for r in roof_audit['source_assets']}
    for module in catalog['modules']:
        asset_id = module['id']
        source = KIT / module['asset_glb']
        imported = imports[asset_id]
        assert imported['source_sha256'] == sha(source), (asset_id, 'native import provenance stale')
        asset_path = imported['mesh'].split('.')[0]
        package = ROOT / 'Content' / (asset_path.removeprefix('/Game/') + '.uasset')
        assert package.is_file(), (asset_id, 'native package missing')
        source_check = inspect_asset(source)
        document, bounds, _ = glb.decode(source)
        assert len(document['nodes']) == 1 and not any(k in document['nodes'][0] for k in ('matrix', 'translation', 'rotation', 'scale'))
        size = [bounds[1][i] - bounds[0][i] for i in range(3)]
        assert all(abs(size[i] * 50 - imported['bounds_extent_cm'][i]) < .15 for i in range(3)), (asset_id, 'recorded native dimension mismatch')
        pbr = [{'name': m['name'], **m['pbrMetallicRoughness']} for m in document['materials']]
        assert all(0 <= m.get('roughnessFactor', 1) <= 1 and 0 <= m.get('metallicFactor', 1) <= 1 for m in pbr)
        for material in pbr:
            if material['name'] in palette:
                assert palette[material['name']] == material, (asset_id, 'shared palette name has conflicting PBR', material['name'])
            palette[material['name']] = material
        assert not document.get('textures') and not document.get('images')
        state, tasks = disposition(module, active, adapter)
        rows.append({'id': asset_id, 'category': module['category'], 'material_family': module.get('material_family'),
                     'mesh': imported['mesh'], 'source': source.relative_to(ROOT).as_posix(), 'source_sha256': sha(source),
                     'native_package_sha256': sha(package), 'native_package_exists': True,
                     'source_matches_import_record': True, 'recorded_native_dimensions_match': True,
                     'bounds_authoring_m': bounds, 'dimensions_m': size,
                     'origin': 'Source-local identity, +Z up, front -Y. Preserve this component datum; do not recenter.',
                     'triangles': source_check['triangles'], 'uv0': True, 'unit_normals': True,
                     'pbr': pbr, 'style': 'Existing VillageKit palette and beveled low-poly geometry; no external textures.',
                     'can_use_in_current_planner': asset_id in active, 'integration_status': state,
                     'runtime_recipe': recipes.get(asset_id), 'authored_art_recipe_not_runtime_balance': module['resource_inputs'],
                     'npc_handheld': module['npc_handheld'], 'execution_unit': module['execution_unit'], 'next_steps': tasks})
        if asset_id in roof_sources:
            assert roof_sources[asset_id]['source_sha256'] == sha(source)
            rows[-1]['roof_highlight_evidence'] = {'report': 'Art/RoofMaterialAudit/native-audit.json',
                'source_hash_matches': True, 'native_pbr_and_nanite_verified': True,
                'source_smooth_triangle_fraction': roof_sources[asset_id]['smooth_triangle_fraction'],
                'fresh_screenshot_taken': False, 'native_split_normals_byte_compared': False}
    groups = {}
    for category in ('wall', 'wall_door', 'wall_window', 'gable', 'roof_slope', 'ridge', 'canopy'):
        members = [r for r in rows if r['category'] == category]
        baseline = next((r for r in members if r['material_family'] == 'timber'), members[0])
        groups[category] = {'baseline': baseline['id'], 'variants': [
            {'id': r['id'], 'max_bounds_delta_from_baseline_m': max(abs(r['bounds_authoring_m'][j][i] - baseline['bounds_authoring_m'][j][i]) for j in range(2) for i in range(3)),
             'swap_policy': 'Replace whole component asset, preserve local datum, revalidate measured bounds/support/opening. Do not reuse material-slot indices between meshes.'} for r in members]}
    report = {'status': 'passed', 'scope': 'Source GLB geometry/PBR plus current package existence/hash and import provenance. Native live query is separate.',
              'asset_count': len(rows), 'current_planner_count': len(active), 'missing_native_packages': [],
              'source_cpp_sha256': {name: sha(CPP / name) for name in ('HearthStructureCatalog.cpp', 'HearthResidentBuildingPlanner.cpp', 'HearthPlannedConstructionAdapter.cpp')},
              'active_planner_materials': ['stone', 'plank', 'beam'], 'art_to_runtime_material_aliases': {'planks': 'plank'},
              'palette_consistency': 'Repeated exact material names have identical source PBR factors.', 'palette': palette,
              'material_variant_groups': groups, 'components': rows,
              'missing_asset_scope': 'Current VillageKit catalog; does not claim all other art collections were searched.',
              'missing_asset_variants': [{'id': 'canopy_timber_2m', 'reason': 'No timber canopy in VillageKit; ceramic canopy would require tiles.'},
                                         {'id': 'alternative_structural_frame_family', 'reason': 'Only timber post/beam/frame exists; stone walls are infill variants, not stone posts/beams.'}],
              'installation_rule': 'NPC carries real recipe inputs; the installed component may be a wall/frame unit. Never let an NPC hand-carry an entire floor/house.'}
    (OUT / 'component-map.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    try:
        import unreal as ue
    except ImportError:
        ue = None
    if ue:
        from import_resident_kit import verify_native
        native_report = {'status': 'running', 'read_only': True, 'component_map_sha256': sha(OUT / 'component-map.json'), 'assets': []}
        try:
            for row in rows:
                result = {'id': row['id'], 'mesh': row['mesh']}
                verify_native(result, ROOT / row['source'])
                native_report['assets'].append(result)
            native_report.update(status='passed', engine=ue.SystemLibrary.get_engine_version())
        except Exception as exc:
            native_report.update(status='failed', error=repr(exc))
            raise
        finally:
            (OUT / 'native-validation.json').write_text(json.dumps(native_report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print('RESIDENTIAL_VARIANT_AUDIT_PASSED', len(rows), 'assets;', len(active), 'current planner bindings')


if __name__ == '__main__':
    try:
        main()
    finally:
        if 'unreal' in sys.modules:
            ue = sys.modules['unreal']
            if 'HearthVariantsAuditQuit' in ue.SystemLibrary.get_command_line():
                ue.SystemLibrary.quit_editor()
