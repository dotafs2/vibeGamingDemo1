"""Original skinned anime-town residents, authored in metres in Blender.

No imported character geometry. Front=-Y; one identical named rig per resident.
Export self-contained FBX skeletal mesh plus authored idle/walk clips.
"""
import argparse
import json
import math
import sys
from pathlib import Path
import bpy
from mathutils import Vector, Matrix

OUT=Path(__file__).resolve().parent
for sub in ('Exports','Previews'): (OUT/sub).mkdir(parents=True,exist_ok=True)
parser=argparse.ArgumentParser();parser.add_argument('--render',action='store_true')
opt=parser.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
bpy.ops.wm.read_factory_settings(use_empty=True)
scene=bpy.context.scene;scene.unit_settings.system='METRIC';scene.render.fps=30
PALETTES={
 'Aileen':{'Skin':'F3CFB7','Hair':'7D4733','HairLight':'AC7150','Iris':'52775B','Cloth':'EFE3CC','Accent':'44675B','Leather':'704D38','Boot':'483D38','Metal':'B7A585'},
 'Takuma':{'Skin':'E3B494','Hair':'353846','HairLight':'626778','Iris':'666F84','Cloth':'BBC8D0','Accent':'465975','Leather':'895941','Boot':'36333A','Metal':'ABB5BE'},
 'Kashiwagi':{'Skin':'ECCCAB','Hair':'887353','HairLight':'B59D72','Iris':'71826B','Cloth':'D8D8B7','Accent':'65765D','Leather':'776044','Boot':'49473C','Metal':'B2AEA0'}
}
BASE={'White':'F5F1EA','Ink':'332E38','Mouth':'A76562'}

def linear(code):
    rgb=[int(code[i:i+2],16)/255 for i in (0,2,4)]
    return tuple(v/12.92 if v<=.04045 else ((v+.055)/1.055)**2.4 for v in rgb)

class Geo:
    def __init__(self):self.v=[];self.f=[];self.roles=[];self.weights=[]
    def mesh(self,verts,faces,role,bone):
        base=len(self.v);self.v.extend(tuple(v) for v in verts)
        self.f.extend(tuple(base+i for i in f) for f in faces);self.roles.extend([role]*len(faces))
        self.weights.extend([bone if isinstance(bone,dict) else {bone:1.} for _ in verts])
    def ellipsoid(self,p,s,role,bone,rings=12,sectors=20):
        verts=[]
        for j in range(rings+1):
            phi=math.pi*j/rings
            for i in range(sectors):
                a=2*math.pi*i/sectors
                verts.append((p[0]+s[0]*math.sin(phi)*math.cos(a),p[1]+s[1]*math.sin(phi)*math.sin(a),p[2]+s[2]*math.cos(phi)))
        faces=[]
        for j in range(rings):
            for i in range(sectors):faces.append((j*sectors+i,(j+1)*sectors+i,(j+1)*sectors+(i+1)%sectors,j*sectors+(i+1)%sectors))
        self.mesh(verts,faces,role,bone)
    def rings(self,rows,role,bone,sectors=24):
        verts=[]
        for z,xr,yr,y in rows:
            for i in range(sectors):
                a=i*2*math.pi/sectors;verts.append((xr*math.cos(a),y+yr*math.sin(a),z))
        faces=[tuple(range(sectors-1,-1,-1))]
        for j in range(len(rows)-1):
            for i in range(sectors):faces.append((j*sectors+i,j*sectors+(i+1)%sectors,(j+1)*sectors+(i+1)%sectors,(j+1)*sectors+i))
        faces.append(tuple(range((len(rows)-1)*sectors,len(rows)*sectors)))
        self.mesh(verts,faces,role,bone)
    def tube(self,a,b,r1,r2,role,bone,n=12):
        axis=(Vector(b)-Vector(a)).normalized();u=axis.cross(Vector((0,1,0))).normalized()
        if u.length<.1:u=axis.cross(Vector((1,0,0))).normalized()
        v=axis.cross(u).normalized();verts=[]
        for p,r in [(a,r1),(b,r2)]:
            for i in range(n):verts.append(Vector(p)+r*(math.cos(i*2*math.pi/n)*u+math.sin(i*2*math.pi/n)*v))
        faces=[tuple(range(n-1,-1,-1)),tuple(range(n,2*n))]+[(i,(i+1)%n,(i+1)%n+n,i+n) for i in range(n)]
        self.mesh(verts,faces,role,bone)
    def stroke(self,points,width,role,bone):
        for a,b in zip(points,points[1:]):self.tube(a,b,width,width,role,bone,6)
    def ribbon(self,points,widths,role,bone,depth=.006):
        verts=[]
        for p,w in zip(points,widths):verts.extend([(p[0]-w,p[1],p[2]),(p[0],p[1]-depth,p[2]+.004),(p[0]+w,p[1],p[2])])
        faces=[]
        for i in range(len(points)-1):
            k=i*3;j=k+3;faces.extend([(k,j,j+1,k+1),(k+1,j+1,j+2,k+2)])
        # Strands are closed, so profile/back views remain readable.
        count=len(verts);back=[(x,y+.009,z) for x,y,z in verts]
        closed=faces+[tuple(count+v for v in reversed(f)) for f in faces]
        for i in range(len(points)-1):
            for offset in (0,2):
                a=i*3+offset;b=(i+1)*3+offset;closed.append((a,a+count,b+count,b))
        self.mesh(verts+back,closed,role,bone)

