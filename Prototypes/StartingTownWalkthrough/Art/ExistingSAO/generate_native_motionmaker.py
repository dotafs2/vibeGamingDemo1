"""Generate genuine local Autodesk MotionMaker output, with unmodified source evidence."""
from pathlib import Path
import json, traceback, time, os
import maya.standalone
maya.standalone.initialize(name='python')
import maya.cmds as c
OUT=Path('C:/vibeGamingDemo1/Prototypes/StartingTownWalkthrough/Art/ExistingSAO/Maya')
try:
    c.loadPlugin('motionMaker')
    c.file('C:/vibeGamingDemo1/tmp/kirito_maya/motionmaker_setup.ma',open=True,force=True)
    c.currentUnit(time='film')
    c.playbackOptions(minTime=0,maxTime=96,animationStartTime=0,animationEndTime=96)
    ac='motionAI:basic_maleAction'; gn='motionAI:generator'; pl='motionAI:pathLocator'
    c.setAttr(ac+'.timeRanges[0].rangeStart',0)
    c.setAttr(ac+'.timeRanges[0].rangeEnd',96)
    for i,(f,tag) in enumerate([(0,0),(24,1),(48,0)]):
        c.setAttr(ac+'.actionGroups[0].keys[%d].keyStart'%i,f)
        c.setAttr(ac+'.actionGroups[0].keys[%d].keyTag'%i,tag)
    for f,y,z in [(0,0,0),(18,0,0),(48,0,70),(72,0,140),(96,0,140)]:
        for a,v in [('tx',0),('ty',y),('tz',z)]:c.setKeyframe(pl,attribute=a,time=f,value=v,inTangentType='auto',outTangentType='auto')
    c.currentTime(0)
    started=time.time()
    (OUT/'native_motionmaker_status.json').write_text(json.dumps({'stage':'generating','pid':os.getpid()}))
    code=c.motionMaker(gn,generate=True,asi=0,sf=0,ef=96)
    if code[0]!=0:raise RuntimeError('MotionMaker returned '+repr(code))
    names=c.motionMaker('motionAI:biped',info='chardef_joints')
    nodes=c.motionMaker('motionAI:biped',info='joint_nodes')
    joints={n:j for n,j in zip(names,nodes) if not any(x in n for x in ['thumb','index','middle','ring','pinky'])}
    frames=[]
    for f in range(97):
        c.currentTime(f)
        frames.append({'frame':f,'joints':{n:{'p':c.xform(j,q=True,ws=True,t=True),'r':c.xform(j,q=True,ws=True,ro=True)} for n,j in joints.items()}})
    c.file(rename=str(OUT/'MotionMaker_Native_Jump_Source.ma'))
    c.file(save=True,type='mayaAscii',force=True)
    result={'engine':'Autodesk Maya 2026.2 MotionMaker','native_result_code':code,'style':'adsk_biped/basic_male','tags':[[0,'default'],[24,'jump'],[48,'default']],'fps':24,'generation_seconds':time.time()-started,'frames':frames}
    (OUT/'native_motionmaker_jump.json').write_text(json.dumps(result),encoding='utf-8')
    (OUT/'native_motionmaker_status.json').write_text(json.dumps({'stage':'complete','native_result_code':code,'seconds':time.time()-started}))
except Exception:
    (OUT/'native_motionmaker_error.txt').write_text(traceback.format_exc(),encoding='utf-8')
finally:
    maya.standalone.uninitialize()
