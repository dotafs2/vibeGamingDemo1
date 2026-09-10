"""Authored uppercut demo: anticipation, planted push, fast strike, apex settle.

Uses the two existing endpoint poses. This take is assistant-authored animation;
no new MotionMaker inference is claimed. Previous native AI and comparison remain.
"""
from pathlib import Path
import math,json,runpy,traceback,copy
import maya.cmds as c
import maya.api.OpenMaya as om
BASE=Path('C:/vibeGamingDemo1/Prototypes/StartingTownWalkthrough/Art/ExistingSAO')
OUT=BASE/'Maya'
H=runpy.run_path(str(BASE/'compare_uppercut_transitions_maya.py'))
J=H['J'];ease=H['ease'];mix=H['mix'];qmix=H['qmix'];pos=H['pos'];aim=H['aim']
WINDOW='KiritoDirectedDemo'

def snapshot():return {n:{'r':list(c.getAttr(n+'.rotate')[0]),'t':list(c.getAttr(n+'.translate')[0])} for n in J.values()}
def rotate(name,value):c.setAttr(J[name]+'.rotate',*value,type='double3')
def direct(n,ch,d):aim(J[n],J[ch],d)

def solve_leg(side,target):
    hip=J[side+'足'];knee=J[side+'ひざ'];ankle=J[side+'足首']
    p=pos(hip);a=(pos(knee)-p).length();b=(pos(ankle)-pos(knee)).length()
    direction=om.MVector(*target)-p;dist=max(.01,min(direction.length(),a+b-.002));direction.normalize()
    pole=om.MVector(0,0,1);pole=(pole-direction*(pole*direction)).normal()
    along=(a*a-b*b+dist*dist)/(2*dist)
    bend=math.sqrt(max(0,a*a-along*along))
    knee_target=p+direction*along+pole*bend
    aim(hip,knee,tuple(knee_target-p))
    aim(knee,ankle,tuple(om.MVector(*target)-pos(knee)))

def coat(weight,air=0):
    for col in range(12):
        ang=col*math.tau/12
        for row in range(7):
            name='コート_%d_%d'%(row,col);child='コート_%d_%d'%(row+1,col)
            if name not in J or child not in J:continue
            before=c.getAttr(J[name]+'.rotate')[0]
            # Keep hanging fabric aligned with gravity as the torso leans forward.
            direction=om.MVector(.30*math.sin(ang),-.94,.22*math.cos(ang)-.12).normal()
            p=pos(J[name]);length=(pos(J[child])-p).length()
            if length>.001 and p.y+direction.y*length<4:
                dy=max(-.99,min(.10,(4-p.y)/length))
                horizontal=om.MVector(direction.x,0,direction.z).normal()*math.sqrt(1-dy*dy)
                direction=om.MVector(horizontal.x,dy,horizontal.z)
            direct(name,child,tuple(direction))
            rotate(name,qmix(before,c.getAttr(J[name]+'.rotate')[0],weight))

def play(slow=False,*_):
    c.play(state=False);c.playbackOptions(minTime=1,maxTime=72,loop='once',playbackSpeed=.5 if slow else 1)
    c.currentTime(1);c.play(forward=True)

def show_ui():
    for w in ['KiritoTransitionCompare',WINDOW]:
        if c.window(w,exists=True):c.deleteUI(w)
    c.window(WINDOW,title='桐人 · 爆发上勾拳 Demo',widthHeight=(335,225),sizeable=False)
    c.columnLayout(adjustableColumn=True,rowSpacing=9,columnAttach=('both',12))
    c.text(label='压低蓄力 → 蹬地出拳 → 腾空定格',height=34)
    c.button(label='播放 Demo',height=48,backgroundColor=(.26,.41,.47),command=lambda *_:play(False))
    c.button(label='半速看动作细节',height=34,command=lambda *_:play(True))
    c.button(label='停止',height=28,command=lambda *_:c.play(state=False))
    c.text(label='3 秒 · 保留原来的两个首尾姿势',height=24)
    c.showWindow(WINDOW)
    try:
        from PySide6 import QtWidgets,QtCore
        import shiboken6,maya.OpenMayaUI as mui
        main=shiboken6.wrapInstance(int(mui.MQtUtil.mainWindow()),QtWidgets.QWidget)
        w=shiboken6.wrapInstance(int(mui.MQtUtil.findWindow(WINDOW)),QtWidgets.QWidget)
        w.move(main.mapToGlobal(QtCore.QPoint(max(10,main.width()-w.width()-16),150)))
    except Exception:pass

