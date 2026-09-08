"""Bind the authored horse meshes and author a grounded four-beat walk.

Run with Blender 5.2: blender -b -t 6 --python rig_horses.py -- --render
Source masters are read only. New armatures, actions and exports live in Rigged/.
"""
import argparse
import json
import math
import sys
from pathlib import Path
import bpy
from mathutils import Matrix, Vector, Quaternion

OUT=Path(__file__).resolve().parent
DEST=OUT/'Rigged';DEST.mkdir(exist_ok=True)
(DEST/'Previews').mkdir(exist_ok=True)
parser=argparse.ArgumentParser();parser.add_argument('--render',action='store_true');parser.add_argument('--samples',type=int,default=24)
opt=parser.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
bpy.ops.wm.open_mainfile(filepath=str(OUT/'MedievalLife_Masters.blend'))
scene=bpy.context.scene;scene.render.fps=30
COLL=bpy.data.collections.new('60 | bound horses');scene.collection.children.link(COLL)
for c in bpy.data.collections:
    if c.name.startswith('00 |') or c.name.startswith('50 |'):
        c.hide_render=True
        if c.name.startswith('00 |'):c.hide_viewport=False

BONES={
    'root':((0,0,0),(0,0,.35),None),
    'body':((0,.12,1.10),(0,.12,1.78),'root'),
    'neck':((0,-.59,1.40),(0,-1.12,2.32),'body'),
    'head':((0,-1.12,2.24),(0,-1.74,1.94),'neck'),
    'tail_base':((0,1.02,1.60),(.06,1.29,1.24),'body'),
    'tail_tip':((.06,1.29,1.24),(.12,1.30,.58),'tail_base'),
}
LEGS={}
for side,suffix in ((-1,'L'),(1,'R')):
    for front,prefix in ((True,'F'),(False,'H')):
        x=side*(.27 if front else .30)
        points=[(x,-.57,1.37),(x,-.69,.84),(x,-.64,.40),(x,-.70,.12)] if front else [
            (x,.72,1.39),(x,.51,.92),(x,.94,.54),(x,.78,.13)]
        key=prefix+suffix;LEGS[key]=[Vector(p) for p in points]
        for i,part in enumerate(('upper','lower','foot')):
            BONES[key+'_'+part]=(points[i],points[i+1],'body' if i==0 else key+'_'+('upper','lower')[i-1])

def connected_components(mesh):
    adjacency=[[] for _ in mesh.vertices]
    for edge in mesh.edges:
        a,b=edge.vertices;adjacency[a].append(b);adjacency[b].append(a)
    seen=set();result=[]
    for vertex in mesh.vertices:
        if vertex.index in seen:continue
        stack=[vertex.index];seen.add(vertex.index);component=[]
        while stack:
            i=stack.pop();component.append(i)
            for j in adjacency[i]:
                if j not in seen:seen.add(j);stack.append(j)
        result.append(component)
    return result

def segment_distance(p,a,b):
    ab=b-a;t=max(0,min(1,(p-a).dot(ab)/ab.length_squared))
    return (p-(a+ab*t)).length

def classify(ob,ids):
    points=[ob.data.vertices[i].co for i in ids]
    centre=sum(points,Vector())/len(points)
    x,y,z=centre;layer=ob.get('layer','structure')
    if layer=='attachments':
        length_y=max(p.y for p in points)-min(p.y for p in points)
        if length_y>1.8:return 'rein_blend'
        return 'head' if y<-1.12 and z>1.7 else 'body'
    if abs(x)>.17 and z<1.42:
        key=('F' if y<0 else 'H')+('L' if x<0 else 'R')
        chain=LEGS[key]
        idx=min(range(3),key=lambda i:segment_distance(centre,chain[i],chain[i+1]))
        return key+'_'+('upper','lower','foot')[idx]
    if y>1.06 and z>.3:
        return min(('tail_base','tail_tip'),key=lambda b:segment_distance(centre,Vector(BONES[b][0]),Vector(BONES[b][1])))
    if y<-1.1 and z>1.8:return 'head'
    if y<-.36 and z>1.62:return 'neck_blend'
    return 'body'

