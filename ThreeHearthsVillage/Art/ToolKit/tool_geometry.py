"""Shared-style local geometry primitives, vendored from HomeLifeKit/VillageKit.

Self-contained: ToolKit generation never modifies or executes the older kits.
"""
import math
import bpy
import bmesh
from mathutils import Vector

MODULES = None
PARTS = []
M = {}

def configure(collection, materials):
    global MODULES
    MODULES = collection
    PARTS.clear()
    M.clear()
    M.update(materials)

def linear(v):
    return v / 12.92 if v <= .04045 else ((v + .055) / 1.055) ** 2.4

def mat(name, color, roughness, metallic=0):
    rgba = tuple(linear(int(color[i:i+2], 16) / 255) for i in (0, 2, 4)) + (1,)
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    m.diffuse_color = rgba
    p = m.node_tree.nodes.get('Principled BSDF')
    p.inputs['Base Color'].default_value = rgba
    p.inputs['Roughness'].default_value = roughness
    p.inputs['Metallic'].default_value = metallic
    return m

def adopt(obj, material):
    for c in list(obj.users_collection):
        c.objects.unlink(obj)
    MODULES.objects.link(obj)
    if material:
        obj.data.materials.append(material)
    PARTS.append(obj)
    return obj

def uv_project(mesh):
    """Repeatable local planar UVs, including custom geometry; never empty UVs."""
    layer = mesh.uv_layers.new(name='UVMap') if not mesh.uv_layers else mesh.uv_layers.active
    for p in mesh.polygons:
        axis = max(range(3), key=lambda i: abs(p.normal[i]))
        axes = [i for i in range(3) if i != axis]
        for li in p.loop_indices:
            v = mesh.vertices[mesh.loops[li].vertex_index].co
            layer.data[li].uv = (v[axes[0]], v[axes[1]])

def mesh(name, verts, faces, material, smooth_faces=()):
    data = bpy.data.meshes.new(name)
    data.from_pydata(verts, [], faces)
    data.update()
    bm = bmesh.new()
    bm.from_mesh(data)
    bmesh.ops.recalc_face_normals(bm, faces=list(bm.faces))
    bm.to_mesh(data)
    bm.free()
    uv_project(data)
    for i in smooth_faces:
        data.polygons[i].use_smooth = True
    return adopt(bpy.data.objects.new(name, data), material)

def bevel(obj, width=.018, segments=1):
    if width <= 0:
        return obj
    bpy.ops.object.select_all(action='DESELECT')
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    mod = obj.modifiers.new('Rounded hand-cut edges', 'BEVEL')
    # Keep a safety margin below half the thinnest dimension. Letting Blender's
    # overlap clamp collapse a narrow bevel to its centre creates zero-area faces.
    mod.width = min(width, min(obj.dimensions)*.24)
    mod.segments = segments
    mod.limit_method = 'ANGLE'
    mod.angle_limit = math.radians(45)
    bpy.ops.object.modifier_apply(modifier=mod.name)
    return obj

def box(name, center, dims, material, edge=.018):
    bpy.ops.mesh.primitive_cube_add(size=1, location=center)
    obj = bpy.context.view_layer.objects.active
    obj.name = name
    obj.dimensions = dims
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    adopt(obj, material)
    bevel(obj, edge)
    return obj

def beam(name, a, b, width=.12, depth=.12, material=None):
    a, b = Vector(a), Vector(b)
    obj = box(name, (a+b)/2, (width, depth, (b-a).length), material or M['wood'])
    obj.rotation_euler = (b-a).to_track_quat('Z', 'Y').to_euler()
    return obj

def cylinder(name, center, radius, depth, material, vertices=12, direction=None):
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices, radius=radius, depth=depth, location=center)
    obj = adopt(bpy.context.view_layer.objects.active, material)
    obj.name = name
    if direction:
        obj.rotation_euler = Vector(direction).to_track_quat('Z', 'Y').to_euler()
    bevel(obj, .008)
    return obj

def arch_tile(name, origin, down, cross, normal, length, width, material, bulge=.055):
    """Shared tile UVs; smooth ONLY the curved surfaces, keep end/side edges hard."""
    origin, down, cross, normal = map(Vector, (origin, down, cross, normal))
    n = 8
    verts = []
    for layer in range(2):
        for end in range(2):
            for k in range(n+1):
                t = k/n
                h = math.sin(t*math.pi)*bulge + layer*.027 + (end*.012)
                verts.append(origin + down*(end*length) + cross*((t-.5)*width) + normal*h)
    stride = n+1
    faces = []
    smooth = []
    for layer in range(2):
        a = layer*2*stride
        for k in range(n):
            smooth.append(len(faces))
            faces.append((a+k, a+k+1, a+stride+k+1, a+stride+k))
    for end in range(2):
        a = end*stride
        for k in range(n):
            faces.append((a+k, a+k+1, a+2*stride+k+1, a+2*stride+k))
    faces += [(0, stride, stride*3, stride*2), (n, stride+n, stride*3+n, stride*2+n)]
    obj = mesh(name, verts, faces, material, smooth)
    # Identical local tile UV parameterization reused independent of placement.
    uv = obj.data.uv_layers.active
    for p in obj.data.polygons:
        vertex_ids=[obj.data.loops[li].vertex_index for li in p.loop_indices]
        same_end=len({(vi//stride)%2 for vi in vertex_ids})==1
        same_cross=len({vi%stride for vi in vertex_ids})==1
        for li in p.loop_indices:
            vi = obj.data.loops[li].vertex_index
            uv.data[li].uv = (((vi//stride)%2) if same_cross else (vi%stride)/n,
                              (vi//(2*stride)) if same_end or same_cross else ((vi//stride)%2))
    return obj
