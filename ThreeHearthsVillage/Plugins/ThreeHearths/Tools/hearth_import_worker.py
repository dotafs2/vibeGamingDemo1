"""Editor commandlet worker for PIE previews; the game polls its result file.

UE 5.8.1 Interchange's PIE fast-build path loses render data when the editor
factory rebuilds it without a committed MeshDescription. Importing in this
separate editor process uses the supported editor asset build path instead.
"""
import json
from pathlib import Path
import sys
import time

import unreal

sys.path.insert(0, str(Path(__file__).resolve().parent))
from pie_import_preview import inspect_glb


def main():
    _, _, arguments = unreal.SystemLibrary.parse_command_line(unreal.SystemLibrary.get_command_line())
    request_path = Path(arguments['HearthImportRequest'])
    request = json.loads(request_path.read_text(encoding='utf-8'))
    result_path = request_path.with_name('result.json')
    started = time.monotonic()
    result = {'success': False, 'editor_only': True, 'assets': []}
    try:
        inspect_glb(request['source'])
        destination = request['destination']
        if not destination.startswith('/Game/Developers/ThreeHearthsImportPreview/'):
            raise ValueError('Worker destination must be the preview cache')
        manager = unreal.InterchangeManager.get_interchange_manager_scripted()
        pipeline = unreal.InterchangeGenericAssetsPipeline()
        mesh_pipeline = pipeline.get_editor_property('mesh_pipeline')
        mesh_pipeline.set_editor_property('import_static_meshes', True)
        mesh_pipeline.set_editor_property('import_skeletal_meshes', False)
        mesh_pipeline.set_editor_property('combine_static_meshes_behavior', unreal.InterchangeCombineStaticMeshesBehavior.ALL)
        mesh_pipeline.set_editor_property('collision', False)
        pipeline.get_editor_property('common_meshes_properties').set_editor_property('bake_meshes', True)
        pipeline.get_editor_property('animation_pipeline').set_editor_property('import_animations', False)
        pipeline.get_editor_property('material_pipeline').set_editor_property('import_materials', True)
        gltf_pipeline = unreal.InterchangeGLTFPipeline()
        source_data = manager.create_source_data(request['source'])
        parameters = unreal.ImportAssetParameters()
        parameters.set_editor_property('is_automated', True)
        parameters.set_editor_property('replace_existing', False)
        parameters.set_editor_property('override_pipelines', [
            unreal.SoftObjectPath(pipeline.get_path_name()),
            unreal.SoftObjectPath(gltf_pipeline.get_path_name())])
        imported = []
        parameters.on_assets_import_done.bind_callable(lambda objects: imported.extend(objects))
        # Synchronous inside the worker; the playable editor never waits here.
        if not manager.import_asset(destination, source_data, parameters):
            raise RuntimeError('Interchange rejected the source')
        meshes = [obj for obj in imported if isinstance(obj, unreal.StaticMesh)]
        if len(meshes) != 1 or meshes[0].get_num_lods() < 1:
            raise RuntimeError('Interchange did not produce a renderable combined mesh')
        assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
        for obj in imported:
            if not obj.get_path_name().startswith(destination + '/'):
                raise RuntimeError('Imported object escaped the preview cache')
            if not assets.save_loaded_asset(obj, False):
                raise RuntimeError('Could not save preview asset: ' + obj.get_path_name())
            result['assets'].append(obj.get_path_name())
        result.update(success=True, mesh=meshes[0].get_path_name())
    except Exception as exc:
        result['error'] = str(exc)
        unreal.log_error('[HearthImportWorker] ' + str(exc))
    result['import_seconds'] = time.monotonic() - started
    temporary = result_path.with_suffix('.tmp')
    temporary.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
    temporary.replace(result_path)


if __name__ == '__main__':
    main()
