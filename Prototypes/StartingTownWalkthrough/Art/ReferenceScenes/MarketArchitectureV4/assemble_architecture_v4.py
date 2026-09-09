"""Individually traced solid buildings for the existing Level0 market site."""
import bpy,math,sys,json,hashlib,os
from pathlib import Path
from mathutils import Vector
HERE=Path(__file__).resolve().parent
sys.path.insert(0,str(HERE));sys.path.insert(0,str(HERE.parent));sys.path.insert(0,str(HERE.parent/'MarketDemoV3'))
import v4_geometry as geo
import central_architecture as central
import build_reference_scenes_hq as hq
import build_reference_scenes as base
import assemble_market_demo as old
G=geo.G;PROJECT=HERE.parents[2];EXPORT=PROJECT/'assets/reference_scenes';OUT=PROJECT/'validation/market_architecture_v4'
base.PALETTE.update({'arch_stone':'B7AA93','rail_red':'805A50','plaster_blue':'ABBDBB','canvas_blue':'75AEB7','canvas_leaf':'658551','tile_red':'AF817A','canvas_underside':'7E4B3D','canvas_shadow':'434E40'})
MEASURE=[];SPECS=[]

def windows(items,kind='cinched',material='recess'):
    return [{'id':'window_%02d'%i,'outline_px':geo.window_shape(cx,t,b,w,shape if shape else kind),'material':material,'bars':material=='shutter'} for i,(cx,t,b,w,shape) in enumerate(items)]

