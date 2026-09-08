"""Restore generated UE material parameters from their tracked GLB sources."""
from pathlib import Path
from collections import Counter
import re,json,sys
import bpy

OUT=Path(__file__).resolve().parent
MANIFEST=OUT/'ThreeHearths_All_Git_Assets_ccd6757.json'
m=json.loads(MANIFEST.read_text(encoding='utf-8'))
bpy.ops.wm.open_mainfile(filepath=str(OUT/m['blend']))
def base(name):
    name=re.sub(r'[._]\d{3}$','',name)
    return re.sub(r'[^A-Za-z0-9_]','_',name)
source={}
for entry in m['entries']:
    if not entry['id'].startswith('GLB_'):continue
    for obj in bpy.data.objects[entry['root_name']].children_recursive:
        if obj.type=='MESH':
            for slot in obj.material_slots:
                mat=slot.material
                if mat and mat.node_tree:
                    principled=next((n for n in mat.node_tree.nodes if n.type=='BSDF_PRINCIPLED'),None)
                    if principled:
                        source[base(mat.name)]={k:list(principled.inputs[k].default_value) if k=='Base Color' else float(principled.inputs[k].default_value)
                            for k in ('Base Color','Roughness','Metallic')}
matched=[];unmatched=set()
done=set()
for entry in m['entries']:
    if entry['group']!='03_UE_Generated':continue
    for obj in bpy.data.objects[entry['root_name']].children_recursive:
        if obj.type!='MESH':continue
        for slot in obj.material_slots:
            mat=slot.material
            if not mat or mat in done:continue
            done.add(mat)
            key=base(mat.name)
            if key not in source:
                unmatched.add(key);continue
            spec=source[key]
            mat.use_nodes=True
            bsdf=next(n for n in mat.node_tree.nodes if n.type=='BSDF_PRINCIPLED')
            for k,value in spec.items():
                for link in list(bsdf.inputs[k].links):mat.node_tree.links.remove(link)
                bsdf.inputs[k].default_value=value
            mat.diffuse_color=spec['Base Color']
            mat.roughness=spec['Roughness']
            mat.metallic=spec['Metallic']
            mat['color_source']='Tracked GLB material '+key
            matched.append(mat.name)
m['generated_material_restore']={'matched':len(matched),'unmatched_names':sorted(unmatched),
    'source':'Same-named materials on Git GLB sources; Unreal generated material graphs do not round-trip through FBX.'}
MANIFEST.write_text(json.dumps(m,ensure_ascii=False,indent=2),encoding='utf-8')
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/m['blend']),compress=True)
print('RESTORED_SOURCE_MATERIALS '+json.dumps(m['generated_material_restore']),flush=True)
print('ROOF_SOURCE_KEYS '+json.dumps([k for k in source if 'Roof' in k or k.startswith('01')]),flush=True)
