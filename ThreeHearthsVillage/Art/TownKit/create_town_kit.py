"""Create small TownKit connection pieces as self-contained glTF 2.0 GLBs.

The repository's Blender executable/MCP is not available in this environment, so
this exporter keeps the same authored +Z-up, -Y-front conventions and metadata
used by the existing kits. It is intentionally limited to box-built connection
pieces; Blender source/render evidence remains pending.
"""
from pathlib import Path
import base64, hashlib, json, math, struct

OUT = Path(__file__).resolve().parent
MODULES = OUT / "modules"
MODULES.mkdir(parents=True, exist_ok=True)

MATERIALS = {
    "Town_Stone": ([0.314, 0.376, 0.413, 1], 0.52),
    "Town_Stone_Edge": ([0.491, 0.533, 0.491, 1], 0.47),
    "Town_Chestnut": ([0.283, 0.130, 0.063, 1], 0.48),
    "Town_Cut_Oak": ([0.503, 0.292, 0.138, 1], 0.43),
    "Town_Honey_Plank": ([0.445, 0.238, 0.107, 1], 0.46),
    "Town_Terracotta": ([0.47, 0.18, 0.08, 1], 0.55),
}

def box(cx, cy, cz, sx, sy, sz, material):
    x0, x1 = cx-sx/2, cx+sx/2; y0, y1 = cy-sy/2, cy+sy/2; z0, z1 = cz-sz/2, cz+sz/2
    # position, normal, uv; six independent faces keep authored materials simple.
    faces = [
        ((0,0,-1), [(x0,y0,z0),(x1,y0,z0),(x1,y1,z0),(x0,y1,z0)]),
        ((0,0,1), [(x0,y0,z1),(x0,y1,z1),(x1,y1,z1),(x1,y0,z1)]),
        ((0,-1,0), [(x0,y0,z0),(x0,y0,z1),(x1,y0,z1),(x1,y0,z0)]),
        ((0,1,0), [(x0,y1,z0),(x1,y1,z0),(x1,y1,z1),(x0,y1,z1)]),
        ((-1,0,0), [(x0,y0,z0),(x0,y1,z0),(x0,y1,z1),(x0,y0,z1)]),
        ((1,0,0), [(x1,y0,z0),(x1,y0,z1),(x1,y1,z1),(x1,y1,z0)]),
    ]
    out=[]
    for normal, points in faces:
        base=len(out); out.extend([(points[0],normal,(0,0)),(points[1],normal,(1,0)),(points[2],normal,(1,1)),(points[3],normal,(0,1))])
        yield out, material, base

def make_mesh(parts):
    verts=[]; indices=[]; mats=[]
    for cx,cy,cz,sx,sy,sz,mat in parts:
        for face_vertices, face_mat, base in box(cx,cy,cz,sx,sy,sz,mat):
            start=len(verts); verts.extend(face_vertices[-4:])
            indices.extend([start,start+1,start+2,start,start+2,start+3]); mats.extend([face_mat]*6)
    return verts, indices, mats

