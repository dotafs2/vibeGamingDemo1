"""Exaggerated airborne uppercut and actual Maya studio lighting."""
from pathlib import Path
import json,math,traceback
import maya.cmds as c
import maya.api.OpenMaya as om
import maya.api.OpenMayaUI as ui

OUT=Path('C:/vibeGamingDemo1/Prototypes/StartingTownWalkthrough/Art/ExistingSAO/Maya')
DATA=json.loads((OUT/'conversion_validation.json').read_text(encoding='utf-8'))
J=DATA['joint_names']

def pos(node):return om.MVector(*c.xform(node,q=True,ws=True,t=True))
def rotation(node):
    s=om.MSelectionList();s.add(node)
    return om.MFnTransform(s.getDagPath(0)).rotation(om.MSpace.kWorld,asQuaternion=True)
def aim(name,child,direction):
    node=J[name];q=rotation(node)
    now=(pos(J[child])-pos(node)).normal();target=om.MVector(*direction).normal()
    change=now.rotateTo(target);local=now.rotateBy(q.inverse())
    result=max((q*change,change*q),key=lambda r:local.rotateBy(r)*target)
    c.xform(node,ws=True,rotation=[math.degrees(x) for x in result.asEulerRotation()])
def rot(name,angles):c.setAttr(J[name]+'.rotate',*angles,type='double3')
def capture():
    c.refresh(force=True)
    image=om.MImage();ui.M3dView.active3dView().readColorBuffer(image,True)
    image.writeToFile(str(OUT/'airborne_uppercut_viewport.png'),'png')

def material_look():
    for info in DATA['materials']:
        old=info['node'];name=info['source_name']
        if not c.objExists(old):continue
        c.setAttr(old+'.ambientColor',.10,.10,.10,type='double3')
        c.setAttr(old+'.diffuse',.8)
        if not any(x in name for x in ['コート','髪','手袋','金具','ベルト','靴','ズボン']):
            c.setAttr(old+'.ambientColor',.16,.16,.16,type='double3')
            continue
        if info['alpha']<.01:continue
        new=c.shadingNode('blinn',asShader=True,name=old+'_Studio')
        for attr in ['color','transparency','ambientColor']:
            incoming=c.listConnections(old+'.'+attr,s=True,d=False,plugs=True) or []
            if incoming:c.connectAttr(incoming[0],new+'.'+attr,f=True)
            else:c.setAttr(new+'.'+attr,*c.getAttr(old+'.'+attr)[0],type='double3')
        c.setAttr(new+'.diffuse',.78)
        metal='金具' in name
        hair='髪' in name
        spec=(.30,.34,.40) if metal else ((.065,.075,.10) if hair else (.09,.11,.15))
        c.setAttr(new+'.specularColor',*spec,type='double3')
        c.setAttr(new+'.eccentricity',.20 if metal else .38)
        c.setAttr(new+'.specularRollOff',.4)
        for sg in c.listConnections(old+'.outColor',s=False,d=True,type='shadingEngine') or []:
            c.connectAttr(new+'.outColor',sg+'.surfaceShader',f=True)

def spot(name,position,target,color,power,cone=90,shadow=False):
    shape=c.spotLight(name=name,rgb=color,intensity=power,coneAngle=cone,penumbra=15,decayRate=0)
    transform=c.listRelatives(shape,parent=True)[0]
    c.xform(transform,ws=True,t=position)
    aim_target=c.spaceLocator(name='Temporary_Light_Target')[0]
    c.xform(aim_target,ws=True,t=target)
    constraint=c.aimConstraint(aim_target,transform,aimVector=(0,0,-1),upVector=(0,1,0),worldUpType='vector',worldUpVector=(0,1,0))[0]
    c.delete(constraint,aim_target)
    c.parent(transform,'Uppercut_Studio')
    c.setAttr(shape+'.useDepthMapShadows',shadow)
    if shadow:
        c.setAttr(shape+'.dmapResolution',2048)
        c.setAttr(shape+'.dmapFilterSize',5)
        c.setAttr(shape+'.dmapBias',.04)
    return transform

