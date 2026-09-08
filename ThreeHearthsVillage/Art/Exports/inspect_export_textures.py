from pathlib import Path
import bpy,json
OUT=Path(__file__).resolve().parent
bpy.ops.wm.open_mainfile(filepath=str(OUT/'ThreeHearths_All_Git_Assets_ccd6757.blend'))
report=[]
for img in bpy.data.images:
    if img.source=='FILE' and not img.has_data:
        users=[]
        for mat in bpy.data.materials:
            if mat.node_tree:
                for node in mat.node_tree.nodes:
                    if getattr(node,'image',None)==img:
                        users.append({'material':mat.name,'node':node.name,'node_type':node.type,
                            'outlinks':[l.to_node.name for out in node.outputs for l in out.links]})
        report.append({'name':img.name,'path':img.filepath,'users':users})
print('MISSING_TEXTURES '+json.dumps(report),flush=True)
