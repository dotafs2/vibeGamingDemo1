"""Only missing in-process wood pieces; existing stock/tool/facility assets are reused."""
from pathlib import Path
import importlib
import json
import math
import sys
import time
import bpy
import bmesh
from mathutils import Vector
OUT=Path(__file__).resolve().parent
sys.dont_write_bytecode=True
if str(OUT) not in sys.path:sys.path.insert(0,str(OUT))
import wood_geometry as G
importlib.reload(G)
START=time.monotonic()
for name in ('modules','previews','mcp'):(OUT/name).mkdir(parents=True,exist_ok=True)
for o in list(bpy.data.objects):bpy.data.objects.remove(o,do_unlink=True)
for kind in ('collections','meshes','materials','armatures','actions','libraries'):
    for item in list(getattr(bpy.data,kind)):getattr(bpy.data,kind).remove(item)
scene=bpy.context.scene;scene.name='WoodProductionKit | two original intermediate pieces'
scene.unit_settings.system='METRIC';scene.unit_settings.scale_length=1
MODULES=bpy.data.collections.new('ORIGINAL_INTERMEDIATES');scene.collection.children.link(MODULES)
M={'bark':G.mat('WP_Warm_Chestnut_Bark','9B7554',.55),'bark_dark':G.mat('WP_Bark_Facet','8B674D',.57),
 'fresh':G.mat('WP_Fresh_Oak_Cut','D0AC7A',.46),'heart':G.mat('WP_Honey_Heartwood','B2865C',.49),
 'kerf':G.mat('WP_Inner_Kerf','BC9368',.49)}
G.configure(MODULES,M)

def stock():
    # One watertight log with concentric end-grain faces, not overlapping discs.
    n=16;zc=.113;r=.113;verts=[]
    for x in (-.37,.37):
        for radius in (r,.070,.032):
            verts.extend((x,radius*math.cos(j*math.tau/n),zc+radius*math.sin(j*math.tau/n)) for j in range(n))
    faces=[];indices=[]
    for j in range(n):faces.append((j,(j+1)%n,3*n+(j+1)%n,3*n+j));indices.append(0 if j%4 else 1)
    for end in range(2):
        off=end*3*n
        for ring in range(2):
            for j in range(n):
                a=off+ring*n+j;b=off+ring*n+(j+1)%n
                faces.append((a,b,b+n,a+n));indices.append(2 if ring==0 else 3)
        faces.append(tuple(off+2*n+j for j in range(n)));indices.append(2)
    obj=G.mesh('Chestnut raw stock',verts,faces,None)
    for material in M.values():obj.data.materials.append(material)
    for p,index in zip(obj.data.polygons,indices):p.material_index=index
    return obj

def remove_volume(obj,name,center,size):
    bpy.ops.mesh.primitive_cube_add(size=1,location=center)
    cutter=bpy.context.view_layer.objects.active;cutter.name=name;cutter.dimensions=size
    bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
    bpy.ops.object.select_all(action='DESELECT');obj.select_set(True);bpy.context.view_layer.objects.active=obj
    mod=obj.modifiers.new(name,'BOOLEAN');mod.operation='DIFFERENCE';mod.solver='EXACT';mod.object=cutter
    bpy.ops.object.modifier_apply(modifier=mod.name)
    bpy.data.objects.remove(cutter,do_unlink=True)

def saw_planks():
    obj=stock()
    for y in (-.043,.043):remove_volume(obj,'Real longitudinal saw kerf',(.175,y,.18),(.45,.014,.264))
    # One exterior slab already removed exposes a broad fresh face.
    remove_volume(obj,'First bark slab removed',(.175,-.15,.14),(.45,.14,.34))
    for p in obj.data.polygons:
        c=p.center;n=p.normal
        if c.x>-.051 and ((abs(n.y)>.98 and abs(c.y)<.101) or (abs(n.x)>.98 and c.x<.0) or abs(c.z-.048)<.002):p.material_index=2
    return obj

def hew_beam():
    obj=stock()
    remove_volume(obj,'Top hewn face',(.175,0,.272),(.45,.36,.180))
    for sign in (-1,1):remove_volume(obj,'Side hewn face',(.175,sign*.155,.14),(.45,.15,.34))
    for p in obj.data.polygons:
        c=p.center;n=p.normal
        if c.x>-.051 and ((abs(n.y)>.98 and abs(c.y)<.101) or (abs(n.z)>.98 and c.z>.17) or (abs(n.x)>.98 and c.x<.0)):p.material_index=2
    return obj

DATA=[('wip_log_to_planks','Sawing log into planks',saw_planks,'planks',[.12,-.043,.13],
       'Two real longitudinal kerfs and one stripped bark slab; the remaining base is still connected.'),
      ('wip_log_to_beam','Hewing a timber beam',hew_beam,'timber_beams',[.16,-.080,.15],
       'Three faces have been flattened; the left rough end and rounded underside show unfinished stock.')]