def make_facades(col):
    near=windows([(271,49,189,25,None),(343,94,244,25,None),(403,140,285,23,None),(459,175,321,22,None),(402,-8,48,18,None),(449,30,97,19,None),(488,64,135,18,None),(525,96,158,17,None)])
    arcade=[(285,468,561,28,'arch'),(351,495,584,28,'arch'),(409,516,604,26,'arch'),(461,535,621,25,'arch'),(510,549,632,24,'arch'),(556,564,644,23,'arch')]
    for w in windows(arcade):w['id']='arcade_'+w['id'];near.append(w)
    for w in windows([(190,776,869,54,'rect'),(310,786,870,48,'rect'),(406,793,871,43,'rect')],material='shutter'):w['id']='shop_'+w['id'];near.append(w)
    SPECS.append({'id':'left_near','plane_value':-5.8,'outward_sign':1,'contour_px':[(-250,-550),(557,-40),(557,1500),(-250,1800)],'windows':near,'material':'plaster_cream','course_height':.50,'bands':[{'line_px':[(-20,142),(557,400)],'width_m':.10}]})
    mid=windows([(765,395,555,30,None),(812,425,578,24,None),(852.5,450,596,19,None),(887,473,611,18,None),(783,357,380,13,'rect'),(808,377,400,12,'rect'),(837,398,418,11,'rect')])
    for w in windows([(596,578,656,23,'arch'),(631,591,665,22,'arch'),(666,607,676,21,'arch'),(699,620,688,20,'arch'),(730,632,698,19,'arch'),(759,645,706,18,'arch'),(785,652,711,17,'arch'),(806,661,718,16,'arch')]):w['id']='arcade_'+w['id'];mid.append(w)
    for w in windows([(568,804,1040,57,'lancet'),(649,809,1055,51,'lancet'),(798,815,866,22,'rect')],material='shutter'):w['id']='shop_'+w['id'];mid.append(w)
    # The near portion lies behind the tall first building; only its lower arcade appears.
    SPECS.append({'id':'left_middle','plane_value':-5.70,'outward_sign':1,'contour_px':[(557,350),(616,253),(665,250),(914,415),(914,1220),(557,1500)],'windows':mid,'material':'plaster_peach','bands':[{'line_px':[(665,280),(914,443)],'width_m':.11},{'line_px':[(560,565),(913,730)],'width_m':.07}]})
    far=windows([(927,590,630,15,'arch'),(957,608,647,14,'arch'),(981,620,659,13,'arch')])
    for w in windows([(889,802,1020,31,'lancet'),(938,814,1010,28,'lancet'),(978,830,1010,25,'lancet')],material='shutter'):w['id']='shop_'+w['id'];far.append(w)
    SPECS.append({'id':'left_low_shop','plane_value':-5.9,'outward_sign':1,'contour_px':[(880,530),(1034,607),(1034,1100),(880,1230)],'windows':far,'material':'plaster_white','bands':[{'line_px':[(880,544),(1034,620)],'width_m':.14,'material':'tile_red'}]})
    right=windows([(1982,199,352,27,None),(2020,155,316,29,None),(2065,113,289,30,None),(2128,54,235,35,None)])
    for w in windows([(2012,655,984,32,'lancet'),(2081,610,1005,42,'lancet'),(2174,570,1030,47,'lancet')],material='shutter'):w['id']='tall_'+w['id'];right.append(w)
    for w in windows([(1900,795,1120,42,'rect')],material='door_blue'):w['id']='door';right.append(w)
    SPECS.append({'id':'right_near','plane_value':7.4,'outward_sign':-1,'contour_px':[(1918,156),(2253,-200),(2253,1610),(1918,1190)],'windows':right,'material':'limestone','course_height':.48,'bands':[{'line_px':[(1920,525),(2250,325)],'width_m':.10},{'line_px':[(1920,570),(2250,379)],'width_m':.07}]})
    SPECS.append({'id':'right_far','plane_value':7.6,'outward_sign':-1,'contour_px':[(1735,386),(1877,180),(1920,158),(1920,1200),(1735,1040)],'windows':windows([(1785,369,489,17,None),(1820,332,476,20,None),(1844,297,448,23,None)]),'material':'plaster_sage','bands':[{'line_px':[(1735,584),(1920,497)],'width_m':.09}]})
    front_y=geo.point_px((2253,500),0,7.4).y
    SPECS.append({'id':'right_forward_face','plane_axis':1,'plane_value':front_y,'outward_sign':-1,'contour_px':[(2253,-200),(2800,-200),(2800,1780),(2253,1610)],'windows':[],'material':'stone_light','course_height':.50,'building_depth':7,'omit_return_edges':[3]})
    for s in SPECS:geo.facade(col,s,MEASURE)
    # Projected slender parapet teeth follow the two actual roof slopes.
    parapet=G()
    for x0,v0,x1,v1,count,plane,role in [(665,250,914,415,22,-5.7,'tile_red'),(1735,386,1877,180,15,7.6,'limestone'),(1918,156,2253,-200,18,7.4,'limestone')]:
        for i in range(count):
            t=i/count;px=x0+(x1-x0)*t;py=v0+(v1-v0)*t;p=geo.point_px((px,py),0,plane)
            parapet.box(tuple(p+Vector((0,0,.15))),(.22,.14,.30),role)
        a=geo.point_px((x0,v0),0,plane);b=geo.point_px((x1,v1),0,plane);parapet.beam(a,b,.12,role)
    # Projected near masonry quoin and middle right pilaster.
    for points,axis,value,outward,role in [([(557,-30),(607,42),(607,355),(557,383)],0,-5.71,1,'stone_light'), ([(1877,150),(1920,151),(1920,558),(1877,572)],0,7.48,-1,'stone_light')]:
        geo.facade(col,{'id':'quoin_'+str(axis)+'_'+str(value),'plane_axis':axis,'plane_value':value,'outward_sign':outward,'contour_px':points,'windows':[],'material':role,'building_depth':.18,'course_height':.43},MEASURE)
    geo.emit(parapet,col,'V4_individual_parapets',.006)

def fabric_panel(col,name,upper,lower,upper_x,lower_x,role,fold=.06):
    """Real cloth patch between separately traced attachment and free edges."""
    g=G();g.smoothing=True;nu=64;nv=22
    def sample(poly,t):
        k=min(len(poly)-2,int(t*(len(poly)-1)));s=t*(len(poly)-1)-k
        return(poly[k][0]*(1-s)+poly[k+1][0]*s,poly[k][1]*(1-s)+poly[k+1][1]*s)
    def point(s,t):
        a=geo.point_px(sample(upper,s),0,upper_x);b=geo.point_px(sample(lower,s),0,lower_x);p=a.lerp(b,t)
        p.x+=fold*math.sin(6*math.pi*s+2*t)*math.sin(math.pi*t)*math.sin(math.pi*s)
        p.z-=fold*.55*math.sin(math.pi*s)**2*math.sin(math.pi*t);return p
    vs=[point(i/nu,j/nv) for j in range(nv+1) for i in range(nu+1)];fs=[(j*(nu+1)+i,j*(nu+1)+i+1,(j+1)*(nu+1)+i+1,(j+1)*(nu+1)+i) for j in range(nv) for i in range(nu)]
    g.mesh(vs,fs,role);ob=geo.emit(g,col,name,0);sol=ob.modifiers.new('Cloth thickness','SOLIDIFY');sol.thickness=.012
    seam=G()
    for t in (0,1):seam.tube([point(i/nu,t) for i in range(nu+1)],[.005]*(nu+1),role,5)
    geo.emit(seam,col,name+'_sewn_edges',0)
    for tag,poly,x in [('upper',upper,upper_x),('lower',lower,lower_x)]:MEASURE.append({'id':name+'/'+tag,'kind':'cloth_edge','reference_px':poly,'points_blender_m':[list(geo.point_px(p,0,x)) for p in poly]})

