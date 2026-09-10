"""Two endpoints: ordinary interpolation versus actual MotionMaker jump retargeting.

The hybrid uses native generated lower-body directions and jump height/timing,
with authored uppercut arms, coat animation and blends to the exact endpoints.
"""
from pathlib import Path
import json, math, re, traceback
import maya.cmds as c
import maya.api.OpenMaya as om
import maya.api.OpenMayaUI as ui

BASE=Path('C:/vibeGamingDemo1/Prototypes/StartingTownWalkthrough/Art/ExistingSAO')
OUT=BASE/'Maya'
J=json.loads((OUT/'conversion_validation.json').read_text(encoding='utf-8'))['joint_names']
WINDOW='KiritoTransitionCompare'

def pose_from_ma(path):
    text=path.read_text(encoding='utf-8')
    poses={}
    for block in re.split(r'(?=createNode )',text):
        m=re.match(r'createNode joint -n "([^"]+)"',block)
        if not m or m[1] not in J.values():continue
        p={}
        for a,default in [('t',[0.,0.,0.]),('r',[0.,0.,0.])]:
            match=re.search(r'setAttr "\.'+a+r'" -type "double3"\s+([^;]+);',block)
            p[a]=list(map(float,match[1].split())) if match else default
        poses[m[1]]=p
    if len(poses)!=len(J):raise RuntimeError('Incomplete endpoint skeleton '+str(len(poses)))
    return poses

def ease(x):
    x=max(0.,min(1.,x));return x*x*(3-2*x)

def mix(a,b,u):return [x+(y-x)*u for x,y in zip(a,b)]

def qmix(a,b,u):
    qa=om.MEulerRotation(*map(math.radians,a)).asQuaternion()
    qb=om.MEulerRotation(*map(math.radians,b)).asQuaternion()
    return [math.degrees(x) for x in om.MQuaternion.slerp(qa,qb,u).asEulerRotation()]

def pos(node):return om.MVector(*c.xform(node,q=True,ws=True,t=True))

def aim(node,child,direction):
    s=om.MSelectionList();s.add(node)
    q=om.MFnTransform(s.getDagPath(0)).rotation(om.MSpace.kWorld,asQuaternion=True)
    now=(pos(child)-pos(node)).normal();target=om.MVector(*direction).normal()
    change=now.rotateTo(target);local=now.rotateBy(q.inverse())
    result=max((q*change,change*q),key=lambda r:local.rotateBy(r)*target)
    c.xform(node,ws=True,ro=[math.degrees(x) for x in result.asEulerRotation()])

def mesh_min():
    shape=c.listRelatives('Kirito_Body',s=True,ni=True,f=True)[0]
    sel=om.MSelectionList();sel.add(shape)
    return min(p.y for p in om.MFnMesh(sel.getDagPath(0)).getPoints(om.MSpace.kWorld))

def apply_pose(p):
    for n,d in p.items():
        c.setAttr(n+'.translate',*d['t'],type='double3')
        c.setAttr(n+'.rotate',*d['r'],type='double3')

def capture(name):
    c.refresh(force=True)
    im=om.MImage();ui.M3dView.active3dView().readColorBuffer(im,True)
    im.writeToFile(str(OUT/name),'png')

def play_clip(start,end,label):
    c.play(state=False)
    c.playbackOptions(minTime=start,maxTime=end,loop='once',playbackSpeed=1)
    c.currentTime(start)
    if c.text('KiritoTransitionStatus',exists=True):c.text('KiritoTransitionStatus',e=True,label=label)
    c.select(clear=True)
    c.play(forward=True)

