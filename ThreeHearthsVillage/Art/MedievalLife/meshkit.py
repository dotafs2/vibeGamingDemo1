"""Small geometry helpers shared by the authored MedievalLife masters.

All dimensions are metres. Geometry, material slots and UVs survive GLB export;
presentation lights and ground never enter the asset exports.
"""
import math
import bpy
import bmesh
from mathutils import Vector

COLORS = {
    'oak': '936442', 'oak_light': 'C59460', 'oak_dark': '503B2D',
    'endgrain': 'C9A172', 'iron': '495258', 'steel': 'A2B7BD',
    'stone': '8D9996', 'stone_light': 'B8BEAD', 'stone_dark': '687775',
    'blue': '416B86', 'blue_light': '648B9D', 'gold': 'D6AC61',
    'linen': 'E8D6AE', 'leather': '604231', 'leather_light': 'AB7550',
    'bay': '976249', 'bay_light': 'B8805C', 'bay_dark': '704B3D',
    'grey': 'B9BDB0', 'grey_light': 'D5D8C9', 'grey_dark': '919B95',
    'mane': '3C3530', 'hoof': '44403B', 'eye': '252A2D',
    'skin': 'CE9976', 'skin_light': 'E6B792', 'hair': '665041',
    'hay': 'B8A768', 'hay_light': 'D6BD7B', 'water': '728E91',
    'roof': '627987', 'roof_light': '8A9BA0', 'moss': '899364',
    'earth': '757B62', 'flame': 'E7AB5A', 'red': 'A76452',
}
MATS = {}

def material(role):
    if role in MATS:
        return MATS[role]
    def lin(v):
        v /= 255
        return v / 12.92 if v <= .04045 else ((v + .055) / 1.055) ** 2.4
    color = COLORS[role]
    rgba = tuple(lin(int(color[i:i+2], 16)) for i in (0, 2, 4)) + (1,)
    mat = bpy.data.materials.new('Medieval | ' + role)
    mat.use_nodes = True
    mat.diffuse_color = rgba
    p = mat.node_tree.nodes.get('Principled BSDF')
    p.inputs['Base Color'].default_value = rgba
    p.inputs['Roughness'].default_value = .42 if role in ('iron', 'steel', 'gold') else .8
    p.inputs['Metallic'].default_value = .55 if role in ('iron', 'steel') else .3 if role == 'gold' else 0
    mat['semantic_slot'] = role
    MATS[role] = mat
    return mat

class Geo:
    def __init__(self):
        self.v, self.f, self.mi, self.roles = [], [], [], []

    def mesh(self, verts, faces, role):
        start = len(self.v)
        self.v.extend(tuple(v) for v in verts)
        self.f.extend(tuple(start+i for i in face) for face in faces)
        if role not in self.roles:
            self.roles.append(role)
        self.mi.extend([self.roles.index(role)] * len(faces))

    def box(self, p, size, role='oak', rot=None):
        verts = [Vector((x*size[0]/2, y*size[1]/2, z*size[2]/2))
                 for x,y,z in [(-1,-1,-1),(1,-1,-1),(1,1,-1),(-1,1,-1),
                               (-1,-1,1),(1,-1,1),(1,1,1),(-1,1,1)]]
        self.mesh([Vector(p) + (rot @ v if rot else v) for v in verts],
                  [(0,3,2,1),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)], role)

    def beam(self, a, b, width=.1, depth=None, role='oak'):
        a,b = Vector(a),Vector(b)
        self.box((a+b)/2, (width,depth or width,(b-a).length),role,
                 (b-a).to_track_quat('Z','Y').to_matrix())

    def loft(self, rings, role, n=12, axis=(0,0,1)):
        """(centre, radius_x, radius_y) rings perpendicular to local Z."""
        rot = Vector(axis).to_track_quat('Z','Y').to_matrix()
        vs = [Vector(p)+rot@Vector((rx*math.cos(2*math.pi*i/n),ry*math.sin(2*math.pi*i/n),0))
              for p,rx,ry in rings for i in range(n)]
        fs = [tuple(reversed(range(n))), tuple(range((len(rings)-1)*n, len(rings)*n))]
        fs += [(j*n+i,j*n+(i+1)%n,(j+1)*n+(i+1)%n,(j+1)*n+i)
               for j in range(len(rings)-1) for i in range(n)]
        self.mesh(vs,fs,role)

    def tube(self,a,b,r1,r2=None,role='oak',n=10):
        self.loft([(a,r1,r1),(b,r1 if r2 is None else r2,r1 if r2 is None else r2)],role,n,Vector(b)-Vector(a))

    def ellipsoid(self, p, size, role, n=12, bands=8, rot=None):
        vs=[]
        for j in range(bands+1):
            t=math.pi*j/bands
            for i in range(n):
                a=2*math.pi*i/n
                v=Vector((size[0]*math.sin(t)*math.cos(a),size[1]*math.sin(t)*math.sin(a),size[2]*math.cos(t)))
                vs.append(Vector(p)+(rot@v if rot else v))
        self.mesh(vs,[(j*n+i,j*n+(i+1)%n,(j+1)*n+(i+1)%n,(j+1)*n+i)
                      for j in range(bands) for i in range(n)],role)

    def ring(self, p, outer, inner, width, role='iron', n=24, axis=(1,0,0)):
        rot=Vector(axis).to_track_quat('Z','Y').to_matrix()
        vs=[Vector(p)+rot@Vector((r*math.cos(2*math.pi*i/n),r*math.sin(2*math.pi*i/n),z))
            for z,r in [(-width/2,outer),(width/2,outer),(-width/2,inner),(width/2,inner)] for i in range(n)]
        fs=[]
        for i in range(n):
            j=(i+1)%n
            fs += [(i,j,j+n,i+n),(i+2*n,i+3*n,j+3*n,j+2*n),
                   (i,i+2*n,j+2*n,j),(i+n,j+n,j+3*n,i+3*n)]
        self.mesh(vs,fs,role)

    def object(self,name,coll,parent=None,layer='structure',bevel=.012):
        mesh=bpy.data.meshes.new(name)
        mesh.from_pydata(self.v,[],self.f);mesh.update()
        for role in self.roles: mesh.materials.append(material(role))
        for face,idx in zip(mesh.polygons,self.mi): face.material_index=idx
        bm=bmesh.new();bm.from_mesh(mesh)
        bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=.000001)
        bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces))
        bm.to_mesh(mesh);bm.free();mesh.update()
        uv=mesh.uv_layers.new(name='UV0_meters')
        for face in mesh.polygons:
            axis=max(range(3),key=lambda a:abs(face.normal[a]))
            axes=((1,2),(0,2),(0,1))[axis]
            for li in face.loop_indices:
                co=mesh.vertices[mesh.loops[li].vertex_index].co
                uv.data[li].uv=(co[axes[0]],co[axes[1]])
        ob=bpy.data.objects.new(name,mesh);coll.objects.link(ob);ob.parent=parent
        ob['layer']=layer;ob['material_roles']=self.roles
        if bevel:
            mod=ob.modifiers.new('Hand softened edges','BEVEL');mod.width=bevel;mod.segments=2
            mod.limit_method='ANGLE';mod.angle_limit=.65
        return ob