def rig():
    data=bpy.data.armatures.new('AincradResidentRig');obj=bpy.data.objects.new('AincradResidentRig',data);scene.collection.objects.link(obj)
    bpy.context.view_layer.objects.active=obj;obj.select_set(True);bpy.ops.object.mode_set(mode='EDIT')
    rows=[('root',(0,0,0),(0,0,.2),None),('pelvis',(0,0,.87),(0,0,1.03),'root'),('spine',(0,0,1.03),(0,0,1.23),'pelvis'),('chest',(0,0,1.23),(0,0,1.41),'spine'),('neck',(0,0,1.41),(0,0,1.50),'chest'),('head',(0,0,1.50),(0,0,1.72),'neck')]
    for s,label in [(-1,'L'),(1,'R')]:
        rows += [('upperarm.'+label,(s*.18,0,1.38),(s*.36,0,1.16),'chest'),('forearm.'+label,(s*.36,0,1.16),(s*.46,-.01,.95),'upperarm.'+label),('hand.'+label,(s*.46,-.01,.95),(s*.49,-.02,.85),'forearm.'+label),('thigh.'+label,(s*.09,0,.91),(s*.10,-.025,.51),'pelvis'),('shin.'+label,(s*.10,-.025,.51),(s*.10,0,.105),'thigh.'+label),('foot.'+label,(s*.10,0,.105),(s*.10,-.14,.055),'shin.'+label)]
    for name,a,b,parent in rows:
        bone=data.edit_bones.new(name);bone.head=a;bone.tail=b
        if parent:bone.parent=data.edit_bones[parent]
    bpy.ops.object.mode_set(mode='OBJECT');obj.select_set(False)
    return obj