def show_ui():
    if c.window(WINDOW,exists=True):c.deleteUI(WINDOW)
    c.window(WINDOW,title='桐人 · 两种过渡对比',widthHeight=(360,295),sizeable=False)
    c.columnLayout(adjustableColumn=True,rowSpacing=9,columnAttach=('both',12))
    c.text(label='相同起点与终点：站立上勾拳 → 腾空上勾拳',height=30)
    c.button(label='① 播放普通平滑过渡',height=45,backgroundColor=(.22,.34,.46),command=lambda *_:play_clip(1,72,'普通：两个姿势的平滑插值'))
    c.button(label='② 播放 Maya AI 跳跃＋上勾拳',height=45,backgroundColor=(.23,.42,.34),command=lambda *_:play_clip(101,172,'AI 混合：MotionMaker 跳跃＋摆好的拳姿'))
    c.button(label='停止',height=28,command=lambda *_:c.play(state=False))
    c.text('KiritoTransitionStatus',label='点击上方按钮即可观看',height=25)
    c.text(label='AI 来自 Maya 自带 MotionMaker。\n拳姿、衣摆和首尾衔接由脚本调整。',align='left',height=38)
    c.showWindow(WINDOW)
    try:
        from PySide6 import QtWidgets,QtCore
        import shiboken6
        import maya.OpenMayaUI as mui
        main=shiboken6.wrapInstance(int(mui.MQtUtil.mainWindow()),QtWidgets.QWidget)
        widget=shiboken6.wrapInstance(int(mui.MQtUtil.findWindow(WINDOW)),QtWidgets.QWidget)
        widget.move(main.mapToGlobal(QtCore.QPoint(max(10,main.width()-widget.width()-16),150)))
    except Exception:pass

