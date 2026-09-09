"""Open the generated scene in the user's empty Maya session and frame it."""
from pathlib import Path
import json
import traceback
import maya.cmds as c

HERE = Path(__file__).resolve().parent
OUT = HERE / 'Maya'


def setup():
    scene = OUT / 'Kirito_Start.ma'
    current = c.file(q=True, sn=True)
    owned_scene = current and Path(current).resolve() == scene.resolve()
    if not owned_scene and (current or c.ls(type='mesh') or set(c.ls(assemblies=True)) - {'persp', 'top', 'front', 'side'}):
        raise RuntimeError('The current scene is no longer empty; keep its contents and inspect before opening.')
    c.file(str(scene), open=True, force=True, executeScriptNodes=False)
    for panel in c.getPanel(type='modelPanel'):
        c.modelEditor(panel, edit=True, displayAppearance='smoothShaded', displayTextures=True,
                      displayLights='default', useDefaultMaterial=False, twoSidedLighting=True,
                      wireframeOnShaded=False, joints=False, grid=True)
    c.viewPlace('persp', eye=(120, 115, 600), lookAt=(0, 86, 0), up=(0, 1, 0))
    c.setAttr('perspShape.focalLength', 55)
    c.select(clear=True)
    c.refresh(force=True)
    c.file(save=True, type='mayaAscii')
    report = {'passed': True, 'scene': c.file(q=True, sn=True), 'meshes': len(c.ls(type='mesh', noIntermediate=True)),
              'joints': len(c.ls(type='joint')), 'skinClusters': c.ls(type='skinCluster'),
              'blendShapes': c.ls(type='blendShape'), 'missing_textures': [], 'maya_pid': 23272}
    for node in c.ls(type='file'):
        path = c.getAttr(node + '.fileTextureName')
        if path and not Path(path).is_file():
            report['missing_textures'].append(path)
    assert not report['missing_textures']
    assert report['joints'] == 200
    (OUT / 'live_validation.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    c.inViewMessage(amg='Kirito loaded | Alt + drag: orbit | Wheel: zoom', pos='topCenter', fade=True)


try:
    setup()
except Exception:
    (OUT / 'live_error.txt').write_text(traceback.format_exc(), encoding='utf-8')
    raise
