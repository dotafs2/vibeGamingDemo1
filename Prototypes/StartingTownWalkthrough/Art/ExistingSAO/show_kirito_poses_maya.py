"""Show the existing Godot relaxed/inspection states in Maya with a preview.

Run inside Maya. Creates a separate local scene and a small session-only UI.
The transition uses Maya animation curves, not a trained AI motion model.
"""
from pathlib import Path
import json
import math
import traceback
import maya.cmds as c
import maya.api.OpenMaya as om
import maya.api.OpenMayaUI as omui

HERE = Path(__file__).resolve().parent
OUT = HERE / 'Maya'
CONVERSION = json.loads((OUT / 'conversion_validation.json').read_text(encoding='utf-8'))
JOINTS = CONVERSION['joint_names']
WINDOW = 'KiritoPosePreviewWindow'
COMPARE = 'Kirito_Pose_Comparison'
REPORT = OUT / 'two_poses_validation.json'
BODY = 'Kirito_Body'
BLEND = 'Kirito_Expressions'
ELBOW = JOINTS['右ひじ']
ANIM_PLUGS = [ELBOW+'.rotate'+axis for axis in 'XYZ'] + [BLEND+'.Blink', BLEND+'.Mouth_A']


def mesh_points(node):
    shapes = c.listRelatives(node, shapes=True, noIntermediate=True, fullPath=True)
    sel = om.MSelectionList(); sel.add(shapes[0])
    return om.MFnMesh(sel.getDagPath(0)).getPoints(om.MSpace.kWorld)


def capture(name):
    c.refresh(force=True)
    view=omui.M3dView.active3dView()
    img=om.MImage();view.readColorBuffer(img,True)
    img.writeToFile(str(OUT/name),'png')


def show_compare(*_):
    c.play(state=False)
    c.currentTime(1)
    c.setAttr(COMPARE+'.visibility',True)
    c.setAttr('Kirito.visibility',False)
    c.viewPlace('persp',eye=(0,102,650),lookAt=(0,91,0),up=(0,1,0))
    c.setAttr('perspShape.focalLength',50)
    for panel in c.getPanel(type='modelPanel'):
        c.modelEditor(panel,edit=True,grid=False)
    c.select(clear=True)
    c.refresh(force=True)


def show_frame(frame):
    c.play(state=False)
    c.setAttr(COMPARE+'.visibility',False)
    c.setAttr('Kirito.visibility',True)
    c.currentTime(frame)
    c.viewPlace('persp',eye=(120,115,600),lookAt=(0,86,0),up=(0,1,0))
    c.setAttr('perspShape.focalLength',55)
    c.select(clear=True)
    c.refresh(force=True)


def play_transition(*_):
    show_frame(1)
    c.playbackOptions(loop='once',playbackSpeed=1)
    c.play(forward=True)


def show_ui():
    if c.window(WINDOW,exists=True):
        c.deleteUI(WINDOW)
    c.window(WINDOW,title='桐人：两个现有姿势',widthHeight=(350,300),sizeable=False)
    c.columnLayout(adjustableColumn=True,rowSpacing=8,columnAttach=('both',12))
    c.text(label='A：自然站姿',align='left',height=24)
    c.text(label='B：右肘弯曲＋闭眼、张嘴（原检查姿势）',align='left')
    c.button(label='并排看 A 和 B',height=34,command=show_compare)
    c.rowLayout(numberOfColumns=2,columnWidth2=(154,154),columnAttach2=('both','both'))
    c.button(label='只看 A',width=150,height=30,command=lambda *_:show_frame(1))
    c.button(label='只看 B',width=150,height=30,command=lambda *_:show_frame(49))
    c.setParent('..')
    c.button(label='播放过渡：A → B → A',height=36,command=play_transition)
    c.button(label='停止播放',height=26,command=lambda *_:c.play(state=False))
    c.text(label='此演示使用 Maya 平滑插值，未使用 AI 生成。',align='left',height=24)
    c.showWindow(WINDOW)
    # Place the small window over the right-side channel area, outside the model.
    try:
        from PySide6 import QtWidgets, QtCore
        import shiboken6
        import maya.OpenMayaUI as legacy_ui
        main=shiboken6.wrapInstance(int(legacy_ui.MQtUtil.mainWindow()),QtWidgets.QWidget)
        widget=shiboken6.wrapInstance(int(legacy_ui.MQtUtil.findWindow(WINDOW)),QtWidgets.QWidget)
        pos=main.mapToGlobal(QtCore.QPoint(max(10,main.width()-widget.width()-25),180))
        widget.move(pos)
    except Exception:
        pass