def character(name):
    g=Geo();female=name=='Aileen'
    # A seven-head silhouette with shoulders, fitted waist and full-length legs.
    g.rings([(.88,.148,.095,0),(.98,.137,.085,0),(1.08,.118,.072,0),(1.24,.158 if female else .182,.091,0),(1.36,.174 if female else .194,.081,0),(1.41,.12,.063,0)],'Accent',{'spine':.5,'chest':.5})
    g.tube((0,0,1.38),(0,0,1.52),.051,.039,'Skin','neck',20)
    # Shaped jaw instead of a sphere: narrow chin, cheekbones, smooth forehead.
    g.rings([(1.49,.018,.022,-.025),(1.515,.047,.042,-.016),(1.55,.078,.064,-.004),(1.59,.102,.080,0),(1.64,.106,.085,.008),(1.70,.092,.078,.013),(1.735,.06,.052,.012),(1.751,.007,.012,.01)],'Skin','head',32)
    for side in (-1,1):g.ellipsoid((side*.102,.005,1.587),(.019,.014,.032),'Skin','head',8,12)
    # Shallow anime eyes with iris rings, dark lashes, brow and highlights.
    for side in (-1,1):
        x=side*.042;z=1.608
        g.ellipsoid((x,-.075,z),(.033,.014,.0185),'White','head',10,24)
        g.ellipsoid((x,-.088,z-.001),(.0145,.0045,.016),'Iris','head',12,24)
        g.ellipsoid((x,-.092,z-.001),(.0058,.002,.011),'Ink','head',10,16)
        g.ellipsoid((x-.004,-.094,z+.006),(.0038,.001,.0044),'White','head',6,12)
        points=[(x+.034*math.cos(a),-.089,z+.019*math.sin(a)) for a in [0,.35,.7,1.1,1.5,1.9,2.4,2.8,math.pi]]
        g.stroke(points,.0026,'Ink','head')
        g.stroke([(x-.026,-.077,1.644),(x,-.083,1.649),(x+.026,-.077,1.645)],.003,'Hair','head')
    g.ellipsoid((0,-.078,1.574),(.007,.009,.012),'Skin','head',10,12)
    g.stroke([(-.017,-.062,1.54),(0,-.067,1.538),(.017,-.062,1.54)],.0016,'Mouth','head')
    # Hair crown is a single shaped cap, plus asymmetric grouped fringe.
    vertices=[];n=36;steps=10
    for row in range(steps+1):
        for i in range(n):
            angle=i*2*math.pi/n
            front=max(0,-math.sin(angle))
            end=2.12-.94*front
            phi=.015+(end-.015)*row/steps
            vertices.append((.113*math.sin(phi)*math.cos(angle),.011+.094*math.sin(phi)*math.sin(angle),1.646+.118*math.cos(phi)))
    faces=[]
    for row in range(steps):
        for i in range(n):faces.append((row*n+i,(row+1)*n+i,(row+1)*n+(i+1)%n,row*n+(i+1)%n))
    g.mesh(vertices,faces,'Hair','head')
    for index,x in enumerate([-.083,-.054,-.025,.005,.035,.065,.09]):
        tip=1.626+(.015 if index%2 else 0)+(.018 if index in (0,6) else 0)
        g.ribbon([(x*.6,-.041,1.747),(x,-.090,1.703),(x+.008,-.097,1.671),(x+.018,-.083,tip)],[.02,.026,.02,.001],'HairLight' if index==1 else 'Hair','head')
    for side in (-1,1):
        bottom=1.41 if female else 1.55
        g.ribbon([(side*.097,-.023,1.696),(side*.109,-.047,1.626),(side*.11,-.047,1.553),(side*.095,-.035,bottom)],[.02,.025,.018,.002],'Hair','head')
    if female:
        # Low ponytail and ribbon identify the innkeeper from the rear.
        g.ellipsoid((0,.097,1.515),(.069,.049,.115),'Hair','head')
        g.ellipsoid((0,.13,1.417),(.052,.034,.077),'Hair','head')
        g.tube((-.054,.116,1.555),(.054,.116,1.555),.009,.009,'Accent','head')
    elif name=='Kashiwagi':
        for i in range(5):g.ribbon([(-.08+i*.04,.066,1.70),(-.075+i*.04,.102,1.63),(-.067+i*.04,.105,1.54)],[.02,.025,.003],'HairLight' if i==3 else 'Hair','head')
    # Sleeves, forearms and small hands retain the same deform skeleton.
    for side,label in [(-1,'L'),(1,'R')]:
        a=(side*.17,0,1.38);elbow=(side*.36,0,1.16);wrist=(side*.46,-.01,.95)
        upper='upperarm.'+label;fore='forearm.'+label;hand='hand.'+label
        g.ellipsoid(a,(.07,.075,.075),'Cloth',upper)
        g.tube(a,elbow,.071,.051,'Cloth',upper,18)
        g.ellipsoid(elbow,(.052,.052,.052),'Cloth',fore,8,16)
        g.tube(elbow,wrist,.045,.026,'Skin',fore,16)
        cuff=Vector(elbow).lerp(Vector(wrist),.15)
        g.tube(elbow,cuff,.057,.054,'Cloth',fore,16)
        palm=Vector(wrist)+Vector((side*.015,-.005,-.037))
        g.ellipsoid(palm,(.029,.021,.048),'Skin',hand,12,16)
        for finger in range(4):
            x=palm.x+(finger-1.5)*.012
            a=(x,palm.y-.001,palm.z-.033);b=(x+side*.004,palm.y-.003,palm.z-.068+abs(finger-1.5)*.004)
            g.tube(a,b,.0057,.0047,'Skin',hand,8)
        g.ellipsoid(palm+Vector((-side*.025,-.005,.002)),(.013,.016,.028),'Skin',hand,8,12)
        thigh='thigh.'+label;shin='shin.'+label;foot='foot.'+label
        hip=(side*.09,0,.91);knee=(side*.10,-.025,.51);ankle=(side*.10,0,.105)
        g.tube(hip,knee,.078,.049,'Accent' if female else 'Cloth',thigh,18)
        g.ellipsoid(knee,(.05,.052,.052),'Accent' if female else 'Cloth',shin)
        g.tube(knee,ankle,.048,.029,'Accent' if female else 'Cloth',shin,18)
        g.tube((side*.1,0,.28),ankle,.047,.044,'Boot',shin,16)
        g.ellipsoid((side*.10,-.07,.062),(.051,.122,.056),'Boot',foot,10,20)
        g.ellipsoid((side*.10,-.072,.02),(.055,.126,.016),'Leather',foot,8,20)
    # Collar and waist belt have discrete trim and a buckle, at body scale.
    for side in (-1,1):
        g.mesh([(side*.045,-.059,1.43),(side*.107,-.079,1.37),(side*.050,-.105,1.30),(0,-.101,1.355)],[(0,1,2,3)],'Cloth','chest')
    g.rings([(1.035,.13,.086,0),(1.075,.13,.086,0)],'Leather','spine')
    g.ellipsoid((0,-.091,1.055),(.024,.007,.021),'Metal','spine',8,12)
    for z in (1.17,1.24,1.31):g.ellipsoid((0,-.096,z),(.005,.004,.005),'Metal','chest',6,10)
    if female:
        g.rings([(.36,.245,.135,0),(.56,.224,.12,0),(.78,.18,.105,0),(1.01,.145,.095,0)],'Accent','pelvis',32)
        # Front apron follows the outside of the skirt; no exposed underside.
        verts=[]
        for z,w,y in [(1.02,.11,-.098),(.80,.135,-.116),(.60,.165,-.13),(.40,.18,-.143)]:
            for i in range(9):
                x=(i/8*2-1)*w;verts.append((x,y+.023*(x/w)**2,z))
        g.mesh(verts,[(r*9+i,r*9+i+1,(r+1)*9+i+1,(r+1)*9+i) for r in range(3) for i in range(8)],'Cloth','pelvis')
    else:
        width=.13 if name=='Takuma' else .105
        g.mesh([(-width,-.101,1.31),(width,-.101,1.31),(width*.8,-.105,1.05),(-width*.8,-.105,1.05)],[(0,1,2,3)],'Leather','chest')
        g.mesh([(-.12,-.109,1.04),(.12,-.109,1.04),(.16,-.117,.70),(-.16,-.117,.70)],[(0,1,2,3)],'Leather','pelvis')
        for side in (-1,1):g.tube((side*.1,-.09,1.30),(side*.1,-.075,1.41),.009,.009,'Leather','chest',6)
        if name=='Kashiwagi':
            g.ellipsoid((.15,-.015,.94),(.045,.04,.07),'Leather','pelvis')
            g.tube((.16,-.015,.95),(.16,-.015,1.085),.01,.01,'Metal','pelvis',8)
    return g

