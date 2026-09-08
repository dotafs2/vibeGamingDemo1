from pathlib import Path
import bpy

def prepare_images(texture_dir):
    repaired=[]
    shader_only=[]
    for img in list(bpy.data.images):
        if img.source=='FILE' and not img.has_data:
            candidate=Path(texture_dir)/Path(bpy.path.abspath(img.filepath)).name
            if candidate.is_file():
                img.filepath=str(candidate)
                img.reload()
                img.pack()
                assert img.packed_file,candidate
                repaired.append(str(candidate))
            elif img.name=='EnvSamplerTex':
                # This UE water-shader environment sampler is a runtime value,
                # not a missing repository bitmap. FBX has no such sampler.
                for mat in bpy.data.materials:
                    if mat.node_tree:
                        for node in list(mat.node_tree.nodes):
                            if getattr(node,'image',None)==img:
                                mat.node_tree.nodes.remove(node)
                                mat['export_note']='UE runtime environment sampler omitted; standard fallback material retained.'
                                shader_only.append(mat.name)
                bpy.data.images.remove(img)
                continue
            else:
                raise FileNotFoundError('Missing texture '+img.filepath)
        if img.has_data and not img.packed_file:
            img.pack()
    return {'resolved_files':sorted(set(repaired)),
        'runtime_shader_samplers_omitted':shader_only,
        'packed_images':sum(bool(im.packed_file) for im in bpy.data.images)}
