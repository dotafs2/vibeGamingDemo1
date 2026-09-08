"""Author bound gatekeeper, royal guard and carter from the reviewed masters.

Blender 5.2: -b -t 6 --python-exit-code 1 --python rig_people.py -- --render
The source masters and all original static modules remain read-only.
"""
import argparse
import json
import math
import sys
from pathlib import Path
import bpy
from mathutils import Matrix, Vector, Quaternion

BASE=Path(__file__).resolve().parent
sys.path.insert(0,str(BASE))
from meshkit import Geo, material, COLORS
OUT=BASE/'PeopleRigged'; OUT.mkdir(exist_ok=True); (OUT/'Previews').mkdir(exist_ok=True)
parser=argparse.ArgumentParser(); parser.add_argument('--render',action='store_true')
opt=parser.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
bpy.ops.wm.open_mainfile(filepath=str(BASE/'MedievalLife_Masters.blend'))
scene=bpy.context.scene;scene.render.fps=30
coll=bpy.data.collections.new('70 | bound service residents');scene.collection.children.link(coll)
for collection in bpy.data.collections:
    if collection.name.startswith(('00 |','50 |')):
        collection.hide_render=True;collection.hide_viewport=False

BONES={'root':((0,0,0),(0,0,.3),None),
       'pelvis':((0,0,.9),(0,0,1.06),'root'),
       'spine':((0,0,1.06),(0,0,1.44),'pelvis'),
       'neck':((0,0,1.44),(0,0,1.60),'spine'),
       'head':((0,0,1.60),(0,-.01,1.87),'neck')}
LEGS={};ARMS={}
for side,tag in ((-1,'l'),(1,'r')):
    leg=[Vector(p) for p in ((side*.12,0,.9),(side*.12,.015,.49),(side*.12,-.015,.12),(side*.12,-.075,.025))]
    arm=[Vector(p) for p in ((side*.29,0,1.38),(side*.39,-.045,1.13),(side*.38,-.16,.96),(side*.38,-.17,.87))]
    LEGS[tag]=leg;ARMS[tag]=arm
    for chain,parts,parent in ((leg,('thigh','shin','foot'),'pelvis'),(arm,('upper_arm','forearm','hand'),'spine')):
        for i,name in enumerate(parts):BONES[name+'_'+tag]=(chain[i],chain[i+1],parent if i==0 else parts[i-1]+'_'+tag)

def components(mesh):
    adjacency=[[] for _ in mesh.vertices]
    for e in mesh.edges:
        a,b=e.vertices;adjacency[a].append(b);adjacency[b].append(a)
    seen=set();result=[]
    for v in mesh.vertices:
        if v.index in seen:continue
        stack=[v.index];seen.add(v.index);piece=[]
        while stack:
            i=stack.pop();piece.append(i)
            for j in adjacency[i]:
                if j not in seen:seen.add(j);stack.append(j)
        result.append(piece)
    return result

def segment_distance(p,a,b):
    direction=b-a;alpha=max(0,min(1,(p-a).dot(direction)/direction.length_squared))
    return (p-a-direction*alpha).length

def bone_matrix(a,b):
    y=(b-a).normalized();x=Vector((1,0,0));x=(x-y*x.dot(y)).normalized();z=x.cross(y)
    matrix=Matrix((x,y,z)).transposed().to_4x4();matrix.translation=a
    return matrix

def solve_joint(a,c,rest_joint,length1,length2):
    delta=c-a;distance=delta.length
    reach=min(max(distance,abs(length1-length2)+1e-5),length1+length2-1e-5)
    direction=delta.normalized();c=a+direction*reach
    along=(length1**2-length2**2+reach**2)/(2*reach)
    radius=math.sqrt(max(0,length1**2-along**2))
    pole=rest_joint-a-direction*(rest_joint-a).dot(direction)
    if pole.length<1e-5:pole=Vector((0,-direction.z,direction.y))
    return a+direction*along+pole.normalized()*radius,c,abs(distance-reach)