def canopies(col):
    # Three independent gold cloth bays, with the large red underside behind.
    for name,coords,role in [('gold_shadowed_roof',[((-120,50),-5.8),((394,474),-3.75),((-120,415),-3.75)],'canvas_shadow'),('gold_dark_underside',[((-120,415),-3.75),((394,474),-3.75),((-120,505),-3.73)],'canvas_underside')]:
        roof=G();roof.mesh([geo.point_px(p,0,x) for p,x in coords],[(0,1,2)],role)
        ob=geo.emit(roof,col,name,0);sol=ob.modifiers.new('Canopy cloth thickness','SOLIDIFY');sol.thickness=.018
    for i,(a,b,c,d) in enumerate([((249,216),(394,311),(249,443),(394,474)),((394,311),(525,373),(394,474),(525,534)),((525,373),(629,400),(525,534),(629,566))]):
        fabric_panel(col,'gold_hanging_%d'%i,[a,b],[c,((c[0]+d[0])/2,(c[1]+d[1])/2+18),d],-3.75,-3.73,'canvas_gold',.05)
    fabric_panel(col,'white_sloping_roof',[(-130,470),(240,532),(442,605)],[(-130,625),(190,616),(442,605)],-5.8,-2.75,'canvas_cream',.05)
    fabric_panel(col,'white_deep_drape',[(-130,625),(190,616),(442,605)],[(-130,762),(160,762),(420,758),(442,744)],-2.75,-2.71,'canvas_cream',.10)
    fabric_panel(col,'green_sloping_roof',[(442,605),(620,651),(798,696)],[(442,695),(622,722),(798,708)],-5.70,-2.75,'canvas_green',.065)
    fabric_panel(col,'green_gathered_drape',[(442,695),(620,722),(798,708)],[(442,743),(580,758),(734,769),(795,756)],-2.75,-2.72,'canvas_green',.09)
    fabric_panel(col,'green_end_valance',[(797,711),(852,737)],[(797,778),(852,787)],-2.75,-2.72,'canvas_leaf',.02)
    fabric_panel(col,'middle_cream_roof',[(851,683),(952,726),(1050,747)],[(851,721),(952,765),(1050,779)],-5.9,-3.1,'canvas_cream',.03)
    fabric_panel(col,'middle_green_valance',[(851,721),(952,765),(1050,779)],[(851,770),(952,795),(1050,806)],-3.1,-3.07,'canvas_leaf',.045)
    fabric_panel(col,'right_blue_roof',[(1654,704),(1840,663)],[(1470,722),(1650,762),(1840,760)],7.4,3.4,'canvas_blue',.06)
    fabric_panel(col,'right_cream_roof',[(1850,659),(2060,645)],[(1640,705),(1810,755),(2060,751)],7.4,4.6,'canvas_cream',.06)
    # Slender supports placed from actual edges, not default repeated posts.
    g=G()
    for px,x in [((852,787),-2.75),((1050,806),-3.1),((1840,760),3.4)]:
        p=geo.point_px(px,0,x);g.tube([(p.x,p.y,.24),p],[.022,.022],'wood_dark',10)
    geo.emit(g,col,'V4_awning_supports',.002)

