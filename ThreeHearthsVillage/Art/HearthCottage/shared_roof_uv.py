"""Material UVs: repeated roof pieces reuse two explicitly unwrapped prototypes.

UV_Material is texture UV0, with intentional overlap. It is not a lightmap atlas.
Geometry and materials are never changed. Short/cut pieces sample a subregion of
the same prototype charts with consistent orientation and physical scale.
"""
import bpy
import json
import math
from collections import defaultdict
from mathutils import Vector, Matrix

UV_NAME = 'UV_Material'
ROOF_NAMES = {'Roof | main curved tiles':'tile', 'Roof | porch curved tiles':'tile',
              'Roof | main ridge caps':'ridge', 'Roof | porch ridge caps':'ridge'}

def connected_parts(obj):
    mesh=obj.data
    parent=list(range(len(mesh.vertices)))
    def find(i):
        while parent[i]!=i:
            parent[i]=parent[parent[i]]
            i=parent[i]
        return i
    for edge in mesh.edges:
        a,b=map(find,edge.vertices)
        parent[b]=a
    groups=defaultdict(list)
    for v in mesh.vertices:
        groups[find(v.index)].append(v.index)
    faces=defaultdict(list)
    for poly in mesh.polygons:
        faces[find(poly.vertices[0])].append(poly)
    result=[]
    for key,ids in groups.items():
        ids.sort()
        local={vi:i for i,vi in enumerate(ids)}
        result.append({'obj':obj,'ids':ids,'local':local,'faces':faces[key],
                       'points':[obj.matrix_world@mesh.vertices[i].co for i in ids]})
    return result

def q(value):
    # 0.1 mm quantization makes translated/rotated copies use bit-identical UVs.
    return round(float(value),4)

def metrics(part,kind):
    p=part['points']
    if kind=='tile':
        assert len(p)==20 and len(part['faces'])==18
        arc=[0.0]
        for k in range(4):
            arc.append(q(arc[-1]+(p[k+1]-p[k]).length))
        return {'arc':arc,'length':q((p[5]-p[0]).length),'thickness':q((p[10]-p[0]).length)}
    assert len(p)==28 and len(part['faces'])==26
    inner=[0.0]
    outer=[0.0]
    for k in range(6):
        inner.append(q(inner[-1]+(p[k+1]-p[k]).length))
        outer.append(q(outer[-1]+(p[15+k]-p[14+k]).length))
    center=(p[0]+p[6])*.5
    axis=(p[0]-p[6]).normalized()
    up=(p[3]-center).normalized()
    radius=q((p[14]-center).length)
    return {'inner':inner,'outer':outer,'length':q((p[7]-p[0]).length),
            'thickness':q((p[14]-p[0]).length),'center':center,'axis':axis,'up':up,'radius':radius}