def rigid_rotation(pivot,axis,radians):
    return Matrix.Translation(Vector(pivot))@Quaternion(axis,radians).to_matrix().to_4x4()@Matrix.Translation(-Vector(pivot))

def classify(ob,indices):
    centre=sum((ob.data.vertices[i].co for i in indices),Vector())/len(indices)
    x,y,z=centre;tag='l' if x<0 else 'r'
    if z>1.58:return 'head'
    if z>1.46 and abs(x)<.14:return 'neck'
    if abs(x)>.275 and .8<z<1.51:
        chain=ARMS[tag];i=min(range(3),key=lambda k:segment_distance(centre,chain[k],chain[k+1]))
        return ('upper_arm','forearm','hand')[i]+'_'+tag
    if z<.77:
        chain=LEGS[tag];i=min(range(3),key=lambda k:segment_distance(centre,chain[k],chain[k+1]))
        return ('thigh','shin','foot')[i]+'_'+tag
    return 'torso_blend'

def make_material(role,hex_color):
    COLORS[role]=hex_color
    return material(role)

def create_person(identity,source_id):
    data=bpy.data.armatures.new(identity+'_Skeleton');rig=bpy.data.objects.new(identity+'_Rig',data);coll.objects.link(rig)
    rig['identity']=identity;rig['front']='-Y';rig['walk_speed_mps']=.9
    bpy.ops.object.select_all(action='DESELECT');rig.select_set(True);bpy.context.view_layer.objects.active=rig
    bpy.ops.object.mode_set(mode='EDIT')
    for name,(head,tail,parent) in BONES.items():
        b=data.edit_bones.new(name);b.head=head;b.tail=tail
        if parent:b.parent=data.edit_bones[parent]
    bpy.ops.object.mode_set(mode='OBJECT')
    source=bpy.data.objects[source_id];specs=[]
    for ob in source.children:
        if ob.type=='MESH':specs.append((ob,ob.get('layer','structure'),None,Vector()))
    if identity in ('gatekeeper','royal_guard'):
        gear=[('guard_helmet','helmet','head',Vector((0,-.008,1.89))),
              ('guard_spear','spear','hand_r',Vector((.38,-.17,.02)))]
        if identity=='royal_guard':gear.append(('guard_shield','shield','hand_l',Vector((-.38,-.265,.48))))
        for source_gear,key,bone,offset in gear:
            for ob in bpy.data.objects[source_gear].children:
                if ob.type=='MESH':specs.append((ob,key+'_'+ob.get('layer','structure'),bone,offset))
    meshes=[];binding=[]
    for src,key,forced,offset in specs:
        ob=src.copy();ob.data=src.data.copy();coll.objects.link(ob);ob.parent=rig
        ob.name=identity+'__'+key;ob['layer_key']=key;ob.hide_render=False;ob.hide_viewport=False
        # Bake module placement into vertices so every layer has one rig origin.
        for vertex in ob.data.vertices:vertex.co+=offset
        for group in list(ob.vertex_groups):ob.vertex_groups.remove(group)
        groups={name:ob.vertex_groups.new(name=name) for name in BONES}
        histogram={}
        for piece in components(ob.data):
            key_bone=forced or classify(ob,piece);histogram[key_bone]=histogram.get(key_bone,0)+len(piece)
            for i in piece:
                if key_bone=='torso_blend':
                    weight=max(0,min(1,(ob.data.vertices[i].co.z-1.01)/.34))
                    groups['spine'].add([i],weight,'REPLACE');groups['pelvis'].add([i],1-weight,'REPLACE')
                else:groups[key_bone].add([i],1,'REPLACE')
        skin=ob.modifiers.new('Resident skin','ARMATURE');skin.object=rig
        if identity=='gatekeeper':
            for i,mat in enumerate(ob.data.materials):
                if mat.get('semantic_slot')=='blue':ob.data.materials[i]=make_material('gate_olive','788364')
                elif mat.get('semantic_slot')=='gold':ob.data.materials[i]=make_material('gate_brass','B39561')
        meshes.append(ob);binding.append({'layer':key,'mesh':ob.name,'vertices':len(ob.data.vertices),'groups':histogram})
    if identity=='carter':
        # A worn flat cap and short grey beard distinguish the older carter.
        g=Geo();g.ellipsoid((0,.012,1.90),(.183,.168,.085),'leather_light',12,6)
        g.box((0,-.145,1.862),(.26,.145,.027),'leather')
        g.loft([((0,-.08,1.655),.108,.085),((0,-.108,1.572),.071,.047)],'grey',10)
        ob=g.object(identity+'__wardrobe',coll,rig,'wardrobe',bevel=.005);ob['layer_key']='wardrobe'
        group=ob.vertex_groups.new(name='head');group.add(list(range(len(ob.data.vertices))),1,'REPLACE')
        skin=ob.modifiers.new('Resident skin','ARMATURE');skin.object=rig;meshes.append(ob)
        binding.append({'layer':'wardrobe','mesh':ob.name,'vertices':len(ob.data.vertices),'groups':{'head':len(ob.data.vertices)}})
    return rig,meshes,binding