def write_glb(path, asset_id, parts, role, inputs, origin):
    verts, indices, mats = make_mesh(parts)
    names=list(MATERIALS)
    mat_ids=[names.index(m) for m in mats]
    pos=bytearray(); nor=bytearray(); uv=bytearray(); idx=bytearray()
    for p,n,t in verts: pos += struct.pack('<3f',*p); nor += struct.pack('<3f',*n); uv += struct.pack('<2f',*t)
    # One primitive per material, preserving source-like slots.
    for i in indices: idx += struct.pack('<I',i)
    blob=bytes(pos+nor+uv+idx); po=0; no=len(pos); uo=no+len(nor); io=uo+len(uv)
    json_doc={"asset":{"version":"2.0","generator":"TownKit offline authored exporter"},"scene":0,"scenes":[{"nodes":[0]}],"nodes":[{"mesh":0,"name":asset_id,"extras":{"asset_id":asset_id,"role":role,"npc_portable":False,"execution_unit":"one_site_installation","resource_inputs":inputs,"installation_anchor_m":[0,0,0],"front_axis":"-Y","up_axis":"+Z"}}],"meshes":[{"name":asset_id,"primitives":[{"attributes":{"POSITION":0,"NORMAL":1,"TEXCOORD_0":2},"indices":3,"material":0}]}],"materials":[{"name":n,"pbrMetallicRoughness":{"baseColorFactor":MATERIALS[n][0],"metallicFactor":0,"roughnessFactor":MATERIALS[n][1]}} for n in names],"buffers":[{"byteLength":len(blob)}],"bufferViews":[{"buffer":0,"byteOffset":po,"byteLength":len(pos),"target":34962},{"buffer":0,"byteOffset":no,"byteLength":len(nor),"target":34962},{"buffer":0,"byteOffset":uo,"byteLength":len(uv),"target":34962},{"buffer":0,"byteOffset":io,"byteLength":len(idx),"target":34963}],"accessors":[{"bufferView":0,"componentType":5126,"count":len(verts),"type":"VEC3","min":[min(v[0] for v,_,_ in verts),min(v[1] for v,_,_ in verts),min(v[2] for v,_,_ in verts)],"max":[max(v[0] for v,_,_ in verts),max(v[1] for v,_,_ in verts),max(v[2] for v,_,_ in verts)]},{"bufferView":1,"componentType":5126,"count":len(verts),"type":"VEC3"},{"bufferView":2,"componentType":5126,"count":len(verts),"type":"VEC2"},{"bufferView":3,"componentType":5125,"count":len(indices),"type":"SCALAR"}]}
    raw=json.dumps(json_doc,separators=(',',':')).encode(); raw += b' ' * ((4-len(raw)%4)%4); blob += b'\0' * ((4-len(blob)%4)%4)
    glb=struct.pack('<4sII',b'glTF',2,12+8+len(raw)+8+len(blob))+struct.pack('<II',len(raw),0x4E4F534A)+raw+struct.pack('<II',len(blob),0x004E4942)+blob
    path.write_bytes(glb)
    coords=[v for v,_,_ in verts]; bounds=[[min(v[i] for v in coords) for i in range(3)],[max(v[i] for v in coords) for i in range(3)]]
    return {'id':asset_id,'asset_glb':'modules/'+path.name,'role':role,'measured_size_m':[bounds[1][i]-bounds[0][i] for i in range(3)],'bounds_authoring_m':bounds,'origin_convention':origin,'installation_anchor_m':[0,0,0],'grid_m':2,'npc_handheld':False,'execution_unit':'one_site_installation','resource_inputs':inputs,'recipe_status':'ready_for_townkit_art_review','runtime_binding':'pending','native_import_status':'not_imported','material_names':names,'geometry_source':'original_minimal_connection_component','source_geometry_reused_unchanged':False,'triangles':len(indices)//3}

specs=[]
# A 2m corner joins two orthogonal wall runs at an inside-corner anchor.
specs.append(write_glb(MODULES/'town_corner_stone_2m.glb','town_corner_stone_2m',[(1,0,1.2,2,.4,2.4,'Town_Stone'),(0,1,1.2,.4,2,2.4,'Town_Stone_Edge')],'corner_wall',{'stone':16},'Inside-corner anchor at (0,0,0); two orthogonal 2m runs, +X and +Y.'))
# A genuine opening connects room/yard edges without replacing the wall family.
specs.append(write_glb(MODULES/'town_wall_gate_timber_2m.glb','town_wall_gate_timber_2m',[(-.78,0,1.2,.18,.16,2.4,'Town_Chestnut'),(.78,0,1.2,.18,.16,2.4,'Town_Chestnut'),(0,0,2.25,1.74,.16,.30,'Town_Cut_Oak'),(0,0,.10,1.74,.16,.20,'Town_Honey_Plank')],'wall_gate',{'planks':2,'beams':2},'Centre-bottom anchor; 1.56m clear opening, front -Y.'))
# Small roof joint lets two 2m roof runs meet and extend an existing plan.
specs.append(write_glb(MODULES/'town_roof_ridge_joint_2m.glb','town_roof_ridge_joint_2m',[(0,0,1.22,.32,2,.18,'Town_Terracotta'),(0,0,1.35,.20,.20,.35,'Town_Cut_Oak'),(-.55,0,1.05,.95,2,.08,'Town_Terracotta'),(.55,0,1.05,.95,2,.08,'Town_Terracotta')],'roof_connector',{'planks':2,'beams':1,'tiles':2},'Centre anchor at ridge datum Z=1.2; two roof slopes meet a 2m ridge joint.'))
# Compact eight-step stair flight for multi-storey rooms and wall walkways.
stair_parts=[]
for i in range(8):
    stair_parts.append((-.875+i*.25,0,.15+i*.25,.24,1.2,.30,'Town_Honey_Plank'))
stair_parts += [(-.0,-.68,1.0,2,.12,2.0,'Town_Chestnut'),(-.0,.68,1.0,2,.12,2.0,'Town_Chestnut')]
specs.append(write_glb(MODULES/'town_stair_timber_2m.glb','town_stair_timber_2m',stair_parts,'stair_access',{'planks':6,'beams':2},'Centre-bottom anchor; eight .25m risers over a 2m run, side stringers provide one installation unit.'))
# A valley block joins two perpendicular roof runs without a whole-roof asset.
valley_parts=[(0,0,1.22,.28,2,.18,'Town_Terracotta'),(0,0,1.22,2,.28,.18,'Town_Terracotta'),(0,0,1.38,.18,.18,.28,'Town_Cut_Oak')]
specs.append(write_glb(MODULES/'town_roof_valley_joint_2m.glb','town_roof_valley_joint_2m',valley_parts,'roof_valley_connector',{'planks':2,'beams':1,'tiles':2},'Centre anchor at intersecting ridge/valley datum; orthogonal 2m roof edges meet at one joint.'))
# A narrow timber gable closes a 2m end bay while leaving the roof family reusable.
gable_parts=[]
for i,(width,z) in enumerate(((2.0,.25),(1.5,.75),(1.0,1.25),(.5,1.75))):
    gable_parts.append((0,0,z,width,.16,.48,'Town_Honey_Plank' if i%2 else 'Town_Cut_Oak'))
gable_parts += [(-.91,0,1.0,.14,.22,2.0,'Town_Chestnut'),(.91,0,1.0,.14,.22,2.0,'Town_Chestnut')]
specs.append(write_glb(MODULES/'town_gable_end_timber_2m.glb','town_gable_end_timber_2m',gable_parts,'gable_end',{'planks':4,'beams':1},'Centre-bottom anchor at 2m end bay; stepped timber infill closes the roof end without replacing the plan.'))

def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
for module in specs:
    module['runtime_support'] = 'blocked_on_tiles_runtime_resource' if 'tiles' in module['resource_inputs'] else 'runtime_supported_existing_stock_candidate'
catalog={'schema_version':1,'kit_id':'town_kit_01','status':'art-authored-runtime-binding-pending','units':'metres','up_axis':'+Z','front_axis':'-Y','grid_m':2,'materials':{'stone':'reused SocietyKit blue-grey stone family','timber':'reused VillageKit chestnut/cut-oak family','terracotta':'reused VillageKit terracotta roof family'},'reuse_audit':{'VillageKit':'Straight walls, doors/windows and roof slopes already cover bays; TownKit adds orthogonal corner, real gate opening and roof junction.','PublicWallKit':'2m grid and native PBR conventions reused; its public wall pieces remain unchanged.','SocietyKit':'Castle stone palette and modular wall precedent reused; no source geometry copied into new pieces.'},'modules':specs,'npc_transport_rule':'NPCs carry listed stock inputs; each TownKit piece is one on-site installation unit; no whole building is handheld.','ue_runtime_verified':False}
catalog['runtime_supported_module_ids']=[m['id'] for m in specs if m['runtime_support']=='runtime_supported_existing_stock_candidate']
catalog['blocked_module_ids']=[m['id'] for m in specs if m['runtime_support']=='blocked_on_tiles_runtime_resource']
(OUT/'catalog.json').write_text(json.dumps(catalog,indent=2)+'\n',encoding='utf-8')
recipes={'schema_version':1,'status':'art-authored-not-live','recipes':[{'id':'install_'+s['id'],'output_component_asset_id':s['id'],'output_count':1,'resource_inputs':s['resource_inputs'],'requires_site_installation':True,'inventory_deduction_verified':False,'runtime_support':s['runtime_support']} for s in specs], 'runtime_supported_module_ids':catalog['runtime_supported_module_ids'], 'blocked_module_ids':catalog['blocked_module_ids'], 'note':'tiles is retained as the authored roof material input; current C++ economy exposes stone/planks/beams only.'}
(OUT/'recipes.json').write_text(json.dumps(recipes,indent=2)+'\n',encoding='utf-8')
sockets={'schema_version':1,'grid_m':2,'coordinate_system':'+Z up / -Y front','sockets':[{'component':'town_corner_stone_2m','provides':['corner_inside_origin','wall_edge_x_plus','wall_edge_y_plus'],'accepts':['wall_stone_2m','wall_timber_2m','wall_door_timber_2m']},{'component':'town_wall_gate_timber_2m','provides':['wall_edge_x_minus','wall_edge_x_plus','gate_clear_opening'],'accepts':['floor_timber_2m','porch_steps_stone_2m']},{'component':'town_roof_ridge_joint_2m','provides':['ridge_x','roof_slope_left','roof_slope_right'],'accepts':['roof_slope_terracotta_2m','roof_slope_slateblue_2m','roof_ridge_timber_2m']},{'component':'town_stair_timber_2m','provides':['lower_floor_access','upper_floor_access','wall_walkway_access'],'accepts':['floor_timber_2m','floor_opening_2m','castle_walkway_timber_2m']},{'component':'town_roof_valley_joint_2m','provides':['roof_valley_x','roof_valley_y','extension_joint'],'accepts':['roof_slope_terracotta_2m','roof_slope_slateblue_2m','town_roof_ridge_joint_2m']},{'component':'town_gable_end_timber_2m','provides':['gable_end_left','gable_end_right','roof_end_cap'],'accepts':['roof_slope_timber_2m','roof_ridge_timber_2m','wall_timber_2m']}],'assembly_examples':[{'id':'shelter_to_courtyard','pieces':['town_wall_gate_timber_2m','wall_timber_2m','town_gable_end_timber_2m','roof_slope_timber_2m'],'scale_note':'one bay'}, {'id':'courtyard_to_castle','pieces':['town_corner_stone_2m','castle_wall_stone_2m','town_stair_timber_2m','town_roof_valley_joint_2m'],'scale_note':'repeated orthogonal runs'}]}
(OUT/'attachment-sockets.json').write_text(json.dumps(sockets,indent=2)+'\n',encoding='utf-8')
manifest={'kit_id':'town_kit_01','source_workflow':'offline glTF authored exporter; Blender source/render pending','artifacts':[{'file':p.relative_to(OUT).as_posix(),'bytes':p.stat().st_size,'sha256':sha(p)} for p in sorted(OUT.rglob('*')) if p.is_file() and p.suffix in ('.glb','.json','.py','.md','.svg')]}
(OUT/'artifact-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
print(json.dumps({'assets':len(specs),'triangles':sum(s['triangles'] for s in specs),'status':catalog['status']}))
