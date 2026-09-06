"""Versioned ceramic-finish repair. Run through Blender MCP; each render is separate.

Only the four roof materials and roof shading flags change. Geometry, shared UV0,
transforms, non-roof materials, lights and the two original SharedUV files survive.
"""
import bpy
from collections import Counter, defaultdict
import hashlib
import json
from pathlib import Path
import sys
from mathutils import Vector

OUT=Path(__file__).resolve().parent
SOURCE='HearthCottage_SharedUV'
TARGET=SOURCE+'_Polished'
sys.path.insert(0,str(OUT))
from shared_roof_uv import ROOF_NAMES, connected_parts, chart_for

PALETTE=[('BB695F',.34),('CA7C6F',.38),('AF6059',.36),('D28B79',.40)]

def dump(path,data):
    path.write_text(json.dumps(data,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')

def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()

def fingerprint(value):return hashlib.sha256(json.dumps(value,sort_keys=True).encode()).hexdigest()

def original_files():return {str(OUT/(SOURCE+ext)):sha(OUT/(SOURCE+ext)) for ext in ('.blend','.glb')}

def open_source():
    bpy.ops.wm.open_mainfile(filepath=str(OUT/(SOURCE+'.blend')),load_ui=False)
    active=bpy.context.view_layer.objects.active
    if active and active.mode!='OBJECT':
        with bpy.context.temp_override(object=active,active_object=active,
                selected_objects=[active],selected_editable_objects=[active]):
            bpy.ops.object.mode_set(mode='OBJECT')
    bpy.context.view_layer.update()
    return bpy.data.collections['HOUSE | Hearth Cottage']

def house_meshes():
    return sorted([o for o in bpy.data.collections['HOUSE | Hearth Cottage'].objects if o.type=='MESH'],key=lambda o:o.name)

def material_state(m):
    node=m.node_tree.nodes.get('Principled BSDF')
    return {'name':m.name,'base_color_linear':list(node.inputs['Base Color'].default_value),
        'roughness':node.inputs['Roughness'].default_value,'metallic':node.inputs['Metallic'].default_value,
        'ior':node.inputs['IOR'].default_value,'specular_ior_level':node.inputs['Specular IOR Level'].default_value,
        'coat_weight':node.inputs['Coat Weight'].default_value,
        'roughness_linked':node.inputs['Roughness'].is_linked,'normal_linked':node.inputs['Normal'].is_linked}

def invariant_state():
    output=[]
    for o in house_meshes():
        data=o.data
        output.append({'name':o.name,'transform':[list(r) for r in o.matrix_world],
            'vertices':[list(v.co) for v in data.vertices],
            'faces':[(list(p.vertices),p.material_index) for p in data.polygons],
            'materials':[m.name for m in data.materials],
            'uv_layers':[{'name':u.name,'coords':[list(v.uv) for v in u.data]} for u in data.uv_layers]})
    return fingerprint(output)

def non_roof_state():
    mats={m.name:material_state(m) for o in house_meshes() if o.name not in ROOF_NAMES for m in o.data.materials}
    shading={o.name:{'faces':[p.use_smooth for p in o.data.polygons],
       'sharp_edges':[e.use_edge_sharp for e in o.data.edges]} for o in house_meshes() if o.name not in ROOF_NAMES}
    return fingerprint({'materials':mats,'shading':shading})

def lighting_state():
    scene=bpy.context.scene
    world=scene.world.node_tree.nodes.get('Background')
    return {'lights':[{'name':o.name,'matrix':[list(r) for r in o.matrix_world],
       'type':o.data.type,'energy':o.data.energy,'color':list(o.data.color),
       'size':getattr(o.data,'size',None)} for o in sorted(bpy.data.objects,key=lambda o:o.name) if o.type=='LIGHT'],
       'world_color':list(world.inputs['Color'].default_value),'world_strength':world.inputs['Strength'].default_value,
       'view_transform':scene.view_settings.view_transform,'look':scene.view_settings.look,
       'exposure':scene.view_settings.exposure,'gamma':scene.view_settings.gamma}

def roof_state():
    output=[]
    for name,kind in ROOF_NAMES.items():
        o=bpy.data.objects[name];o.data.calc_loop_triangles()
        output.append({'name':name,'kind':kind,'pieces':len(connected_parts(o)),
          'faces':len(o.data.polygons),'smooth_faces':sum(p.use_smooth for p in o.data.polygons),
          'triangles':len(o.data.loop_triangles),'sharp_edges':sum(e.use_edge_sharp for e in o.data.edges),
          'uv_name':o.data.uv_layers.active.name,'materials':[material_state(m) for m in o.data.materials]})
    return output

def inspect():
    open_source()
    report={'source':SOURCE,'source_files':original_files(),'roof':roof_state(),
      'lights_and_color_management':lighting_state(),'geometry_and_uv_fingerprint':invariant_state(),
      'non_roof_fingerprint':non_roof_state(),
      'diagnosis':['All four roof materials use inherited roughness 0.78 and metallic 0',
        'All curved tile and ridge faces have flat shading, so reflections break at each polygon',
        'Normal and roughness inputs are unlinked; no micro-normal or roughness texture is supplied',
        'Old roof base colours are lighter/pinker than the current VillageKit terracotta palette',
        'Missing textures alone do not prevent highlights; roughness and surface-normal continuity are the direct repair targets']}
    assert all(abs(m['roughness']-.78)<1e-5 and m['metallic']==0 for r in report['roof'] for m in r['materials'])
    assert all(r['smooth_faces']==0 for r in report['roof'])
    dump(OUT/(TARGET+'_diagnosis.json'),report)
    return report

def linear(v):return v/12.92 if v<=.04045 else ((v+.055)/1.055)**2.4

def apply_finish(palette=True):
    touched={}
    for name,kind in ROOF_NAMES.items():
        o=bpy.data.objects[name];data=o.data
        chart_by_face={}
        for part in connected_parts(o):
            for p in part['faces']:
                chart=chart_for(kind,[part['local'][v] for v in p.vertices])
                chart_by_face[p.index]=chart
                p.use_smooth=chart in ('top','bottom','outer','inner')
        edge_faces=defaultdict(list)
        for p in data.polygons:
            for key in p.edge_keys:edge_faces[tuple(sorted(key))].append(p.index)
        for e in data.edges:
            faces=edge_faces[tuple(sorted(e.vertices))]
            e.use_edge_sharp=len(faces)!=2 or len({chart_by_face[p] for p in faces})!=1
        data.update()
        for m in data.materials:
            if m.name in touched:continue
            assert m.name.startswith('Roof | '),m.name
            i=int(m.name.split('|')[1].strip())-1
            color,rough=PALETTE[i]
            node=m.node_tree.nodes.get('Principled BSDF')
            node.inputs['Roughness'].default_value=rough
            node.inputs['Metallic'].default_value=0
            if palette:
                rgb=[linear(int(color[j:j+2],16)/255) for j in (0,2,4)]
                node.inputs['Base Color'].default_value=(*rgb,1)
                m.diffuse_color=(*rgb,1)
            touched[m.name]=material_state(m)
    bpy.context.view_layer.update()
    return touched

def build():
    before=inspect();light=fingerprint(lighting_state())
    apply_finish(palette=True)
    assert invariant_state()==before['geometry_and_uv_fingerprint'],'Geometry, topology, transforms or UV changed'
    assert non_roof_state()==before['non_roof_fingerprint'],'Non-roof material/shading changed'
    assert fingerprint(lighting_state())==light,'Lighting changed'
    house=bpy.data.collections['HOUSE | Hearth Cottage']
    root=next(o for o in house.objects if o.type=='EMPTY' and not o.parent)
    root['variant']='Polished terracotta; roof-only finish and smooth curved normals'
    root['source_asset']=SOURCE
    root['geometry_unchanged']=True
    for o in bpy.context.view_layer.objects:o.select_set(False)
    for o in house.objects:o.hide_set(False);o.select_set(True)
    active=house_meshes()[0];bpy.context.view_layer.objects.active=active
    window=bpy.context.window_manager.windows[0]
    with bpy.context.temp_override(window=window,object=active,active_object=active,selected_objects=list(house.objects),
            selected_editable_objects=list(house.objects)):
        bpy.ops.export_scene.gltf(filepath=str(OUT/(TARGET+'.glb')),export_format='GLB',use_selection=True,
            export_yup=True,export_apply=True,export_texcoords=True,export_normals=True,
            export_animations=False,export_cameras=False,export_lights=False,export_extras=True,export_materials='EXPORT')
    bpy.ops.wm.save_as_mainfile(filepath=str(OUT/(TARGET+'.blend')),compress=True)
    assert original_files()==before['source_files'],'Original SharedUV files changed'
    report={'status':'passed','asset':TARGET,'source':SOURCE,'blender':bpy.app.version_string,
      'original_files':before['source_files'],'geometry_topology_transforms_uv_unchanged':True,
      'geometry_and_uv_fingerprint':invariant_state(),'non_roof_materials_and_shading_unchanged':True,
      'lighting_and_color_management_unchanged':True,'added_geometry':0,'bevel_added':False,
      'changes':['Roof roughness: 0.78 -> 0.34/0.38/0.36/0.40; metallic remains 0',
        'Only curved top/bottom tile surfaces and outer/inner ridge surfaces are smooth',
        'End caps and side edges remain hard; sharp flags explicitly separate chart surfaces',
        'Four roof base colours now match current VillageKit terracotta palette'],
      'unchanged_shader_controls':['IOR','Specular IOR Level','Coat Weight','Normal input'],
      'roof':roof_state(),'ue_import_verified':False,
      'outputs':[{'file':TARGET+ext,'sha256':sha(OUT/(TARGET+ext)),'bytes':(OUT/(TARGET+ext)).stat().st_size} for ext in ('.blend','.glb')]}
    dump(OUT/(TARGET+'_repair-report.json'),report)
    return report

def render(state,view='house'):
    assert state in ('original','finish_only','polished') and view in ('house','roof')
    frozen=original_files();open_source()
    light=lighting_state()
    if state!='original':apply_finish(palette=state=='polished')
    assert lighting_state()==light
    scene=bpy.context.scene
    # Existing source portrait, studio lamps and colour management remain intact.
    if view=='roof':
        scene.camera.location=(6.5,-8.5,6.0)
        scene.camera.rotation_euler=(Vector((.3,-.03,2.7))-scene.camera.location).to_track_quat('-Z','Y').to_euler()
        scene.camera.data.ortho_scale=4.4
    scene.render.resolution_x=1280
    scene.render.resolution_y=1152 if view=='house' else 900
    scene.render.resolution_percentage=100
    scene.render.engine='CYCLES';scene.cycles.device='CPU';scene.cycles.samples=40
    scene.cycles.use_denoising=True;scene.cycles.seed=17
    scene.render.threads_mode='FIXED';scene.render.threads=4
    scene.render.image_settings.file_format='PNG'
    scene.render.filepath=str(OUT/(TARGET+'_'+view+'_'+state+'.png'))
    # Hidden modelling helpers never affect render visibility in the saved source.
    with bpy.context.temp_override(window=bpy.context.window_manager.windows[0]):
        bpy.ops.render.render(write_still=True)
    assert original_files()==frozen
    meta={'state':state,'view':view,'file':Path(scene.render.filepath).name,'sha256':sha(Path(scene.render.filepath)),
      'lighting':light,'lighting_fingerprint':fingerprint(light),'geometry_and_uv_fingerprint':invariant_state(),
      'camera_matrix':[list(r) for r in scene.camera.matrix_world],'ortho_scale':scene.camera.data.ortho_scale,
      'resolution':[scene.render.resolution_x,scene.render.resolution_y],'samples':40,'seed':17,
      'roughness_and_normals_changed':state!='original','base_color_changed':state=='polished'}
    dump(OUT/(TARGET+'_'+view+'_'+state+'_render.json'),meta)
    return meta

def verify_saved():
    report=json.loads((OUT/(TARGET+'_repair-report.json')).read_text(encoding='utf-8'))
    diagnosis=json.loads((OUT/(TARGET+'_diagnosis.json')).read_text(encoding='utf-8'))
    bpy.ops.wm.open_mainfile(filepath=str(OUT/(TARGET+'.blend')),load_ui=False)
    bpy.context.view_layer.update()
    assert invariant_state()==report['geometry_and_uv_fingerprint']
    assert non_roof_state()==diagnosis['non_roof_fingerprint']
    assert roof_state()==report['roof']
    assert lighting_state()==diagnosis['lights_and_color_management']
    assert original_files()==report['original_files']
    for entry in report['outputs']:assert sha(OUT/entry['file'])==entry['sha256']
    report['saved_blend_reopened_verified']=True
    dump(OUT/(TARGET+'_repair-report.json'),report)
    return {'status':'passed','saved_blend_reopened_verified':True}

if __name__=='__main__':
    args=sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else ['build']
    result=inspect() if args[0]=='inspect' else build() if args[0]=='build' else render(args[1],args[2] if len(args)>2 else 'house')
    print('COTTAGE_POLISHED '+json.dumps(result,ensure_ascii=False),flush=True)