def mesh_object(g,name,palette,armature):
    data=bpy.data.meshes.new(name);data.from_pydata(g.v,[],g.f);data.update()
    obj=bpy.data.objects.new('SK_'+name,data);scene.collection.objects.link(obj)
    for role in dict.fromkeys(g.roles):
        mat=bpy.data.materials.new('AC_'+name+'_'+role);mat.diffuse_color=(*linear(palette[role]),1);mat.use_nodes=True
        bsdf=mat.node_tree.nodes.get('Principled BSDF');bsdf.inputs['Base Color'].default_value=mat.diffuse_color;bsdf.inputs['Roughness'].default_value=.85
        data.materials.append(mat)
    indices={m.name.split('_')[-1]:i for i,m in enumerate(data.materials)}
    uv=data.uv_layers.new(name='UVMap')
    for poly,role in zip(data.polygons,g.roles):
        poly.material_index=indices[role];poly.use_smooth=True
        for idx in poly.loop_indices:
            p=data.vertices[data.loops[idx].vertex_index].co;uv.data[idx].uv=(p.x+.5,p.z/1.8)
    for bone in armature.data.bones:obj.vertex_groups.new(name=bone.name)
    for i,weights in enumerate(g.weights):
        for name,weight in weights.items():obj.vertex_groups[name].add([i],weight,'REPLACE')
    obj.parent=armature;mod=obj.modifiers.new('Resident skin','ARMATURE');mod.object=armature
    return obj

