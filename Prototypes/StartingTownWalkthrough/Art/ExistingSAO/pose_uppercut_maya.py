"""Assistant-authored uppercut pose; not MotionMaker output. Run in Maya."""
from pathlib import Path
import json, math, traceback
import maya.cmds as c
import maya.api.OpenMaya as om
import maya.api.OpenMayaUI as ui

OUT=Path('C:/vibeGamingDemo1/Prototypes/StartingTownWalkthrough/Art/ExistingSAO/Maya')
J=json.loads((OUT/'conversion_validation.json').read_text(encoding='utf-8'))['joint_names']

def worldpos(node):
    return om.MVector(*c.xform(node,q=True,worldSpace=True,translation=True))

def aim(name,child,direction):
    joint=J[name];tip=J[child]
    sel=om.MSelectionList();sel.add(joint)
    fn=om.MFnTransform(sel.getDagPath(0))
    q=fn.rotation(om.MSpace.kWorld,asQuaternion=True)
    current=(worldpos(tip)-worldpos(joint)).normal()
    target=om.MVector(*direction).normal()
    change=current.rotateTo(target)
    local=current.rotateBy(q.inverse())
    result=max([q*change,change*q],key=lambda candidate:local.rotateBy(candidate)*target)
    angles=result.asEulerRotation()
    c.xform(joint,worldSpace=True,rotation=[math.degrees(a) for a in angles])

def main():
    if not c.objExists('Kirito_Body'):
        raise RuntimeError('Kirito is not in the current scene.')
    dest=OUT/'Kirito_Uppercut.ma'
    if dest.exists():
        from datetime import datetime
        dest=OUT/('Kirito_Uppercut_'+datetime.now().strftime('%H%M%S')+'.ma')
    c.play(state=False)
    c.file(rename=str(dest));c.file(save=True,type='mayaAscii')
    if c.window('KiritoPosePreviewWindow',exists=True):c.deleteUI('KiritoPosePreviewWindow')
    if c.objExists('Kirito_Pose_Comparison'):c.setAttr('Kirito_Pose_Comparison.visibility',False)
    c.setAttr('Kirito.visibility',True)
    c.setAttr('Kirito_Skeleton.visibility',False)
    auto=c.autoKeyframe(q=True,state=True);c.autoKeyframe(state=False)
    try:
        # Work only in this new scene; remove the previous demonstration keys.
        c.cutKey(list(J.values()),attribute=['rotateX','rotateY','rotateZ'],clear=True)
        c.cutKey('Kirito_Expressions',clear=True)
        for node in J.values():c.setAttr(node+'.rotate',0,0,0,type='double3')
        for i in range(32):c.setAttr('Kirito_Expressions.weight[%d]'%i,0)
        c.currentTime(1)
        c.setAttr(J['センター']+'.translateY',-7)
        c.setAttr(J['下半身']+'.rotate',0,-16,0,type='double3')
        c.setAttr(J['上半身']+'.rotate',8,-24,-8,type='double3')
        c.setAttr(J['上半身2']+'.rotate',-6,10,-4,type='double3')
        c.setAttr(J['首']+'.rotate',-5,10,4,type='double3')
        c.setAttr(J['頭']+'.rotate',-6,6,0,type='double3')
        # Wide planted legs and a slightly lowered center of mass.
        aim('左足','左ひざ',(.24,-.94,.18))
        aim('左ひざ','左足首',(-.04,-.94,-.34))
        aim('右足','右ひざ',(-.24,-.90,-.32))
        aim('右ひざ','右足首',(.02,-.95,.31))
        c.xform(J['左足首'],worldSpace=True,rotation=(0,13,0))
        c.xform(J['右足首'],worldSpace=True,rotation=(0,-18,0))
        # Right elbow drives forward and the fist rises in front of the face.
        aim('右腕','右ひじ',(.20,-.25,.95))
        aim('右ひじ','右手首',(.03,.98,.20))
        # Left hand guards the cheek.
        aim('左腕','左ひじ',(.30,-.78,.54))
        aim('左ひじ','左手首',(-.50,.83,.25))
        for side,sign in [('右',1),('左',-1)]:
            for finger in ['人指','中指','薬指','小指']:
                for part,angle in [('１',75),('２',90),('３',65)]:
                    c.setAttr(J[side+finger+part]+'.rotateZ',sign*angle)
            c.setAttr(J[side+'親指０']+'.rotate',-15,sign*25,sign*18,type='double3')
            c.setAttr(J[side+'親指１']+'.rotateZ',sign*38)
            c.setAttr(J[side+'親指２']+'.rotateZ',sign*42)
        c.setAttr('Kirito_Expressions.Angry',.6)
        c.setAttr('Kirito_Expressions.Mouth_A',.12)
        # Lift the posed mesh just enough to place its lowest sole at ground.
        shape=c.listRelatives('Kirito_Body',shapes=True,noIntermediate=True,fullPath=True)[0]
        sel=om.MSelectionList();sel.add(shape)
        pts=om.MFnMesh(sel.getDagPath(0)).getPoints(om.MSpace.kWorld)
        c.setAttr(J['センター']+'.translateY',c.getAttr(J['センター']+'.translateY')-min(p.y for p in pts))
        c.viewPlace('persp',eye=(250,135,510),lookAt=(0,93,10),up=(0,1,0))
        c.setAttr('perspShape.focalLength',52)
        for panel in c.getPanel(type='modelPanel'):
            c.modelEditor(panel,edit=True,displayTextures=True,displayAppearance='smoothShaded',displayLights='flat',twoSidedLighting=True,joints=False,grid=False)
        c.select(clear=True);c.refresh(force=True)
        image=om.MImage();ui.M3dView.active3dView().readColorBuffer(image,True)
        image.writeToFile(str(OUT/'uppercut_viewport.png'),'png')
        c.file(save=True,type='mayaAscii')
        (OUT/'uppercut_result.json').write_text(json.dumps({'scene':str(dest),'method':'assistant-authored skeletal pose, not MotionMaker','pose':'right uppercut with left guard and staggered legs'},indent=2),encoding='utf-8')
    finally:c.autoKeyframe(state=auto)

try:main()
except Exception:
    (OUT/'uppercut_error.txt').write_text(traceback.format_exc(),encoding='utf-8')
    raise