def create_rig(coat):
    source=bpy.data.objects['horse_'+coat]
    data=bpy.data.armatures.new('Horse_'+coat+'_Skeleton')
    rig=bpy.data.objects.new('Horse_'+coat+'_Rig',data);COLL.objects.link(rig)
    rig.show_in_front=True;rig['source_module']='horse_'+coat
    rig['front']='-Y';rig['units']='meters';rig['gait_speed_mps']=.6
    bpy.ops.object.select_all(action='DESELECT');rig.select_set(True);bpy.context.view_layer.objects.active=rig
    bpy.ops.object.mode_set(mode='EDIT')
    for name,(head,tail,parent) in BONES.items():
        bone=data.edit_bones.new(name);bone.head=head;bone.tail=tail;bone.use_deform=True
        if parent:bone.parent=data.edit_bones[parent]
    bpy.ops.object.mode_set(mode='OBJECT')
    assignments=[]
    for src in source.children:
        if src.type!='MESH':continue
        ob=src.copy();ob.data=src.data.copy();COLL.objects.link(ob);ob.parent=rig
        ob.name='horse_'+coat+'_skinned__'+src.get('layer','structure')
        ob.hide_render=False;ob.hide_viewport=False
        for group in list(ob.vertex_groups):ob.vertex_groups.remove(group)
        groups={name:ob.vertex_groups.new(name=name) for name in BONES}
        counts={}
        for component in connected_components(ob.data):
            group=classify(ob,component);counts[group]=counts.get(group,0)+len(component)
            for index in component:
                v=ob.data.vertices[index].co
                if group=='neck_blend':
                    blend=max(0,min(1,(v.z-1.44)/.56))
                    groups['neck'].add([index],blend,'REPLACE');groups['body'].add([index],1-blend,'REPLACE')
                elif group=='rein_blend':
                    blend=max(0,min(1,(-v.y+.8)/2.25))
                    groups['head'].add([index],blend,'REPLACE');groups['body'].add([index],1-blend,'REPLACE')
                else:groups[group].add([index],1,'REPLACE')
        modifier=ob.modifiers.new('Horse skin','ARMATURE');modifier.object=rig
        assignments.append({'mesh':ob.name,'vertices':len(ob.data.vertices),'classified_vertices':counts})
    return rig,assignments

def bone_matrix(a,b):
    y=(b-a).normalized();x=Vector((1,0,0))
    # All limb bends are authored in the YZ plane. Tail bones may lean sideways.
    x=(x-y*x.dot(y)).normalized();z=x.cross(y)
    m=Matrix((x,y,z)).transposed().to_4x4();m.translation=a
    return m

def pivot_rotation(pivot,axis,angle):
    return Matrix.Translation(pivot)@Quaternion(axis,angle).to_matrix().to_4x4()@Matrix.Translation(-Vector(pivot))

def solve_knee(hip,ankle,knee_rest,l1,l2):
    delta=ankle-hip;distance=delta.length
    assert abs(delta.x)<1e-5
    reach=min(max(distance,abs(l1-l2)+1e-5),l1+l2-1e-5)
    direction=delta.normalized();ankle=hip+direction*reach
    along=(l1*l1-l2*l2+reach*reach)/(2*reach)
    across=math.sqrt(max(0,l1*l1-along*along))
    perpendicular=Vector((0,-direction.z,direction.y))
    candidates=[hip+direction*along+perpendicular*across,hip+direction*along-perpendicular*across]
    knee=min(candidates,key=lambda p:(p-knee_rest).length)
    return knee,ankle,abs(distance-reach)