def animate(armature):
    actions=[]
    for action_name in ('Idle','Walk'):
        action=bpy.data.actions.new(action_name);armature.animation_data_create();armature.animation_data.action=action
        for frame in range(1,32,3):
            phase=(frame-1)/30*2*math.pi
            for bone in armature.pose.bones:
                bone.rotation_mode='XYZ';bone.rotation_euler=(0,0,0);bone.location=(0,0,0)
            if action_name=='Walk':
                for s,label in [(1,'L'),(-1,'R')]:
                    swing=math.sin(phase)*s
                    armature.pose.bones['thigh.'+label].rotation_euler.x=swing*.34
                    armature.pose.bones['shin.'+label].rotation_euler.x=-max(0,-swing)*.49
                    armature.pose.bones['upperarm.'+label].rotation_euler.x=-swing*.24
                    armature.pose.bones['forearm.'+label].rotation_euler.x=-.08-max(0,swing)*.09
                armature.pose.bones['pelvis'].location.y=.009*math.cos(phase*2)
            else:
                armature.pose.bones['chest'].rotation_euler.x=.012*math.sin(phase)
                armature.pose.bones['head'].rotation_euler.z=.015*math.sin(phase)
            for side,label in [(-1,'L'),(1,'R')]:
                bone=armature.pose.bones['upperarm.'+label]
                rest=bone.bone.matrix_local.to_3x3()
                relax=rest.inverted()@Matrix.Rotation(side*.44,3,'Y')@rest
                bone.rotation_euler=(relax@bone.rotation_euler.to_matrix()).to_euler()
            for bone in armature.pose.bones:
                bone.keyframe_insert('rotation_euler',frame=frame);bone.keyframe_insert('location',frame=frame)
        action.use_fake_user=True;actions.append(action)
    armature.animation_data.action=None
    return actions