def set_pose(rig,t,walking):
    identity=rig['identity'];guard=identity!='carter'
    drop=(-.07+.008*math.cos(4*math.pi*t)) if walking else -.008+.003*math.sin(2*math.pi*t)
    body=Matrix.Translation((0,0,drop));poses={'root':rig.data.bones['root'].matrix_local.copy()}
    for name in ('pelvis','spine','neck','head'):poses[name]=body@rig.data.bones[name].matrix_local
    yaw=math.radians(2 if walking else 5)*math.sin(2*math.pi*t)
    poses['head']=body@rigid_rotation(BONES['head'][0],(0,0,1),yaw)@rig.data.bones['head'].matrix_local
    contacts=[]
    for tag,chain in LEGS.items():
        hip,knee,ankle,foot=[p.copy() for p in chain];hip.z+=drop;knee.z+=drop
        phase=(t+(0 if tag=='l' else .5))%1;stance=not walking or phase<.62
        if not walking:travel=lift=0
        elif stance:travel=-.9*.62/2+.9*phase;lift=0
        else:
            u=(phase-.62)/.38;travel=.9*.62/2*math.cos(math.pi*u);lift=.11*math.sin(math.pi*u)
        # Character faces -Y: the knee pole must be forward, even though the
        # nearly straight authoring pose puts the rest knee slightly behind.
        pole=hip+Vector((0,-.4,-.35))
        ankle+=Vector((0,travel,lift));knee,ankle,error=solve_joint(hip,ankle,pole,(chain[1]-chain[0]).length,(chain[2]-chain[1]).length)
        foot=ankle+(chain[3]-chain[2])
        for name,a,b in [('thigh_'+tag,hip,knee),('shin_'+tag,knee,ankle),('foot_'+tag,ankle,foot)]:poses[name]=bone_matrix(a,b)
        contacts.append({'leg':tag,'phase':phase,'stance':stance,'reach_error_m':error})
    for tag,chain in ARMS.items():
        shoulder,elbow,wrist,hand=[p.copy() for p in chain];shoulder.z+=drop;elbow.z+=drop
        if guard:
            wrist+=Vector((0,-.025 if tag=='r' else -.08,.12 if tag=='r' else .08))
            wrist.z+=.007*math.sin(4*math.pi*t) if walking else 0
        else:
            swing=.11*math.sin(2*math.pi*t+(0 if tag=='r' else math.pi)) if walking else 0
            wrist.y+=swing;wrist.z+=drop+.04
        elbow,wrist,error=solve_joint(shoulder,wrist,elbow,(chain[1]-chain[0]).length,(chain[2]-chain[1]).length)
        hand=wrist+(chain[3]-chain[2])
        for name,a,b in [('upper_arm_'+tag,shoulder,elbow),('forearm_'+tag,elbow,wrist),('hand_'+tag,wrist,hand)]:poses[name]=bone_matrix(a,b)
    for name in BONES:
        bone=rig.data.bones[name];pose=rig.pose.bones[name];parent=BONES[name][2]
        pose.rotation_mode='QUATERNION'
        pose.matrix_basis=bone.matrix_local.inverted()@(rig.data.bones[parent].matrix_local@poses[parent].inverted() if parent else Matrix.Identity(4))@poses[name]
    bpy.context.view_layer.update()
    return contacts