def build():
    A=H['pose_from_ma'](OUT/'Kirito_Uppercut.ma');B=H['pose_from_ma'](OUT/'Kirito_Airborne_Uppercut.ma')
    c.autoKeyframe(state=False)
    c.cutKey(list(J.values()),at=['tx','ty','tz','rx','ry','rz'],clear=True)
    c.cutKey('Kirito_Expressions',clear=True)
    for i in range(32):c.setAttr('Kirito_Expressions.weight[%d]'%i,0)
    H['apply_pose'](A)
    feet={s:tuple(pos(J[s+'足首'])) for s in ['右','左']}
    foot_rotation={s:c.xform(J[s+'足首'],q=True,ws=True,ro=True) for s in feet}
    poses={1:copy.deepcopy(A),5:copy.deepcopy(A)}
    for frame,depth,twist in [(13,15,-27),(19,30,-42),(22,32,-45)]:
        H['apply_pose'](A)
        c.setAttr(J['センター']+'.translate',6, A[J['センター']]['t'][1]-depth,-depth*.45,type='double3')
        rotate('センター',(0,-8,-5))
        rotate('下半身',(0,-14,-2))
        rotate('上半身',(18+depth*.3,twist,8))
        rotate('上半身2',(5,8,2))
        rotate('首',(-15,18,-3));rotate('頭',(-10,8,-2))
        direct('右腕','右ひじ',(-.70,-.69,.18))
        direct('右ひじ','右手首',(.20,.10,.97))
        direct('左腕','左ひじ',(.54,-.75,.38))
        direct('左ひじ','左手首',(-.57,.68,.46))
        for side in feet:
            solve_leg(side,feet[side]);c.xform(J[side+'足首'],ws=True,ro=foot_rotation[side])
        coat(depth/32)
        for name,n in J.items():
            if any(x in name for x in ['指','捻']) and n in B:rotate(name,B[n]['r'])
        poses[frame]=snapshot()
    # The planted leg straightens while the fist still follows behind the torso.
    H['apply_pose'](A)
    c.setAttr(J['センター']+'.translate',3,A[J['センター']]['t'][1]-6,-4,type='double3')
    rotate('センター',(0,-3,4));rotate('下半身',(-3,8,2))
    rotate('上半身',(6,18,1));rotate('上半身2',(-7,-6,-2))
    rotate('首',(-5,-8,-2));rotate('頭',(-5,-3,-3))
    direct('右腕','右ひじ',(-.50,-.36,.79));direct('右ひじ','右手首',(-.12,.97,.20))
    direct('左腕','左ひじ',(.60,-.75,-.26));direct('左ひじ','左手首',(-.48,.66,.58))
    for side in feet:
        target=list(feet[side]);target[1]+=2
        solve_leg(side,target)
        r=list(foot_rotation[side]);r[0]+=12;c.xform(J[side+'足首'],ws=True,ro=r)
    coat(.60)
    for name,n in J.items():
        if any(x in name for x in ['指','捻']) and n in B:rotate(name,B[n]['r'])
    drive=snapshot();poses[26]=drive
    # Punch rises on a deliberate forward-to-upward arc, then overshoots slightly.
    for frame,u,height in [(29,.58,13),(32,.90,39),(36,1,57)]:
        for n in J.values():
            c.setAttr(n+'.translate',*mix(drive[n]['t'],B[n]['t'],u),type='double3')
            c.setAttr(n+'.rotate',*qmix(drive[n]['r'],B[n]['r'],u),type='double3')
        if frame==29:
            direct('右腕','右ひじ',(-.49,.55,.68));direct('右ひじ','右手首',(-.18,.96,.21))
        elif frame==32:
            direct('右腕','右ひじ',(-.38,.88,.28));direct('右ひじ','右手首',(-.15,.985,-.04))
        else:
            rotate('上半身',(-15,45,1));rotate('上半身2',(-10,-12,-6))
            rotate('センター',(0,-9,13))
            direct('右足','右ひざ',(-.34,.52,.78));direct('右ひざ','右足首',(.13,-.85,-.51))
        # Coat follows the body several frames later; its peak follows the fist.
        for name,n in J.items():
            if 'コート_' in name:
                lag={29:.15,32:.6,36:1.12}[frame]
                rotate(name,qmix(drive[n]['r'],B[n]['r'],min(1,lag)))
                if lag>1:
                    r=list(c.getAttr(n+'.rotate')[0]);r[2]*=1.12;rotate(name,r)
        c.setAttr(J['センター']+'.ty',c.getAttr(J['センター']+'.ty')+height-H['mesh_min']())
        poses[frame]=snapshot()
    poses[40]=copy.deepcopy(poses[36])
    poses[55]=copy.deepcopy(B);poses[72]=copy.deepcopy(B)
    # Bake editable skeleton channels; short release segments keep the punch fast.
    keys=sorted(poses)
    allnodes=list(J.values())
    for f in range(1,73):
        lo=max(k for k in keys if k<=f);hi=min(k for k in keys if k>=f)
        u=0 if lo==hi else (f-lo)/(hi-lo)
        if lo<22 or lo>=36:u=ease(u)
        p0,p1=poses[lo],poses[hi]
        for n in allnodes:
            c.setAttr(n+'.rotate',*qmix(p0[n]['r'],p1[n]['r'],u),type='double3')
        c.setAttr(J['センター']+'.translate',*mix(p0[J['センター']]['t'],p1[J['センター']]['t'],u),type='double3')
        # Keep both feet fixed through anticipation; interpolate the rest of the body.
        if 5<f<26:
            toe=ease((f-22)/4)
            for side in feet:
                target=list(feet[side]);target[1]+=2*toe
                solve_leg(side,target)
                r=list(foot_rotation[side]);r[0]+=12*toe;c.xform(J[side+'足首'],ws=True,ro=r)
        c.setKeyframe(allnodes,at=['rx','ry','rz'],t=f,itt='linear',ott='linear')
        c.setKeyframe(J['センター'],at=['tx','ty','tz'],t=f,itt='linear',ott='linear')
        force=ease((f-22)/10)*(1-.5*ease((f-40)/15))
        facial={'Angry':.6+.3*force,'Serious':.18*ease((f-8)/20),'Shout':.44*force,'Mouth_A':.12*(1-ease((f-20)/9))}
        for a,v in facial.items():c.setKeyframe('Kirito_Expressions.'+a,t=f,v=v,itt='linear',ott='linear')
    c.filterCurve(c.listConnections(allnodes,type='animCurveTA') or [],filter='euler')
    c.currentUnit(time='film')
    c.playbackOptions(minTime=1,maxTime=72,animationStartTime=1,animationEndTime=72,loop='once',playbackSpeed=1)
    c.viewPlace('persp',eye=(230,125,535),lookAt=(-9,113,-16),up=(0,1,0))
    c.setAttr('perspShape.focalLength',72)
    c.setAttr('defaultResolution.width',768);c.setAttr('defaultResolution.height',768);c.setAttr('defaultResolution.deviceAspectRatio',1)
    c.currentTime(55);c.select(clear=True)
    c.file(rename=str(OUT/'Kirito_Directed_Uppercut_Demo.ma'));c.file(save=True,type='mayaAscii',force=True)
    (OUT/'directed_demo_result.json').write_text(json.dumps({'scene':str(OUT/'Kirito_Directed_Uppercut_Demo.ma'),'method':'Assistant-authored skeletal keyframes, planted two-bone leg solves and delayed coat; no additional AI motion inference','fps':24,'range':[1,72],'beats':{'anticipation':[5,22],'push_off':[22,26],'strike':[26,36],'apex_hold':[36,40],'settle_to_original_endpoint':[40,55],'end_hold':[55,72]},'endpoints':['Kirito_Uppercut.ma','Kirito_Airborne_Uppercut.ma']},indent=2),encoding='utf-8')

if __name__=='__main__':
    try:build()
    except Exception:
        (OUT/'directed_demo_error.txt').write_text(traceback.format_exc(),encoding='utf-8');raise