characters=[];manifest={'front_axis':'Blender -Y; export and native bounds verified by importer','height_m':1.764,'palette':PALETTES,'assets':[]}
for name,colors in PALETTES.items():
    armature=rig();g=character(name);mesh=mesh_object(g,name,{**BASE,**colors},armature);actions=animate(armature)
    bpy.ops.object.select_all(action='DESELECT');mesh.select_set(True);armature.select_set(True);bpy.context.view_layer.objects.active=armature
    scene.frame_start=1;scene.frame_end=31;scene.frame_set(1)
    path=OUT/'Exports'/(name+'.fbx')
    bpy.ops.export_scene.fbx(filepath=str(path),use_selection=True,object_types={'MESH','ARMATURE'},add_leaf_bones=False,axis_forward='-Y',axis_up='Z',apply_unit_scale=True,bake_anim=False,mesh_smooth_type='FACE',use_mesh_modifiers=True)
    for label,action in zip(('Idle','Walk'),actions):
        armature.animation_data.action=action
        bpy.ops.export_scene.fbx(filepath=str(OUT/'Exports'/(name+'_'+label+'.fbx')),use_selection=True,object_types={'ARMATURE'},add_leaf_bones=False,axis_forward='-Y',axis_up='Z',apply_unit_scale=True,bake_anim=True,bake_anim_use_nla_strips=False,bake_anim_use_all_actions=False,bake_anim_force_startend_keying=True,bake_anim_simplify_factor=0)
    armature.animation_data.action=None
    manifest['assets'].append({'id':name,'fbx':path.relative_to(OUT).as_posix(),'vertices':len(g.v),'triangles':sum(len(f)-2 for f in g.f),'bones':len(armature.data.bones),'roles':list(dict.fromkeys(g.roles))})
    characters.append((name,armature,mesh))
    armature.hide_render=True;mesh.hide_render=True
    # Each FBX has its own pair of clips, never unrelated previous actions.
    for label,action in zip(('Idle','Walk'),actions):
        action.name=name+'_'+label
        track=armature.animation_data.nla_tracks.new();track.name=label;strip=track.strips.new(label,1,action);track.mute=True

(OUT/'manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf-8')
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'AincradCharacters.blend'))
if opt.render:
    for i,(name,armature,mesh) in enumerate(characters):
        armature.hide_render=False;mesh.hide_render=False;armature.location.x=(i-1)*.88
    world=bpy.data.worlds.new('Studio');world.use_nodes=True;world.node_tree.nodes['Background'].inputs[0].default_value=(.52,.60,.67,1);world.node_tree.nodes['Background'].inputs[1].default_value=.65;scene.world=world
    bpy.ops.mesh.primitive_plane_add(size=20,location=(0,0,-.012));ground=bpy.context.object
    mat=bpy.data.materials.new('Ground');mat.diffuse_color=(.22,.27,.28,1);ground.data.materials.append(mat)
    for loc,energy,size in [((-3,-5,5),650,5),((3,-1,3),350,4)]:
        ld=bpy.data.lights.new('Softbox','AREA');ld.energy=energy;ld.size=size;lo=bpy.data.objects.new('Softbox',ld);scene.collection.objects.link(lo);lo.location=loc;lo.rotation_euler=(Vector((0,0,1))-lo.location).to_track_quat('-Z','Y').to_euler()
    camera=bpy.data.objects.new('Camera',bpy.data.cameras.new('Camera'));scene.collection.objects.link(camera);scene.camera=camera
    camera.location=(0,-6,1.48);camera.rotation_euler=(Vector((0,0,.95))-camera.location).to_track_quat('-Z','Y').to_euler();camera.data.type='ORTHO';camera.data.ortho_scale=3.3
    scene.render.engine='CYCLES';scene.cycles.samples=32;scene.cycles.use_denoising=True;scene.render.threads_mode='FIXED';scene.render.threads=4
    scene.render.resolution_x=1500;scene.render.resolution_y=1050;scene.render.resolution_percentage=100;scene.render.filepath=str(OUT/'Previews/characters-source.png');bpy.ops.render.render(write_still=True)
print('AINCRAD_CHARACTERS_EXPORTED',len(characters),flush=True)