report={'status':'running','bones':{name:{'parent':parent} for name,(_,_,parent) in BONES.items()},'people':[],
        'walk_speed_mps':.9,'walk_cycle_seconds':1,'fps':30,'root_motion':False,
        'limitations':['No seated driving, combat, facial speech or IK terrain adjustment yet.','Source previews do not prove live NPC integration.']}
rigs=[]
for identity,source in [('gatekeeper','royal_guard_body'),('royal_guard','royal_guard_body'),('carter','carter_body')]:
    rig,meshes,binding=create_person(identity,source);rig.animation_data_create();rigs.append(rig)
    row={'id':identity,'layers':binding,'clips':{}}
    for clip,end,walking in [('Idle',61,False),('Walk',31,True)]:
        action=bpy.data.actions.new(identity+'_'+clip);action.use_fake_user=True;rig.animation_data.action=action
        errors=[]
        for frame in range(1,end+1):
            scene.frame_set(frame);errors.extend(set_pose(rig,(frame-1)/(end-1),walking))
            for pose in rig.pose.bones:
                for key in ('location','rotation_quaternion','scale'):pose.keyframe_insert(key,frame=frame,group=pose.name)
        max_reach=max(s['reach_error_m'] for s in errors)
        assert max_reach<.002,(identity,clip,max_reach)
        scene.frame_start=1;scene.frame_end=end;scene.frame_set(1);scene.name=identity+'_'+clip
        bpy.ops.object.select_all(action='DESELECT');rig.select_set(True)
        for ob in meshes:ob.select_set(True)
        bpy.context.view_layer.objects.active=rig
        fbx=OUT/(identity+'_'+clip.lower()+'.fbx');glb=fbx.with_suffix('.glb')
        bpy.ops.export_scene.fbx(filepath=str(fbx),use_selection=True,object_types={'ARMATURE','MESH'},add_leaf_bones=False,
            bake_anim=True,bake_anim_use_all_actions=False,bake_anim_use_nla_strips=False,bake_anim_simplify_factor=0,
            apply_scale_options='FBX_SCALE_UNITS',axis_forward='-Y',axis_up='Z',use_mesh_modifiers=True,mesh_smooth_type='FACE')
        bpy.ops.export_scene.gltf(filepath=str(glb),export_format='GLB',use_selection=True,export_apply=True,export_extras=True,
            export_animations=True,export_animation_mode='ACTIVE_ACTIONS',export_skins=True,export_frame_range=True,export_force_sampling=True)
        row['clips'][clip]={'fbx':fbx.name,'glb':glb.name,'frames':end,'seconds':(end-1)/30,'max_reach_error_m':max_reach}
    report['people'].append(row)
for i,rig in enumerate(rigs):rig.location=((i-1)*1.50,0,0)
scene.name='Service residents';scene.frame_start=1;scene.frame_end=31;scene.frame_set(7)
cam=scene.camera;target=Vector((0,0,1.22));cam.location=target+Vector((5,-10,4))
cam.rotation_euler=(target-cam.location).to_track_quat('-Z','Y').to_euler();cam.data.type='ORTHO';cam.data.ortho_scale=5.9
scene.cycles.samples=24;scene.render.resolution_x=1600;scene.render.resolution_y=1050;scene.render.resolution_percentage=100
report['status']='source_rigs_and_authored_actions_created'
(OUT/'people_rig_report.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'Medieval_Service_Residents_Rigged.blend'))
if opt.render:
    for frame in (1,7,16,23):
        scene.frame_set(frame);scene.render.filepath=str(OUT/'Previews'/f'service_walk_{frame:03d}.png')
        bpy.ops.render.render(write_still=True)
        print('PEOPLE_POSE_RENDERED',frame,flush=True)
print('PEOPLE_RIGGED',len(rigs),len(BONES),flush=True)