def chart_for(kind, ids):
    if kind=='tile':
        if min(ids)>=10: return 'top'
        if max(ids)<10: return 'bottom'
        if all(i%10<5 for i in ids): return 'head'
        if all(i%10>=5 for i in ids): return 'foot'
        if all(i%5==0 for i in ids): return 'left'
        if all(i%5==4 for i in ids): return 'right'
    else:
        if min(ids)>=14: return 'outer'
        if max(ids)<14: return 'inner'
        if all((i//7)%2==0 for i in ids): return 'head'
        if all((i//7)%2==1 for i in ids): return 'foot'
        if all(i%7==0 for i in ids): return 'left'
        if all(i%7==6 for i in ids): return 'right'
    raise AssertionError((kind,ids))

def metric_uv(part,kind,chart,i,m):
    if kind=='tile':
        k=i%5
        end=(i%10)//5
        layer=i//10
        if chart in ('top','bottom'):
            return Vector((m['arc'][k],end*m['length']))
        if chart in ('head','foot'):
            return Vector((m['arc'][k],layer*m['thickness']))
        return Vector((end*m['length'],layer*m['thickness']))
    k=i%7
    end=(i//7)%2
    layer=i//14
    if chart in ('outer','inner'):
        return Vector((m[chart][k],end*m['length']))
    if chart in ('head','foot'):
        delta=part['points'][i]-m['center']
        return Vector((q(delta.dot(m['axis'])+m['radius']),q(delta.dot(m['up']))))
    return Vector((end*m['length'],layer*m['thickness']))

def new_uv(obj):
    # The old automatic/default coordinates are preserved in the backed-up files,
    # not exported as a misleading spare UV/lightmap channel.
    while obj.data.uv_layers:
        obj.data.uv_layers.remove(obj.data.uv_layers[0])
    layer=obj.data.uv_layers.new(name=UV_NAME)
    obj.data.uv_layers.active=layer
    layer.active_render=True
    return layer

def select_only(objects):
    bpy.ops.object.select_all(action='DESELECT')
    for obj in objects:
        obj.hide_set(False)
        obj.select_set(True)
    bpy.context.view_layer.objects.active=objects[0]

def apply_shared_roof_uv(house):
    if bpy.context.object and bpy.context.object.mode!='OBJECT':
        bpy.ops.object.mode_set(mode='OBJECT')
    bpy.context.view_layer.update()
    objects=[o for o in house.objects if o.type=='MESH']
    other=[o for o in objects if o.name not in ROOF_NAMES]
    families=defaultdict(list)
    for obj in objects:
        new_uv(obj)
        if obj.name in ROOF_NAMES:
            families[ROOF_NAMES[obj.name]].extend(connected_parts(obj))

    # Non-roof geometry gets a separate material layout; roof templates do not
    # compete with 182 repeated tiles for this packing area.
    if other:
        select_only(other)
        bpy.ops.object.mode_set(mode='EDIT')
        bpy.ops.mesh.select_all(action='SELECT')
        bpy.context.scene.tool_settings.use_uv_select_sync=False
        bpy.ops.uv.smart_project(angle_limit=math.radians(66),island_margin=.002,
                                area_weight=.25,correct_aspect=True,scale_to_bounds=False)
        bpy.ops.uv.select_all(action='SELECT')
        bpy.ops.uv.average_islands_scale()
        bpy.ops.uv.pack_islands(rotate=True,scale=True,margin_method='FRACTION',margin=.003,shape_method='AABB')
        bpy.ops.object.mode_set(mode='OBJECT')

    # Only TWO representative meshes enter the roof UV packer.
    templates=[]
    template_data={}
    for kind,parts in families.items():
        exemplar=max(parts,key=lambda p:metrics(p,kind)['length']*(metrics(p,kind).get('arc',[1])[-1]))
        m=metrics(exemplar,kind)
        data=bpy.data.meshes.new('_UV_template_'+kind)
        faces=[[exemplar['local'][i] for i in face.vertices] for face in exemplar['faces']]
        data.from_pydata(exemplar['points'],[],faces)
        data.update()
        obj=bpy.data.objects.new('_UV_template_'+kind,data)
        bpy.context.scene.collection.objects.link(obj)
        uv=new_uv(obj)
        charts={}
        old_coords={}
        for face in data.polygons:
            chart=chart_for(kind,list(face.vertices))
            charts[face.index]=chart
            for li in face.loop_indices:
                vi=data.loops[li].vertex_index
                old=metric_uv(exemplar,kind,chart,vi,m)
                old_coords[li]=old
                # Space the six charts apart before the real packer runs.
                chart_index=list(dict.fromkeys(charts.values())).index(chart)
                uv.data[li].uv=old+Vector((chart_index*2.0,0.0))
        templates.append(obj)
        template_data[kind]={'obj':obj,'charts':charts,'old':old_coords}
    select_only(templates)
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.uv.select_all(action='SELECT')
    bpy.ops.uv.pack_islands(rotate=True,rotate_method='CARDINAL',scale=True,
                          margin_method='FRACTION',margin=.014,shape_method='AABB')
    bpy.ops.object.mode_set(mode='OBJECT')

    # Solve one affine chart transform, then apply it to every repeated piece.
    transforms={}
    for kind,record in template_data.items():
        obj=record['obj']
        uv=obj.data.uv_layers.active.data
        by_chart=defaultdict(list)
        for face in obj.data.polygons:
            by_chart[record['charts'][face.index]].extend(face.loop_indices)
        for chart,loops in by_chart.items():
            fitted=None
            for ia in range(len(loops)-2):
                if fitted: break
                for ib in range(ia+1,len(loops)-1):
                    if fitted: break
                    for ic in range(ib+1,len(loops)):
                        a,b,c=[record['old'][loops[i]] for i in (ia,ib,ic)]
                        source=Matrix(((b.x-a.x,c.x-a.x),(b.y-a.y,c.y-a.y)))
                        if abs(source.determinant())<1e-10: continue
                        aa,bb,cc=[uv[loops[i]].uv.copy() for i in (ia,ib,ic)]
                        dest=Matrix(((bb.x-aa.x,cc.x-aa.x),(bb.y-aa.y,cc.y-aa.y)))
                        linear=dest@source.inverted()
                        fitted=(linear,aa-linear@a)
                        break
            assert fitted,(kind,chart)
            transforms[kind,chart]=fitted

    families_report={}
    for kind,parts in families.items():
        variants=defaultdict(list)
        for part in parts:
            m=metrics(part,kind)
            uv=part['obj'].data.uv_layers.active.data
            signature=[]
            for face in part['faces']:
                ids=[part['local'][i] for i in face.vertices]
                chart=chart_for(kind,ids)
                linear,offset=transforms[kind,chart]
                for li in face.loop_indices:
                    i=part['local'][part['obj'].data.loops[li].vertex_index]
                    value=linear@metric_uv(part,kind,chart,i,m)+offset
                    uv[li].uv=value
                    signature.append((chart,i,round(value.x,6),round(value.y,6)))
            shape_key=(tuple(m['arc']) if kind=='tile' else tuple(m['outer']),m['length'],m['thickness'])
            variants[str(shape_key)].append(tuple(sorted(signature)))
        for key,signatures in variants.items():
            assert all(s==signatures[0] for s in signatures),f'Copies do not share UV: {kind} {key}'
        families_report[kind]={'instances':len(parts),'master_unwraps':1,'charts_per_master':6,
                              'size_variants':len(variants),'identical_size_uvs_match':True,
                              'variant_instance_counts':sorted([len(v) for v in variants.values()],reverse=True)}
    for obj in templates:
        mesh=obj.data
        bpy.data.objects.remove(obj,do_unlink=True)
        bpy.data.meshes.remove(mesh)

    # Complete coverage, finite coordinates, valid UV area and 0..1 bounds.
    minimum=[float('inf'),float('inf')]
    maximum=[float('-inf'),float('-inf')]
    degenerate=0
    triangles=0
    for obj in objects:
        uv=obj.data.uv_layers.active.data
        for loop in uv:
            assert all(math.isfinite(v) for v in loop.uv)
            for i in range(2):
                minimum[i]=min(minimum[i],loop.uv[i])
                maximum[i]=max(maximum[i],loop.uv[i])
        obj.data.calc_loop_triangles()
        triangles+=len(obj.data.loop_triangles)
        for tri in obj.data.loop_triangles:
            a,b,c=[uv[i].uv for i in tri.loops]
            area=abs((b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x))*.5
            degenerate+=area<1e-13
    assert min(minimum)>-1e-5 and max(maximum)<1.00001,(minimum,maximum)
    assert degenerate==0,degenerate
    report={'uv_layer':UV_NAME,'texture_uv_index':0,'roof_uv_policy':'two shared prototype unwraps; size variants use cropped chart regions',
            'intentional_texture_overlap':True,'lightmap_uv':False,'families':families_report,
            'roof_master_unwraps':2,'roof_master_charts':12,'mesh_objects':len(objects),'triangles':triangles,
            'degenerate_uv_triangles':degenerate,'uv_bounds':{'min':minimum,'max':maximum}}
    house['material_uv_policy']='UV0: repeated tiles/ridges share their prototype texture coordinates. Not a lightmap atlas.'
    return report
