"""Embed native bitmap textures in the already-built scene and re-export."""
from pathlib import Path
import hashlib,json,sys
import bpy
OUT=Path(__file__).resolve().parent
sys.path.insert(0,str(OUT))
from prepare_fbx_images import prepare_images
MANIFEST=OUT/'ThreeHearths_All_Git_Assets_ccd6757.json'
m=json.loads(MANIFEST.read_text(encoding='utf-8'))
bpy.ops.wm.open_mainfile(filepath=str(OUT/m['blend']))
previous_shader_notes=m.get('textures',{}).get('runtime_shader_samplers_omitted',[])
m['textures']=prepare_images(OUT.parents[1]/'Saved/ThreeHearths/CombinedFbx/Textures')
m['textures']['runtime_shader_samplers_omitted']=sorted(set(previous_shader_notes+m['textures']['runtime_shader_samplers_omitted']))
bpy.ops.object.select_all(action='SELECT')
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/m['blend']),compress=True)
fbx=OUT/m['fbx']
bpy.ops.export_scene.fbx(filepath=str(fbx),use_selection=True,object_types={'MESH','EMPTY','ARMATURE'},
    global_scale=1.0,apply_unit_scale=True,apply_scale_options='FBX_SCALE_UNITS',
    axis_forward='-Z',axis_up='Y',use_mesh_modifiers=True,mesh_smooth_type='OFF',
    add_leaf_bones=False,bake_anim=False,path_mode='COPY',embed_textures=True,use_custom_props=True)
m['fbx_bytes']=fbx.stat().st_size
m['fbx_sha256']=hashlib.sha256(fbx.read_bytes()).hexdigest()
MANIFEST.write_text(json.dumps(m,ensure_ascii=False,indent=2),encoding='utf-8')
print('FINALIZED_FBX '+json.dumps({'bytes':m['fbx_bytes'],'textures':m['textures']}),flush=True)