def main():
    dest=OUT/'Kirito_Airborne_Uppercut.ma'
    if dest.exists():
        from datetime import datetime
        dest=OUT/('Kirito_Airborne_Uppercut_'+datetime.now().strftime('%H%M%S')+'.ma')
    c.play(state=False)
    c.file(rename=str(dest));c.file(save=True,type='mayaAscii')
    auto=c.autoKeyframe(q=True,state=True);c.autoKeyframe(state=False)
    try:
        if c.window('KiritoPosePreviewWindow',exists=True):c.deleteUI('KiritoPosePreviewWindow')
        if c.objExists('Kirito_Pose_Comparison'):c.setAttr('Kirito_Pose_Comparison.visibility',False)
        c.setAttr('Kirito.visibility',True)
        c.setAttr('Kirito_Skeleton.visibility',False)
        c.cutKey(list(J.values()),attribute=['rotateX','rotateY','rotateZ'],clear=True)
        c.cutKey('Kirito_Expressions',clear=True)
        for node in J.values():c.setAttr(node+'.rotate',0,0,0,type='double3')
        for i in range(32):c.setAttr('Kirito_Expressions.weight[%d]'%i,0)
        c.currentTime(1)
        c.setAttr(J['センター']+'.translateY',45)
        rot('センター',(0,-8,10))
        rot('下半身',(2,-24,3))
        rot('上半身',(-10,37,2))
        rot('上半身2',(-8,-10,-5))
        rot('首',(-7,-8,-2))
        rot('頭',(-8,-5,-5))
        # A forceful diagonal reach, with the opposite arm bent at the ribs.
        aim('右腕','右ひじ',(-.38,.83,.40))
        aim('右ひじ','右手首',(-.22,.97,.03))
        aim('左腕','左ひじ',(.62,-.67,-.12))
        aim('左ひじ','左手首',(-.48,.59,.64))
        # One knee raised high; the other leg trails behind the rising body.
        aim('右足','右ひざ',(-.36,.40,.84))
        aim('右ひざ','右足首',(.13,-.81,-.57))
        aim('左足','左ひざ',(.30,-.87,-.40))
        aim('左ひざ','左足首',(-.03,-.69,-.72))
        c.xform(J['右足首'],ws=True,rotation=(27,-17,-12))
        c.xform(J['左足首'],ws=True,rotation=(34,15,8))
        # Close each finger separately, then lay the thumb across the fist.
        for side,sign in [('右',1),('左',-1)]:
            for finger,extra in [('人指',0),('中指',3),('薬指',6),('小指',9)]:
                for part,angle in [('１',78+extra),('２',94),('３',65)]:
                    c.setAttr(J[side+finger+part]+'.rotateZ',sign*angle)
            wrist_q=rotation(J[side+'手首'])
            first=om.MVector(-sign*.55,-.48,-.68).rotateBy(wrist_q)
            second=om.MVector(-sign*.10,-.12,-1).rotateBy(wrist_q)
            aim(side+'親指０',side+'親指１',tuple(first))
            aim(side+'親指１',side+'親指２',tuple(second))
            rot(side+'親指２',(12,0,sign*25))
        c.setAttr('Kirito_Expressions.Angry',.75)
        c.setAttr('Kirito_Expressions.Serious',.18)
        c.setAttr('Kirito_Expressions.Shout',.22)
        # Fan the long coat behind the body and away from the raised knee.
        for row in range(7):
            for column in range(12):
                name='コート_%d_%d'%(row,column)
                if name not in J:continue
                angle=column*math.tau/12
                side=math.sin(angle)
                front=max(0,math.cos(angle))
                backward=(19+7*(1-front)) if row==0 else (4 if row<4 else -2)
                opening=(25+11*front)*side if row==0 else (3.5*side if row<4 else 0)
                rot(name,(backward,(-5*side if row==0 else 0),opening))
        # A restrained lift in the hair ends supports the upward motion.
        for name in ['前髪2','前髪3','右横髪2','左横髪2','後髪2','後髪3']:
            if name in J:rot(name,(-5,0,0))
        body_shape=c.listRelatives('Kirito_Body',shapes=True,noIntermediate=True,fullPath=True)[0]
        selection=om.MSelectionList();selection.add(body_shape)
        mesh_fn=om.MFnMesh(selection.getDagPath(0))
        posed=mesh_fn.getPoints(om.MSpace.kWorld)
        c.setAttr(J['センター']+'.translateY',c.getAttr(J['センター']+'.translateY')+45-min(v.y for v in posed))
        posed=mesh_fn.getPoints(om.MSpace.kWorld)
        middle=[(min(getattr(v,a) for v in posed)+max(getattr(v,a) for v in posed))/2 for a in 'xyz']
        material_look()
        c.group(empty=True,name='Uppercut_Studio')
        floor=c.polyPlane(name='Studio_Ground',width=2500,height=2500,subdivisionsX=1,subdivisionsY=1,constructionHistory=False)[0]
        c.parent(floor,'Uppercut_Studio')
        mat=c.shadingNode('lambert',asShader=True,name='Studio_DeepBlue_Matte')
        c.setAttr(mat+'.color',.055,.075,.10,type='double3')
        c.setAttr(mat+'.diffuse',.8)
        sg=c.sets(renderable=True,noSurfaceShader=True,empty=True,name=mat+'SG')
        c.connectAttr(mat+'.outColor',sg+'.surfaceShader',f=True);c.sets(floor,e=True,forceElement=sg)
        spot('Key_Warm',(-170,350,290),(-15,163,5),(1,.86,.70),1.45,95,True)
        spot('Fill_SoftBlue',(240,190,280),(0,158,0),(.62,.77,1),.58,105,False)
        spot('Rim_IceBlue',(100,305,-170),(-15,167,0),(.30,.67,1),2.6,85,True)
        spot('Edge_Warm',(-200,220,-80),(-15,160,0),(1,.55,.28),.55,90,False)
        ambient=c.ambientLight(name='Studio_Ambient',intensity=.16,rgb=(.68,.77,1))
        c.parent(c.listRelatives(ambient,p=True)[0],'Uppercut_Studio')
        for attr,value in [('multiSampleEnable',1),('multiSampleCount',8),('ssaoEnable',1),('ssaoAmount',.45),('ssaoRadius',12),('ssaoSamples',16)]:
            if c.objExists('hardwareRenderingGlobals.'+attr):
                c.setAttr('hardwareRenderingGlobals.'+attr,value)
        c.displayRGBColor('background',.022,.034,.055)
        c.displayRGBColor('backgroundTop',.042,.063,.095)
        c.displayRGBColor('backgroundBottom',.012,.020,.035)
        c.displayPref(displayGradient=True)
        c.viewPlace('persp',eye=(middle[0]+275,middle[1]-24,middle[2]+610),lookAt=middle,up=(0,1,0))
        c.setAttr('perspShape.focalLength',55)
        c.setAttr('perspShape.backgroundColor',.022,.034,.055,type='double3')
        cam,shape=c.camera(name='Uppercut_Hero',focalLength=55)
        c.xform(cam,ws=True,matrix=c.xform('persp',q=True,ws=True,matrix=True))
        c.setAttr(shape+'.backgroundColor',.022,.034,.055,type='double3')
        c.setAttr(shape+'.nearClipPlane',.1)
        c.setAttr('defaultResolution.width',1440);c.setAttr('defaultResolution.height',1440)
        c.setAttr('defaultResolution.deviceAspectRatio',1)
        for panel in c.getPanel(type='modelPanel'):
            c.modelEditor(panel,e=True,displayLights='all',displayTextures=True,displayAppearance='smoothShaded',twoSidedLighting=True,shadows=True,joints=False,lights=False,cameras=False,grid=False)
        c.select(clear=True);capture()
        c.file(save=True,type='mayaAscii')
        (OUT/'airborne_uppercut_result.json').write_text(json.dumps({'scene':str(dest),'method':'assistant-authored static skeletal pose','lighting':'four spot lights and ambient, actual scene lighting','pose':'airborne uppercut; extended right fist, raised right knee, trailing left leg','physics_simulated':False},indent=2),encoding='utf-8')
    finally:c.autoKeyframe(state=auto)

try:main()
except Exception:
    (OUT/'airborne_uppercut_error.txt').write_text(traceback.format_exc(),encoding='utf-8')
    raise