SPECS={'schema_version':1,'kit_id':'wood_production_kit_01','units':'metres','authoring_axes':{'up':'+Z','front':'-Y'},
 'glb_axes':{'up':'+Y','front':'+Z','convert_to_authoring':'(x,-z,y)'},'metadata_coordinates':'authoring local metres',
 'origin':'exact bottom centre of each mesh envelope','runtime_integration_verified':False,
 'spawns_stock':False,'intermediates_are_inventory':False,'modules':[]}
REPORT={'kit_id':'wood_production_kit_01','built_via':'Blender MCP execute_blender_code','blender_version':bpy.app.version_string,
 'new_module_count':2,'reuses_existing_stock_tools_and_bench':True,'modules':[]}
ASSETS={}
for mid,title,build,output,workpoint,note in DATA:
    G.PARTS.clear();obj=build();obj.name=mid;obj.data.name=mid
    # Blender Boolean may transfer the cutter's unassigned material slot.
    # Give every resulting surface an explicit portable fresh-wood material.
    for index,material in enumerate(obj.data.materials):
        if material is None:obj.data.materials[index]=M['fresh']
    bpy.ops.object.select_all(action='DESELECT');obj.select_set(True);bpy.context.view_layer.objects.active=obj
    # Soft hand-cut ridges, bounded bevel width, fully applied export geometry.
    G.bevel(obj,.0028,1)
    bm=bmesh.new();bm.from_mesh(obj.data)
    # Boolean cuts across concentric end-grain rings can leave collinear loops.
    # Resolve them before triangulation so the GLB has no zero-area slivers.
    bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=1e-6)
    bmesh.ops.dissolve_degenerate(bm,edges=list(bm.edges),dist=1e-6)
    bmesh.ops.triangulate(bm,faces=list(bm.faces))
    tiny=[f for f in bm.faces if f.calc_area()<1e-10]
    if tiny:bmesh.ops.delete(bm,geom=tiny,context='FACES_ONLY')
    bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(obj.data);bm.free()
    bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
    pts=[v.co.copy() for v in obj.data.vertices];lo=Vector([min(p[k] for p in pts) for k in range(3)]);hi=Vector([max(p[k] for p in pts) for k in range(3)])
    shift=Vector(((lo.x+hi.x)/2,(lo.y+hi.y)/2,lo.z))
    for v in obj.data.vertices:v.co-=shift
    workpoint=list(Vector(workpoint)-shift)
    scene.cursor.location=(0,0,0);bpy.ops.object.origin_set(type='ORIGIN_CURSOR');obj.data.update();G.uv_project(obj.data)
    obj.data.calc_loop_triangles();tris=len(obj.data.loop_triangles);assert tris<=3000,(mid,tris)
    obj['asset_id']=mid;obj['intermediate_id']=mid;obj['output_resource_suggestion']=output;obj['spawns_stock']=False
    obj['work_anchor_m']=workpoint;obj['runtime_integration_verified']=False
    path=OUT/'modules'/(mid+'.glb')
    bpy.ops.export_scene.gltf(filepath=str(path),export_format='GLB',use_selection=True,export_yup=True,export_apply=True,
      export_texcoords=True,export_normals=True,export_materials='EXPORT',export_extras=True,
      export_animations=False,export_cameras=False,export_lights=False)
    spec={'id':mid,'label':title,'asset_glb':'modules/'+path.name,'nominal_size_m':[round(x,6) for x in hi-lo],
       'category':'static_work_in_progress','output_resource_suggestion':output,'work_anchor_m':workpoint,
       'support_anchor_m':[0,0,0],'long_axis_local':[1,0,0],'geometry_note':note,
       'inventory_identity':None,'spawns_stock':False,'runtime_integration_verified':False}
    SPECS['modules'].append(spec);REPORT['modules'].append({'id':mid,'triangles':tris,'size_m':spec['nominal_size_m'],
      'bounds_min_m':[round(x,6) for x in lo-shift],'bounds_max_m':[round(x,6) for x in hi-shift],
      'materials':[m.name for m in obj.data.materials],'uv_layers':len(obj.data.uv_layers),'bytes':path.stat().st_size})
    ASSETS[mid]=obj
REPORT['total_triangles']=sum(m['triangles'] for m in REPORT['modules']);REPORT['generation_seconds']=round(time.monotonic()-START,3)
(OUT/'module-specs.json').write_text(json.dumps(SPECS,indent=2)+'\n',encoding='utf-8')
(OUT/'model-report.json').write_text(json.dumps(REPORT,indent=2)+'\n',encoding='utf-8')
display=bpy.data.collections.new('DISPLAY | original intermediate pieces');scene.collection.children.link(display)
for i,(mid,obj) in enumerate(ASSETS.items()):
    c=bpy.data.objects.new(mid+' | overview',obj.data);display.objects.link(c);c.location=(0,(.5-i)*.65,0)
MODULES.hide_render=True;MODULES.hide_viewport=True
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'WoodProductionKit.blend'))
print('WOOD_PRODUCTION_COMPLETE',2,REPORT['total_triangles'],flush=True)
