"""Import the existing authored PMX rig and make a local, skinned Godot asset."""
from pathlib import Path
import sys, os, json, math, hashlib, importlib.util
import bpy
from mathutils import Matrix, Vector

HERE = Path(__file__).resolve().parent
PROJECT = HERE.parents[1]
spec=importlib.util.spec_from_file_location('pmx_reader',HERE/'inspection_dependency/pmx_reader.py')
pmx=importlib.util.module_from_spec(spec);spec.loader.exec_module(pmx)

print(json.dumps({'pid':os.getpid(),'stage':'kirito_import'}),flush=True)
bpy.ops.wm.read_factory_settings(use_empty=True)
source = HERE/'sugaki_kirito/Kirito/キリト.pmx'
data = pmx.load(str(source))
scale = 1.72/(max(v.co[1] for v in data.vertices)-min(v.co[1] for v in data.vertices))
def coord(v):return Vector((v[0],v[2],v[1]))*scale
arm=bpy.data.armatures.new('KiritoSkeleton');rig=bpy.data.objects.new('KiritoSkeleton',arm)
bpy.context.collection.objects.link(rig);bpy.context.view_layer.objects.active=rig;rig.select_set(True)
bpy.ops.object.mode_set(mode='EDIT')
for bone in data.bones:
    b=arm.edit_bones.new(bone.name);b.head=coord(bone.location)
    target=bone.displayConnection
    if isinstance(target,int) and target>=0:b.tail=coord(data.bones[target].location)
    elif isinstance(target,(tuple,list)):b.tail=b.head+coord(target)
    else:b.tail=b.head+Vector((0,0,.03))
    if (b.tail-b.head).length<.0001:b.tail=b.head+Vector((0,0,.03))
for i,bone in enumerate(data.bones):
    if bone.parent>=0:arm.edit_bones[i].parent=arm.edit_bones[bone.parent]
bpy.ops.object.mode_set(mode='OBJECT')
mesh=bpy.data.meshes.new('KiritoMesh')
mesh.from_pydata([coord(v.co) for v in data.vertices],[],data.faces);mesh.update()
ob=bpy.data.objects.new('KiritoMesh',mesh);bpy.context.collection.objects.link(ob)
uv=mesh.uv_layers.new(name='UVMap')
for loop in mesh.loops:
    source_uv=data.vertices[loop.vertex_index].uv;uv.data[loop.index].uv=(source_uv[0],1-source_uv[1])
for face in mesh.polygons:face.use_smooth=True
mesh.normals_split_custom_set_from_vertices([Vector((v.normal[0],v.normal[2],v.normal[1])).normalized() for v in data.vertices])
start=0
for src in data.materials:
    mat=bpy.data.materials.new(src.name);mesh.materials.append(mat)
    count=src.vertex_count//3
    for i in range(start,start+count):mesh.polygons[i].material_index=len(mesh.materials)-1
    start+=count
assert start==len(mesh.polygons)
groups=[ob.vertex_groups.new(name=b.name) for b in data.bones]
for i,v in enumerate(data.vertices):
    w=v.weight
    if w.type==0:weights=[1.0]
    elif w.type==1:weights=[w.weights[0],1-w.weights[0]]
    elif w.type==2:weights=w.weights
    else:raise RuntimeError('Unexpected skinning method: '+str(w.type))
    for bone_index,weight in zip(w.bones,weights):
        if weight>0 and bone_index>=0:groups[bone_index].add([i],weight,'REPLACE')
ob.parent=rig;mod=ob.modifiers.new('AuthoredSkin','ARMATURE');mod.object=rig
ob.shape_key_add(name='Basis',from_mix=False)
for morph in data.morphs:
    if isinstance(morph,pmx.VertexMorph):
        key=ob.shape_key_add(name=morph.name,from_mix=False)
        key.value=0.0
        for delta in morph.offsets:key.data[delta.index].co=coord(data.vertices[delta.index].co)+coord(delta.offset)
        assert max(abs(c) for v in key.data for c in v.co)<3,'Morph coordinates exceeded character bounds'
meshes=[ob]
assert len(rig.data.bones)>=200
for i,ob in enumerate(meshes): ob.name='KiritoMesh' if i==0 else f'KiritoMesh_{i}'
report={'source':str(source),'scale':scale,'blender':bpy.app.version_string,'importer':'existing standalone MMD Tools PMX reader + explicit Blender conversion',
        'bones':len(rig.data.bones),'mesh_objects':len(meshes),'shape_keys':{},'materials':[],
        'physics_transferred':False,'ik_constraints_transferred':False,'material_morphs_transferred':False,'visual_approved':False}
for ob in meshes:
    report['shape_keys'][ob.name]=[k.name for k in ob.data.shape_keys.key_blocks] if ob.data.shape_keys else []
    ob.hide_render=False;ob.hide_set(False)

