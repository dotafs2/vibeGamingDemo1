"""Clustered planting with a branching hero tree; fixed physical proportions."""
import math,random
from mathutils import Vector
import architecture as a
import build_reference_scenes_hq as hq
G=hq.G
R=random.Random(925)

def tree(col):
    p=Vector((-2.5,4,.22));g=G();leaves=G()
    trunk=[(0,0,0),(.22,.08,1.3),(.47,.02,2.8),(.8,.04,4.3),(.53,.15,6.0),(.66,.35,7.7)]
    g.tube([p+Vector(v) for v in trunk],[.64,.54,.45,.37,.29,.16],'bark',24)
    for j in range(8):
        t=j*math.tau/8;g.tube([p+Vector((0,0,.70)),p+Vector((.67*math.cos(t),.60*math.sin(t),.19)),p+Vector((1.35*math.cos(t),1.30*math.sin(t),.015))],[.16,.17,.015],'bark',12)
    for j in range(22):
        t=j*math.tau/22;coords=[]
        for k,v in enumerate(trunk):
            rr=[.64,.54,.45,.37,.29,.16][k]+.004;coords.append(p+Vector(v)+Vector((rr*math.cos(t+.025*math.sin(j+k)),rr*math.sin(t+.025*math.sin(j+k)),0)))
        g.tube(coords,[.018,.015,.012,.010,.009,.002],'bark_dark' if j%3 else 'bark_light',6)
    crowns=[]
    for branch in range(11):
        t=branch*2.399;dist=R.uniform(3.0,5.3);height=R.uniform(10.1,13.1)
        if branch>=9:dist=1.3;height=14.0+(branch-9)*.45
        start=p+Vector(trunk[3+branch%2]);end=p+Vector((math.cos(t)*dist,math.sin(t)*dist*.85,height));mid=start.lerp(end,.5)+Vector((0,0,.8))
        g.tube([start,mid,end],[.25-.012*branch,.13,.025],'bark',16)
        for j in range(4):
            angle=t+j*1.9;tip=end+Vector((math.cos(angle)*R.uniform(.9,2.0),math.sin(angle)*R.uniform(.8,1.8),R.uniform(-.55,.85)))
            twig=end.lerp(tip,.55)+Vector((0,0,.24));g.tube([mid.lerp(end,.7),twig,tip],[.065,.027,.004],'bark',8)
            crowns.append((tip,Vector((R.uniform(1.1,1.65),R.uniform(1.0,1.5),R.uniform(.85,1.25)))))
    for center,size in crowns:
        for j in range(630):
            # Small offset clusters give canopy edges varied silhouettes and gaps.
            v=Vector((R.gauss(0,.49),R.gauss(0,.49),R.gauss(0,.48)))
            if v.length>1.1:v.normalize()
            q=center+Vector((v.x*size.x,v.y*size.y,v.z*size.z));ang=R.random()*math.tau;length=R.uniform(.12,.21)
            u=Vector((math.cos(ang),math.sin(ang),R.uniform(-.5,.5))).normalized()*length
            vaxis=Vector((-math.sin(ang),math.cos(ang),.18))*length*.49
            role=R.choice(['leaf_light','leaf_mid','leaf']) if q.z>center.z else R.choice(['leaf','leaf_dark','leaf_deep'])
            leaves.mesh([q-u,q-u*.3+vaxis,q+u*.48+vaxis*.8,q+u,q+u*.48-vaxis*.8,q-u*.3-vaxis,q+Vector((0,0,.027))],[(6,k,(k+1)%6) for k in range(6)],role)
    a.emit(g,col,'TolbanaV5_tree_roots_bark_branchwork',.003);a.emit(leaves,col,'TolbanaV5_tree_clustered_leaves',0)
    c=G();c.cylinder(p,.54,5,'bark',20);a.emit(c,col,'COL_TolbanaV5_tree_trunk',0,True)

def grass(col):
    g=G()
    for j in range(16000):
        x=R.uniform(-20.7,19.6);y=R.uniform(-12.6,17.5)
        if math.hypot(x-8.9,y+5)<4.5 or (7.6<x<11.4 and y>-2) or math.hypot(x+2.5,y-4)<1.25:continue
        density=.23+.27*(.5+.5*math.sin(x*.83+math.sin(y*.72)*1.6))*(.5+.5*math.sin(y*.68-x*.22))
        edge=min(x+20.7,19.6-x,y+12.6,17.5-y)
        if edge<.9:density+=.32
        if R.random()>density:continue
        shade=(x+2.5)**2+(y-4)**2<45
        for k in range(R.randint(3,7)):
            xx=x+R.uniform(-.1,.1);yy=y+R.uniform(-.1,.1);height=R.uniform(.08,.23);ang=R.random()*math.tau
            across=Vector((-math.sin(ang),math.cos(ang),0))*.012;down=Vector((math.cos(ang),math.sin(ang),0));q=Vector((xx,yy,.20))
            tip=q+down*height*.43+Vector((0,0,height));mid=q+down*height*.12+Vector((0,0,height*.55))
            g.mesh([q-across,q+across,mid+across*.6,tip,mid-across*.6],[(0,1,2,4),(4,2,3)],'grass_dark' if shade and k%2 else 'grass' if k%3 else 'grass_light')
    a.emit(g,col,'TolbanaV5_variable_grass_groups',0)

def fountain_paving(col):
    g=G();c=G();cx,cy=8.9,-5
    for band in range(3):
        lo=2.7+band*.59;hi=lo+.575;n=round(math.tau*(lo+hi)/2/.64)
        for j in range(n):
            start=(j+.013+band*.5)*math.tau/n;end=(j+.987+band*.5)*math.tau/n
            vs=[(cx+rr*math.cos(t),cy+rr*math.sin(t),z) for z in (.17,.26) for rr,t in [(lo,start),(hi,start),(hi,end),(lo,end)]]
            g.mesh(vs,[(0,3,2,1),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)],['paving','paving_light','paving_warm'][(j+band)%3])
    a.emit(g,col,'TolbanaV5_fountain_radial_stone_paving',.009)
    c.cylinder((cx,cy,.13),4.5,.13,'paving',80);a.emit(c,col,'COL_TolbanaV5_fountain_paving',0,True)