def main():
    c.play(state=False)
    A=pose_from_ma(OUT/'Kirito_Uppercut.ma')
    B=pose_from_ma(OUT/'Kirito_Airborne_Uppercut.ma')
    native=json.loads((OUT/'native_motionmaker_jump.json').read_text())
    if native['native_result_code'][0]!=0:raise RuntimeError('Native generation did not succeed')
    # Save a distinct scene, retaining the current studio and all earlier source files.
    dest=OUT/'Kirito_Transition_Comparison.ma'
    c.file(rename=str(dest));c.file(save=True,type='mayaAscii',force=True)
    old_auto=c.autoKeyframe(q=True,state=True);c.autoKeyframe(state=False)
    c.undoInfo(openChunk=True,chunkName='Uppercut transition comparison')
    c.refresh(suspend=True)
    try:
        for win in ['KiritoPosePreviewWindow',WINDOW]:
            if c.window(win,exists=True):c.deleteUI(win)
        c.cutKey(list(J.values()),at=['tx','ty','tz','rx','ry','rz'],clear=True)
        c.cutKey('Kirito_Expressions',clear=True)
        if c.objExists('Kirito_Pose_Comparison'):c.setAttr('Kirito_Pose_Comparison.visibility',False)
        c.setAttr('Kirito.visibility',True)
        apply_pose(A);floor_a=mesh_min()
        apply_pose(B);floor_b=mesh_min()
        changed=[n for n in A if max(abs(x-y) for x,y in zip(A[n]['r']+A[n]['t'],B[n]['r']+B[n]['t']))>1e-6]
        native_bones=[J[x] for x in ['下半身','上半身','右足','右ひざ','右足首','左足','左ひざ','左足首']]
        keyed=list(dict.fromkeys(changed+native_bones+[J['センター']]))
        # Each clip is 3 s at 24 fps, including short endpoint holds.
        source_start=36;source_end=49
        def source(f):
            lo=int(f);hi=min(96,lo+1);t=f-lo
            return {n:mix(native['frames'][lo]['joints'][n]['p'],native['frames'][hi]['joints'][n]['p'],t) for n in native['frames'][lo]['joints']}
        end_src=source(source_end)
        end_foot=min(end_src['l_foot'][1],end_src['r_foot'][1])
        def record(frame,u):
            c.setKeyframe(keyed,attribute=['rx','ry','rz'],time=frame,inTangentType='linear',outTangentType='linear')
            c.setKeyframe(J['センター'],attribute=['tx','ty','tz'],time=frame,inTangentType='linear',outTangentType='linear')
            for a,v0,v1 in [('Angry',.6,.75),('Serious',0,.18),('Shout',0,.22),('Mouth_A',.12,0)]:
                c.setKeyframe('Kirito_Expressions.'+a,t=frame,v=v0+(v1-v0)*u,inTangentType='linear',outTangentType='linear')
        for mode,start in [('ordinary',1),('native_hybrid',101)]:
            for k in range(72):
                frame=start+k
                x=max(0.,min(1.,(k-10)/44.))
                u=ease(x)
                # AI retains its takeoff timing; punch and coat follow later in the rise.
                punch=u if mode=='ordinary' else ease((x-.20)/.8)
                for n in keyed:
                    blend=punch if mode=='native_hybrid' and n!=J['センター'] else u
                    c.setAttr(n+'.rotate',*qmix(A[n]['r'],B[n]['r'],blend),type='double3')
                c.setAttr(J['センター']+'.translate',*mix(A[J['センター']]['t'],B[J['センター']]['t'],u),type='double3')
                if mode=='native_hybrid' and 0<x<1:
                    src=source(source_start+(source_end-source_start)*x)
                    weight=ease(x/.18)*(1-ease((x-.73)/.27))
                    # World bone-direction transfer avoids joint-axis differences between rigs.
                    pairs=[('下半身','右足','c_pelvis','r_hip',.24),('上半身','上半身2','c_spine_01','c_spine_03',.65),('右足','右ひざ','r_hip','r_knee',1),('右ひざ','右足首','r_knee','r_foot',1),('左足','左ひざ','l_hip','l_knee',1),('左ひざ','左足首','l_knee','l_foot',1),('右足首','右つま先','r_foot','r_toe',.8),('左足首','左つま先','l_foot','l_toe',.8)]
                    for n,ch,sn,sc,strength in pairs:
                        if n not in J or ch not in J:continue
                        before=c.getAttr(J[n]+'.rotate')[0]
                        aim(J[n],J[ch],[b-a for a,b in zip(src[sn],src[sc])])
                        after=c.getAttr(J[n]+'.rotate')[0]
                        c.setAttr(J[n]+'.rotate',*qmix(before,after,weight*strength),type='double3')
                    # Native foot clearance controls takeoff, scaled to the requested 45 cm endpoint.
                    airborne=max(0,min(1,(min(src['l_foot'][1],src['r_foot'][1])-8.3)/max(1,end_foot-8.3)))
                    clearance=floor_a+(floor_b-floor_a)*airborne
                    clearance=clearance*(1-ease((x-.86)/.14))+floor_b*ease((x-.86)/.14)
                    c.setAttr(J['センター']+'.ty',c.getAttr(J['センター']+'.ty')+clearance-mesh_min())
                record(frame,punch)
        c.filterCurve(c.listConnections(keyed,type='animCurveTA') or [],filter='euler')
        c.currentUnit(time='film')
        c.playbackOptions(animationStartTime=1,animationEndTime=172,minTime=1,maxTime=172,loop='once',playbackSpeed=1)
        # Wider framing holds both the standing pose and the airborne fist.
        c.viewPlace('persp',eye=(260,126,650),lookAt=(-10,111,-16),up=(0,1,0))
        c.setAttr('perspShape.focalLength',51)
        for panel in c.getPanel(type='modelPanel') or []:
            c.modelEditor(panel,e=True,displayLights='all',displayTextures=True,displayAppearance='smoothShaded',joints=False,lights=False,cameras=False,grid=False,locators=False)
        c.currentTime(154)
        c.select(clear=True)
        report={'scene':str(dest),'clips':{'ordinary':[1,72],'motionmaker_hybrid':[101,172]},'fps':24,'source':str(OUT/'MotionMaker_Native_Jump_Source.ma'),'native_status':native['native_result_code'],'source_frame_range':[source_start,source_end],'method':'Actual MotionMaker generated body/leg directions and foot height, retargeted, endpoint blended; assistant-authored uppercut arms and coat. Ordinary clip is quaternion interpolation.','endpoint_clearance':[floor_a,floor_b]}
        (OUT/'transition_comparison_result.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
        c.file(save=True,type='mayaAscii',force=True)
    finally:
        c.refresh(suspend=False)
        c.undoInfo(closeChunk=True)
        c.autoKeyframe(state=old_auto)
    if c.about(batch=True):return
    for f,name in [(1,'transition_start.png'),(126,'transition_ai_crouch.png'),(145,'transition_ai_takeoff.png'),(154,'transition_end.png')]:
        c.currentTime(f);capture(name)
    show_ui()
    play_clip(101,172,'AI 混合：MotionMaker 跳跃＋摆好的拳姿')

if __name__=='__main__':
    try:main()
    except Exception:
        (OUT/'transition_comparison_error.txt').write_text(traceback.format_exc(),encoding='utf-8')
        raise