def build():
    for node in ('Kirito',BODY,BLEND,ELBOW):
        if not c.objExists(node):
            raise RuntimeError('Open the imported Kirito scene first: missing '+node)
    dest=OUT/'Kirito_Two_Poses.ma'
    current_scene=c.file(q=True,sn=True)
    retry_owned_failure=(current_scene and Path(current_scene).resolve()==dest.resolve() and (OUT/'two_poses_error.txt').exists() and not REPORT.exists())
    if c.objExists(COMPARE) and retry_owned_failure:
        c.delete(COMPARE)
    if c.objExists(COMPARE):
        show_compare();show_ui()
        return
    if any(c.listConnections(plug,source=True,destination=False) for plug in ANIM_PLUGS):
        raise RuntimeError('Existing animation is connected to the pose channels; inspect before changing it.')
    if dest.exists() and not retry_owned_failure:
        raise RuntimeError('Refusing to overwrite the previous pose scene: '+str(dest))
    source_scene=str(OUT/'Kirito_Start.ma') if retry_owned_failure else c.file(q=True,sn=True)
    # Save a distinct scene before adding any animation or comparison geometry.
    c.file(rename=str(dest));c.file(save=True,type='mayaAscii')
    auto=c.autoKeyframe(q=True,state=True)
    c.autoKeyframe(state=False)
    try:
        c.play(state=False)
        c.currentTime(1)
        for joint in JOINTS.values():
            c.setAttr(joint+'.rotate',0,0,0,type='double3')
        c.setAttr(JOINTS['左腕']+'.rotateZ',-36)
        c.setAttr(JOINTS['右腕']+'.rotateZ',36)
        for i in range(32):
            c.setAttr('%s.weight[%d]'%(BLEND,i),0)
        c.group(empty=True,name=COMPARE)
        states=[('A_Relaxed_Preview',-80,0,0,0),('B_Inspection_Preview',80,-math.degrees(.8),1,.65)]
        snapshots=[]
        for name,offset,angle,blink,mouth in states:
            c.setAttr(ELBOW+'.rotateX',angle)
            c.setAttr(BLEND+'.Blink',blink)
            c.setAttr(BLEND+'.Mouth_A',mouth)
            live=mesh_points(BODY)
            copy=c.duplicate(BODY,name=name,inputConnections=False)[0]
            c.delete(copy,constructionHistory=True)
            # Keep only the copied rendered shape; original intermediates aren't needed.
            for shape in c.listRelatives(copy,shapes=True,fullPath=True) or []:
                if c.getAttr(shape+'.intermediateObject'):
                    c.delete(shape)
            copied=mesh_points(copy)
            error=max((a-b).length() for a,b in zip(live,copied))
            assert len(copied)==31083 and error<.001,(name,error)
            c.parent(copy,COMPARE)
            c.setAttr(copy+'.translateX',lock=False)
            c.setAttr(copy+'.translateX',offset)
            letter=c.textCurves(text=name[0],font='Arial',name='Pose_'+name[0]+'_Label',constructionHistory=False)[0]
            c.parent(letter,COMPARE)
            c.setAttr(letter+'.translate',offset-4,182,0,type='double3')
            c.setAttr(letter+'.scale',10,10,10,type='double3')
            for shape in c.listRelatives(letter,allDescendents=True,type='nurbsCurve',fullPath=True) or []:
                c.setAttr(shape+'.overrideEnabled',True)
                c.setAttr(shape+'.overrideColor',17)
            snapshots.append({'name':name,'copy_error_cm':error,'elbow_x_degrees':angle,'blink':blink,'mouth_a':mouth})
        # The Godot inspection pose uses a -0.8 radian rotation about world-rest X.
        # Maya joints were imported with identity jointOrient, so this maps to Rx.
        for frame,weight in [(1,0),(13,0),(49,1),(61,1),(97,0),(109,0)]:
            values=[-math.degrees(.8)*weight,0,0,weight,.65*weight]
            for plug,value in zip(ANIM_PLUGS,values):
                c.setKeyframe(plug,time=frame,value=value)
        c.keyTangent(ANIM_PLUGS,inTangentType='plateau',outTangentType='plateau')
        c.playbackOptions(minTime=1,maxTime=109,animationStartTime=1,animationEndTime=109,loop='once',playbackSpeed=1)
        samples=[]
        for frame in (1,13,31,49,61,79,97,109):
            c.currentTime(frame)
            values=[c.getAttr(p) for p in ANIM_PLUGS]
            assert all(math.isfinite(v) for v in values)
            assert -math.degrees(.8)-.001<=values[0]<=.001
            assert -.001<=values[3]<=1.001 and -.001<=values[4]<=.651
            samples.append({'frame':frame,'channels':values})
        assert 0 < samples[2]['channels'][3] < 1
        c.currentTime(1);a=mesh_points(BODY)
        c.currentTime(49);b=mesh_points(BODY)
        delta=max((x-y).length() for x,y in zip(a,b))
        assert delta>1
        show_compare()
        capture('two_poses_comparison.png')
        show_frame(1);capture('pose_A.png')
        show_frame(49);capture('pose_B.png')
        show_compare()
        c.file(save=True,type='mayaAscii')
        REPORT.write_text(json.dumps({'passed':True,'source_scene':source_scene,'scene':str(dest),'source_pose_definition':str(HERE.parents[1]/'kirito_actor.gd'),'poses':snapshots,'transition':'Maya plateau keyframe interpolation; not AI generated','range':[1,109],'pose_a_frame':1,'pose_b_frame':49,'samples':samples,'pose_vertex_delta_cm':delta,'source_scene_overwritten':False,'ai_calls':0},ensure_ascii=False,indent=2),encoding='utf-8')
        show_ui()
    finally:
        c.autoKeyframe(state=auto)


if __name__=='__main__':
    try:
        build()
    except Exception:
        (OUT/'two_poses_error.txt').write_text(traceback.format_exc(),encoding='utf-8')
        raise