# Save the full imported rig before adapting materials to glTF.
bpy.ops.file.pack_all()
bpy.ops.wm.save_as_mainfile(filepath=str(HERE/'Kirito_MMD_Imported.blend'))
materials={m.name:m for m in data.materials}
for mat in list(bpy.data.materials):
    name=mat.name
    if name not in materials: continue
    src=materials[name]
    mat.use_nodes=True;nodes=mat.node_tree.nodes;nodes.clear()
    output=nodes.new('ShaderNodeOutputMaterial');bs=nodes.new('ShaderNodeBsdfPrincipled')
    mat.node_tree.links.new(bs.outputs['BSDF'],output.inputs['Surface'])
    bs.inputs['Base Color'].default_value=src.diffuse
    bs.inputs['Alpha'].default_value=src.diffuse[3]
    bs.inputs['Roughness'].default_value=1
    bs.inputs['Specular IOR Level'].default_value=0
    texture=None
    if src.texture>=0:
        path=Path(data.textures[src.texture].path)
        texture=bpy.data.images.load(str(path),check_existing=True)
        tex=nodes.new('ShaderNodeTexImage');tex.image=texture
        mat.node_tree.links.new(tex.outputs['Color'],bs.inputs['Base Color'])
        if src.diffuse[3]>0:
            mat.node_tree.links.new(tex.outputs['Alpha'],bs.inputs['Alpha'])
    mat.surface_render_method='DITHERED'
    mat.use_backface_culling=not src.is_double_sided
    mat['source_material_japanese']=name
    mat['source_alpha']=src.diffuse[3]
    mat['source_outline']=src.enabled_toon_edge
    report['materials'].append({'name':mat.name,'source':name,'alpha':src.diffuse[3],
        'texture':str(texture.filepath) if texture else None,'outline':src.enabled_toon_edge})

# Pose the existing skeleton; keep the skin live and export sampled animation.
# MMD model faces Blender -Y; rotations below are in armature space.
for pb in rig.pose.bones: pb.rotation_mode='QUATERNION'
def rotate_world(name,angle,axis):
    pb=rig.pose.bones[name];rest=pb.bone.matrix_local.to_3x3()
    pb.rotation_quaternion=(rest.inverted()@Matrix.Rotation(angle,3,axis)@rest).to_quaternion()

def relaxed_pose():
    for pb in rig.pose.bones:pb.rotation_quaternion=(1,0,0,0)
    # Existing PMX is an A pose: lower both upper arms toward a resting stance.
    for label,sign in [('左',1),('右',-1)]:
        rotate_world(label+'腕',sign*math.radians(36),'Y')

relaxed_pose()
rig.animation_data_create()
idle=bpy.data.actions.new('Idle');rig.animation_data.action=idle
for frame in [1,31,61,91,121]:
    bpy.context.scene.frame_set(frame);relaxed_pose()
    phase=(frame-1)/120*math.tau
    rotate_world('頭',math.sin(phase)*.018,'Z')
    rotate_world('上半身2',math.sin(phase)*.006,'X')
    for name in ['左腕','右腕','頭','上半身2']:
        rig.pose.bones[name].keyframe_insert('rotation_quaternion',frame=frame)
track=rig.animation_data.nla_tracks.new();track.name='Idle';track.strips.new('Idle',1,idle)
rig.animation_data.action=None

bpy.context.scene.frame_set(1)
for ob in meshes:
    if not ob.data.shape_keys:continue
    keys=ob.data.shape_keys
    # Facial control remains available even with the default standing animation.
    if 'まばたき' in keys.key_blocks:
        blink=keys.key_blocks['まばたき']
        for frame,value in [(1,0),(58,0),(61,1),(64,0),(121,0)]:
            blink.value=value;blink.keyframe_insert('value',frame=frame)
        action=keys.animation_data.action;action.name='Idle_Facial'
        track=keys.animation_data.nla_tracks.new();track.name='Idle'
        track.strips.new('Idle',1,action);keys.animation_data.action=None

bpy.context.scene.frame_start=1;bpy.context.scene.frame_end=121;bpy.context.scene.render.fps=30
bpy.context.scene.frame_set(1)
bpy.context.view_layer.update()
for ob in bpy.context.scene.objects:ob.select_set(False)
for ob in meshes+[rig]:ob.select_set(True)
bpy.context.view_layer.objects.active=rig
out=PROJECT/'assets/characters/kirito';out.mkdir(parents=True,exist_ok=True)
(out/'.gitignore').write_text('*.glb\n*.import\n',encoding='utf-8')
bpy.ops.file.pack_all()
bpy.ops.wm.save_as_mainfile(filepath=str(HERE/'Kirito_Godot.blend'))
dest=out/'Kirito.glb'
bpy.ops.export_scene.gltf(filepath=str(dest),export_format='GLB',use_selection=True,
    export_yup=True,export_apply=False,export_skins=True,export_morph=True,
    export_animations=True,export_animation_mode='NLA_TRACKS',export_force_sampling=True,
    export_def_bones=False,export_all_influences=False,export_extras=True,
    export_cameras=False,export_lights=False)
report['glb']=str(dest);report['sha256']=hashlib.sha256(dest.read_bytes()).hexdigest()
(HERE/'blender_import_validation.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
print('KIRITO_EXPORT_OK '+json.dumps({k:v for k,v in report.items() if k not in ['materials','shape_keys']},ensure_ascii=False),flush=True)