def set_pose(rig,t,walking):
    drop=(-.10+.012*math.cos(t*4*math.pi)) if walking else .005*math.sin(t*2*math.pi)
    body=Matrix.Translation((0,0,drop))
    poses={'root':rig.data.bones['root'].matrix_local.copy(),
           'body':body@rig.data.bones['body'].matrix_local}
    neck=body@pivot_rotation(BONES['neck'][0],(1,0,0),math.radians(1.6 if walking else 2.2)*math.sin(2*math.pi*t))
    poses['neck']=neck@rig.data.bones['neck'].matrix_local
    head=neck@pivot_rotation(BONES['head'][0],(1,0,0),math.radians(1.4)*math.sin(2*math.pi*t+.8))
    poses['head']=head@rig.data.bones['head'].matrix_local
    tail=body@pivot_rotation(BONES['tail_base'][0],(0,1,0),math.radians(5)*math.sin(2*math.pi*t+.5))
    poses['tail_base']=tail@rig.data.bones['tail_base'].matrix_local
    poses['tail_tip']=tail@pivot_rotation(BONES['tail_tip'][0],(0,1,0),math.radians(8)*math.sin(2*math.pi*t+1))@rig.data.bones['tail_tip'].matrix_local
    # Four-beat walk: left hind, left fore, right hind, right fore.
    phases={'HL':0,'FL':.25,'HR':.5,'FR':.75};samples=[]
    for key,chain in LEGS.items():
        hip,knee_rest,ankle_rest,hoof_rest=[p.copy() for p in chain]
        hip.z+=drop;knee_rest.z+=drop
        phase=(t+phases[key])%1
        stance=not walking or phase<.62
        if not walking:travel=0;lift=0
        # The planted foot travels opposite the actor at exactly 0.6 m/s.
        # 0.72 m is the complete world stride; only 62% occurs in stance.
        elif stance:travel=-.72*.62/2+.72*phase;lift=0
        else:
            swing=(phase-.62)/.38;travel=.72*.62/2*math.cos(math.pi*swing);lift=.15*math.sin(math.pi*swing)
        hoof=hoof_rest+Vector((0,travel,lift))
        ankle=hoof+(ankle_rest-hoof_rest)
        l1=(chain[1]-chain[0]).length;l2=(chain[2]-chain[1]).length
        knee,ankle,reach_error=solve_knee(hip,ankle,knee_rest,l1,l2)
        hoof=ankle+(hoof_rest-ankle_rest)
        for name,a,b in [(key+'_upper',hip,knee),(key+'_lower',knee,ankle),(key+'_foot',ankle,hoof)]:
            poses[name]=bone_matrix(a,b)
        samples.append({'leg':key,'phase':phase,'stance':stance,'hoof_target_z_m':hoof.z,
                        'ground_error_m':hoof.z-hoof_rest.z if stance else 0,'reach_clamp_m':reach_error})
    # Derive local pose matrices explicitly; no dependency on evaluation order.
    for name in BONES:
        bone=rig.data.bones[name];pose=rig.pose.bones[name];parent=BONES[name][2]
        basis=bone.matrix_local.inverted()@(rig.data.bones[parent].matrix_local@poses[parent].inverted() if parent else Matrix.Identity(4))@poses[name]
        pose.rotation_mode='QUATERNION';pose.matrix_basis=basis
    bpy.context.view_layer.update()
    return samples

rigs=[];report={'status':'running','bones_per_horse':len(BONES),'clips':[],'horses':[],
    'walk':{'fps':30,'start_frame':1,'end_frame':37,'cycle_seconds':1.2,'stride_m':.72,'root_motion':False,'expected_actor_speed_mps':.6,
            'stance_fraction':.62,'footfall_order':['HL','FL','HR','FR']},
    'limitations':['Procedurally authored walk and idle; no turn, trot, hitch or cart animation yet.',
                   'Horse rigs are separate exports; live game integration must be validated independently.']}
