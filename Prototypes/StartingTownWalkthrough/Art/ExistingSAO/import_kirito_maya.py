"""Convert the local authored PMX into a Maya scene with original skin/morphs.

Run with Maya 2026 mayapy. Requires the existing local PMX reader and textures.
No external services, plug-ins, or user preference changes are required.
"""
from pathlib import Path
import importlib.util
import json
import os
import re
import traceback

HERE = Path(__file__).resolve().parent
OUT = HERE / 'Maya'
OUT.mkdir(exist_ok=True)
REPORT = OUT / 'conversion_validation.json'


def build():
    import maya.standalone
    maya.standalone.initialize(name='python')
    import maya.cmds as c
    import maya.api.OpenMaya as om
    import maya.api.OpenMayaAnim as oma

    c.undoInfo(state=False)
    c.currentUnit(linear='cm', angle='deg', time='film')
    spec = importlib.util.spec_from_file_location('pmx_reader', HERE / 'inspection_dependency/pmx_reader.py')
    pmx = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(pmx)
    source = HERE / 'sugaki_kirito/Kirito/キリト.pmx'
    data = pmx.load(str(source))
    texture_map = json.loads((OUT / 'texture_map.json').read_text(encoding='utf-8'))
    floor = min(v.co[1] for v in data.vertices)
    scale = 172.0 / (max(v.co[1] for v in data.vertices) - floor)

    def co(v, position=True):
        return (v[0] * scale, (v[1] - (floor if position else 0)) * scale, -v[2] * scale)

    def label(node, value):
        c.addAttr(node, longName='sourceName', dataType='string')
        # Keep Maya ASCII independent of the Windows UI/code-page language.
        c.setAttr(node + '.sourceName', ' '.join('U+%04X' % ord(ch) for ch in value), type='string')

    def meshfn(node):
        sel = om.MSelectionList()
        sel.add(node)
        dag = sel.getDagPath(0)
        dag.extendToShape()
        return om.MFnMesh(dag)

    def stage(value):
        print(json.dumps({'stage': value, 'pid': os.getpid()}), flush=True)

    stage('mesh')
    root = c.group(empty=True, name='Kirito')
    body = c.createNode('transform', name='Kirito_Body', parent=root)
    select = om.MSelectionList()
    select.add(body)
    points = om.MPointArray([co(v.co) for v in data.vertices])
    counts = [3] * len(data.faces)
    indices = [i for face in data.faces for i in face]
    fn = om.MFnMesh()
    shape_obj = fn.create(points, counts, indices, parent=select.getDependNode(0))
    shape = c.rename(om.MFnDagNode(shape_obj).fullPathName(), 'Kirito_BodyShape')
    fn.setUVs([v.uv[0] for v in data.vertices], [1-v.uv[1] for v in data.vertices])
    fn.assignUVs(counts, indices)
    fn.setVertexNormals([om.MVector(v.normal[0], v.normal[1], -v.normal[2]) for v in data.vertices], list(range(len(data.vertices))))
    c.setAttr(shape + '.doubleSided', True)

    stage('materials')
    start = 0
    materials = []
    for i, src in enumerate(data.materials):
        mat = c.shadingNode('lambert', asShader=True, name='Kirito_Mat_%02d' % i)
        label(mat, src.name)
        sg = c.sets(renderable=True, noSurfaceShader=True, empty=True, name=mat + 'SG')
        c.connectAttr(mat + '.outColor', sg + '.surfaceShader', force=True)
        c.setAttr(mat + '.color', *src.diffuse[:3], type='double3')
        c.setAttr(mat + '.diffuse', .75)
        c.setAttr(mat + '.ambientColor', .3, .3, .3, type='double3')
        alpha = src.diffuse[3]
        c.setAttr(mat + '.transparency', *([1-alpha]*3), type='double3')
        texpath = None
        if src.texture >= 0:
            texpath = texture_map[str(src.texture)]
            tex = c.shadingNode('file', asTexture=True, name='Kirito_Texture_%02d' % i)
            c.setAttr(tex + '.fileTextureName', texpath, type='string')
            c.setAttr(tex + '.colorSpace', 'sRGB', type='string')
            c.connectAttr(tex + '.outColor', mat + '.color', force=True)
            if alpha > 0 and Path(data.textures[src.texture].path).suffix.lower() in ('.tga', '.png'):
                c.connectAttr(tex + '.outTransparency', mat + '.transparency', force=True)
        count = src.vertex_count // 3
        if count:
            c.sets('%s.f[%d:%d]' % (body, start, start+count-1), edit=True, forceElement=sg)
        start += count
        materials.append({'node': mat, 'source_name': src.name, 'texture': texpath, 'alpha': alpha})
    assert start == len(data.faces)

    stage('facial_blendshapes')
    blend = c.blendShape(body, name='Kirito_Expressions', origin='local')[0]
    aliases = ['Mouth_A', 'Mouth_I', 'Mouth_U', 'Mouth_E', 'Mouth_O', 'Shout', 'Shout_Big', 'Shout_Big2', 'Smirk', 'Mouth_Angle', 'Mouth_Arc', 'Mouth_Eh', 'Blink', 'Smile_Eyes', 'Wink_L', 'Wink_R', 'Wink2_L', 'Wink2_R', 'Surprised', 'HalfEyes', 'HalfEyes2', 'Pupil_Small', 'Highlight_Thin', 'Serious', 'Troubled', 'Angry', 'Smile_Brows', 'Brows_Up', 'Brows_Down', 'Toe_R', 'Cheek_Up', 'Toe_L']
    morphs = []
    for morph in data.morphs:
        if not isinstance(morph, pmx.VertexMorph):
            continue
        i = len(morphs)
        target = c.duplicate(body, name='Kirito_MorphTemp_%02d' % i, inputConnections=False)[0]
        c.delete(target, constructionHistory=True)
        target_points = om.MPointArray(points)
        for d in morph.offsets:
            offset = co(d.offset, False)
            target_points[d.index] = points[d.index] + om.MVector(*offset)
        meshfn(target).setPoints(target_points)
        c.blendShape(blend, edit=True, target=(body, i, target, 1.0))
        c.aliasAttr(aliases[i], '%s.weight[%d]' % (blend, i))
        c.setAttr('%s.weight[%d]' % (blend, i), 0)
        c.delete(target)
        morphs.append({'index': i, 'alias': aliases[i], 'source_name': morph.name})

    stage('skeleton')
    rig = c.group(empty=True, name='Kirito_Skeleton', parent=root)
    joints = []
    for i, b in enumerate(data.bones):
        english = re.sub(r'[^A-Za-z0-9_]+', '_', b.name_e).strip('_') or 'bone'
        joint = c.createNode('joint', name='KRT_%03d_%s' % (i, english))
        label(joint, b.name)
        c.setAttr(joint + '.radius', .35)
        joints.append(joint)
    for i, b in enumerate(data.bones):
        c.parent(joints[i], joints[b.parent] if b.parent >= 0 else rig, relative=True)
        pos = co(b.location)
        parent_pos = co(data.bones[b.parent].location) if b.parent >= 0 else (0, 0, 0)
        c.setAttr(joints[i] + '.translate', *[a-bb for a, bb in zip(pos, parent_pos)], type='double3')

    stage('original_skin_weights')
    skin = c.skinCluster(joints, body, toSelectedBones=True, normalizeWeights=1, maximumInfluences=4, obeyMaxInfluences=False, name='Kirito_OriginalSkin')[0]
    sel = om.MSelectionList()
    sel.add(skin)
    skfn = oma.MFnSkinCluster(sel.getDependNode(0))
    influence_count = len(skfn.influenceObjects())
    influence_map = {p.partialPathName().split('|')[-1]: i for i, p in enumerate(skfn.influenceObjects())}
    dense = om.MDoubleArray([0.0] * (len(data.vertices) * influence_count))
    max_sum_error = 0.0
    for vi, v in enumerate(data.vertices):
        w = v.weight
        values = [1.0] if w.type == 0 else ([w.weights[0], 1-w.weights[0]] if w.type == 1 else w.weights)
        assert w.type in (0, 1, 2)
        max_sum_error = max(max_sum_error, abs(sum(values)-1))
        for bi, value in zip(w.bones, values):
            if bi >= 0 and value > 0:
                dense[vi*influence_count + influence_map[joints[bi]]] += value
    component_fn = om.MFnSingleIndexedComponent()
    component = component_fn.create(om.MFn.kMeshVertComponent)
    component_fn.addElements(range(len(data.vertices)))
    skfn.setWeights(meshfn(body).dagPath(), component, om.MIntArray(range(influence_count)), dense, False)
    del dense
    assert max_sum_error < .0001

    stage('deformation_validation')
    base = meshfn(body).getPoints(om.MSpace.kWorld)
    joint_by_name = {b.name: joints[i] for i, b in enumerate(data.bones)}
    elbow = joint_by_name['右ひじ']
    c.setAttr(elbow + '.rotateY', 35)
    posed = meshfn(body).getPoints(om.MSpace.kWorld)
    elbow_displacement = max((a-b).length() for a, b in zip(base, posed))
    c.setAttr(elbow + '.rotateY', 0)
    blink_i = next(m['index'] for m in morphs if m['source_name'] == 'まばたき')
    c.setAttr('%s.weight[%d]' % (blend, blink_i), 1)
    blink_points = meshfn(body).getPoints(om.MSpace.kWorld)
    blink_displacement = max((a-b).length() for a, b in zip(base, blink_points))
    c.setAttr('%s.weight[%d]' % (blend, blink_i), 0)
    assert elbow_displacement > 1, elbow_displacement
    assert .01 < blink_displacement < 10, blink_displacement
    assert c.polyEvaluate(body, vertex=True) == 31083
    assert c.polyEvaluate(body, triangle=True) == 49600
    assert len(joints) == 200 and len(morphs) == 32

    # A relaxed stance; the bind pose remains recoverable by zeroing rotations.
    c.setAttr(joint_by_name['左腕'] + '.rotateZ', -36)
    c.setAttr(joint_by_name['右腕'] + '.rotateZ', 36)
    c.setAttr(rig + '.visibility', False)
    c.viewPlace('persp', eye=(120, 115, 600), lookAt=(0, 86, 0), up=(0, 1, 0))
    c.setAttr('perspShape.focalLength', 55)
    c.setAttr('perspShape.nearClipPlane', .1)
    c.setAttr('perspShape.farClipPlane', 10000)
    c.playbackOptions(minTime=1, maxTime=120)
    c.select(clear=True)
    c.addAttr(root, longName='assetNotes', dataType='string')
    c.setAttr(root + '.assetNotes', 'Sugaki Kirito PMX. Original mesh, UVs, joints, skin weights and 32 vertex morphs. No MMD physics, IK solver or 4 material morphs. Local study scene.', type='string')
    scene = OUT / 'Kirito_Start.ma'
    if scene.exists():
        raise RuntimeError('Refusing to overwrite an existing scene: ' + str(scene))
    c.file(rename=str(scene))
    c.file(save=True, type='mayaAscii')
    report = {'passed': True, 'scene': str(scene), 'source': str(source), 'maya': c.about(version=True), 'vertices': 31083, 'triangles': 49600, 'joints': len(joints), 'influences': influence_count, 'vertex_morphs': morphs, 'materials': materials, 'joint_names': joint_by_name, 'elbow_test_max_displacement_cm': elbow_displacement, 'blink_test_max_displacement_cm': blink_displacement, 'max_skin_sum_error': max_sum_error, 'mmd_physics_transferred': False, 'mmd_ik_solver_transferred': False, 'material_morphs_transferred': False, 'height_cm': 172, 'visual_checked': False}
    REPORT.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    stage('saved')
    maya.standalone.uninitialize()


if __name__ == '__main__':
    try:
        build()
    except Exception:
        REPORT.write_text(json.dumps({'passed': False, 'error': traceback.format_exc()}, indent=2), encoding='utf-8')
        raise