def banners(col):
    g=G()
    for a,b,y in [((460,300),(1610,-22),-4),((980,468),(1740,540),20),((1610,654),(1940,646),18)]:
        pa=central.at(*a,y);pb=central.at(*b,y);g.tube([pa,pb],[.011,.011],'wood_dark',8)
    for x,y,depth,w,h in [(567,285,-4,45,85),(648,252,-4,40,85),(1018,473,20,38,63),(1070,480,20,36,65),(1480,526,20,38,62),(1530,530,20,38,66),(1612,538,20,39,62)]:
        for j,role in enumerate(['canvas_red','canvas_cream','canvas_green']):
            xx=x+j*w/3;points=[central.at(xx,y,depth),central.at(xx+w/3,y,depth),central.at(xx+w/3,y+h,depth+.04),central.at(xx,y+h,depth)]
            g.mesh(points,[(0,1,2,3)],role)
    geo.emit(g,col,'V4_sparse_reference_banners',0)

def main():
    print(json.dumps({'pid':os.getpid(),'stage':'architecture_v4'}),flush=True)
    bpy.ops.wm.read_factory_settings(use_empty=True);sc=hq.scene('StartingTown_Market_ArchitectureV4');col=sc.collection
    hq.camera(sc,'Anime_Market_V4_Camera',geo.CAM,geo.TARGET,geo.HFOV)
    ground=G();ground.box((0,25,-.08),(55,125,.5),'paving');geo.emit(ground,col,'V4_ground',.002);geo.emit(ground,col,'COL_V4_ground',0,True)
    hq.stone_path('V4_paving',col,(-6,7.5,-22,55),.20,.70)
    make_facades(col);markers=central.build(col,MEASURE);canopies(col);banners(col)
    old.props(col);actors=old.people(col)
    # Continuing street visible through the bridge, preserving a passage.
    for j,(x,y,height) in enumerate([(-4.1,66,8.7),(2.2,72,7.8),(8.3,68,9.7)]):
        hq.facade('V4_beyond_gate_'+str(j),col,6.1,7,height,[(-1.6,3.8,.7,1.4,False,True),(1.1,3.8,.7,1.4,False,True),(-1.6,6.1,.65,1.25,False,False),(1.1,6.1,.65,1.25,False,False)],(x,y,.24),0,False,'plaster_white','terrace',0,False,True)
    for ob in sc.objects:ob.select_set(ob.type=='MESH')
    path=EXPORT/'StartingTown_Market_ArchitectureV4.glb'
    bpy.ops.export_scene.gltf(filepath=str(path),export_format='GLB',use_selection=True,use_active_scene=True,export_extras=True,export_cameras=False,export_lights=False,export_yup=True,export_apply=True)
    visual=collision=0
    for ob in sc.objects:
        if ob.type=='MESH':
            ob.data.calc_loop_triangles();count=len(ob.data.loop_triangles)
            if ob.name.startswith('COL_'):collision+=count;ob.hide_render=True;ob.hide_set(True)
            else:visual+=count
    entry={'id':'StartingTown_Market','region_id':'beginnings_plaza','floor_east_north_m':[-163,-4752],'godot_position_m':[-163,0,-18],'godot_yaw_degrees':78,'glb':path.name,'sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'triangles':visual,'collision_triangles':collision,'collision_mesh_names':[o.name for o in sc.objects if o.name.startswith('COL_')],'reference_camera_blender_m':list(geo.CAM),'reference_target_blender_m':list(geo.TARGET),'reference_horizontal_fov':geo.HFOV,'mesh_objects':sum(o.type=='MESH' for o in sc.objects),'reference_landmarks_3d':markers,'reference_features_3d':MEASURE,'visual_actor_count':len(actors),'walkable_gate':True}
    manifest={'schema':3,'style_id':'level0_market_architecture_v4','scenes':[entry],'visual_approval':False,'composition_method':'individual_reference_contours_extruded_on_world_building_planes','source_files_sha256':{name:hashlib.sha256((HERE/name).read_bytes()).hexdigest() for name in ['assemble_architecture_v4.py','v4_geometry.py','central_architecture.py']}}
    for folder in [HERE,EXPORT]:(folder/'market_architecture_v4_manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf-8')
    (OUT/'authored_facades.json').write_text(json.dumps({'active_picture_px':[2560,1440],'specs':SPECS,'features':MEASURE},ensure_ascii=False,indent=2),encoding='utf-8')
    bpy.ops.wm.save_as_mainfile(filepath=str(HERE/'StartingTown_Market_ArchitectureV4.blend'),compress=True)
    print(json.dumps({'stage':'exported','glb':str(path),'triangles':visual,'features':len(MEASURE)}),flush=True)

if __name__=='__main__':main()