for coat in ('bay','grey'):
    rig,assignments=create_rig(coat);rigs.append(rig)
    rig.animation_data_create();actions=[]
    for clip,end,walking in [('Idle',61,False),('Walk',37,True)]:
        action=bpy.data.actions.new('Horse_'+coat+'_'+clip);rig.animation_data.action=action
        action.use_fake_user=True;contacts=[]
        for frame in range(1,end+1):
            scene.frame_set(frame);samples=set_pose(rig,(frame-1)/(end-1),walking)
            for sample in samples:contacts.append(dict(frame=frame,**sample))
            for pose in rig.pose.bones:
                pose.keyframe_insert('location',frame=frame,group=pose.name)
                pose.keyframe_insert('rotation_quaternion',frame=frame,group=pose.name)
                pose.keyframe_insert('scale',frame=frame,group=pose.name)
        # Keep only this rig selected: no presentation cameras/ground in exports.
        scene.frame_start=1;scene.frame_end=end;scene.frame_set(1)
        bpy.ops.object.select_all(action='DESELECT');rig.select_set(True)
        for child in rig.children:child.select_set(True)
        bpy.context.view_layer.objects.active=rig
        fbx=DEST/f'horse_{coat}_{clip.lower()}.fbx'
        bpy.ops.export_scene.fbx(filepath=str(fbx),use_selection=True,object_types={'ARMATURE','MESH'},
            add_leaf_bones=False,bake_anim=True,bake_anim_use_all_actions=False,bake_anim_use_nla_strips=False,
            bake_anim_simplify_factor=0,apply_scale_options='FBX_SCALE_UNITS',axis_forward='-Y',axis_up='Z',
            use_mesh_modifiers=True,path_mode='AUTO')
        glb=DEST/f'horse_{coat}_{clip.lower()}.glb'
        bpy.ops.export_scene.gltf(filepath=str(glb),export_format='GLB',use_selection=True,export_apply=True,
            export_extras=True,export_animations=True,export_animation_mode='ACTIVE_ACTIONS',
            export_skins=True,export_frame_range=True,export_force_sampling=True,
            export_cameras=False,export_lights=False)
        max_error=max(abs(s['ground_error_m']) for s in contacts)
        max_reach=max(s['reach_clamp_m'] for s in contacts)
        assert max_error<.002 and max_reach<.002,(coat,clip,max_error,max_reach)
        report['clips'].append({'horse':coat,'clip':clip,'fbx':fbx.name,'glb':glb.name,
             'max_stance_target_error_m':max_error,'max_reach_clamp_m':max_reach,'samples':contacts})
        actions.append(action)
    rig.animation_data.action=actions[-1]
    report['horses'].append({'id':'horse_'+coat,'vertex_binding':assignments,'actions':[a.name for a in actions]})

# A native Blender studio preview of the actual bound meshes and authored poses.
rigs[0].location=(-1.22,0,0);rigs[1].location=(1.22,.5,0)
scene.frame_start=1;scene.frame_end=37;scene.frame_set(11)
cam=scene.camera;target=Vector((0,-.20,1.2))
cam.location=target+Vector((6,-9,5));cam.rotation_euler=(target-cam.location).to_track_quat('-Z','Y').to_euler()
cam.data.type='ORTHO';cam.data.ortho_scale=5.8
scene.cycles.samples=opt.samples;scene.render.resolution_x=1500;scene.render.resolution_y=1100
scene.render.resolution_percentage=100
for ob in bpy.data.objects:
    if ob.type=='LIGHT':ob.rotation_euler=(target-ob.location).to_track_quat('-Z','Y').to_euler()
report['status']='passed_source_binding_and_authored_kinematics'
(DEST/'rig_report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
bpy.ops.wm.save_as_mainfile(filepath=str(DEST/'Medieval_Horses_Rigged.blend'))
if opt.render:
    for frame in (1,7,13,19,25,31):
        scene.frame_set(frame);scene.render.filepath=str(DEST/'Previews'/f'horse_walk_{frame:03d}.png')
        bpy.ops.render.render(write_still=True)
        print('HORSE_POSE_RENDERED',frame,flush=True)
print('HORSE_RIG_COMPLETE',len(rigs),len(BONES),flush=True)
