"""Compose the complete, labeled PNG model catalog using real GLB renders."""
from pathlib import Path
import collections
import hashlib
import json
import math
from PIL import Image, ImageDraw, ImageFont

PROJECT = Path(__file__).resolve().parents[1]
REPO = PROJECT.parent
WORK = REPO / '.codex-ue58-diagnostics/model-overview-20260907'
THUMBS = WORK/'thumbs'
OUTPUT = PROJECT/'Docs/Art_Models_Overview.png'
INVENTORY = PROJECT/'Docs/Art_Asset_Inventory.json'
data = json.loads(INVENTORY.read_text(encoding='utf-8'))
records = [r for r in data['records'] if r['kind'] in ('module', 'assembly', 'whole_model_version')]
assert collections.Counter(r['kind'] for r in records) == {'module':128, 'assembly':7, 'whole_model_version':3}
assert len({r['path'] for r in records}) == 138
by_id = {r['id']:r for r in records}
hero_ids = ['HearthCottage', 'HearthCottage_SharedUV', 'HearthCottage_SharedUV_Polished',
            'cottage_terracotta', 'longhouse_slateblue', 'townhouse_terracotta',
            'guild_market_yard', 'kings_gate_courtyard', 'cabin_living_4x4m', 'common_meal_10_seats']
sections = [('整模与组合示例', 'COMPLETE MODELS & ASSEMBLIES', [by_id[k] for k in hero_ids], 5)]
group_titles = [
    ('VillageKit','住宅结构与屋面','HOUSING / 40 MODULES'),
    ('SocietyKit','城堡、市集与作坊','SOCIETY / 32 MODULES'),
    ('ResidentKit','居民服饰与发型配件','RESIDENT ACCESSORIES / 16 MODULES'),
    ('HomeLifeKit','居家家具与生活用品','HOME LIFE / 12 MODULES'),
    ('GoodsKit','建材与商品','GOODS / 8 MODULES'),
    ('ToolKit','劳动工具','TOOLS / 8 MODULES'),
    ('PublicWallKit','公共城墙组件','PUBLIC WALL / 4 MODULES'),
    ('TownKit','街区与屋顶连接件','TOWN CONNECTIONS / 6 MODULES'),
    ('WoodProductionKit','木材加工半成品','WOOD PRODUCTION / 2 MODULES'),
]
for group, title, subtitle in group_titles:
    items = sorted([r for r in records if r['group']==group and r['kind']=='module'], key=lambda r:r['id'])
    sections.append((title, subtitle, items, 8))
assert sum(len(s[2]) for s in sections) == 138

W, M, GAP = 3360, 80, 20
CONTENT = W-2*M
HEADER = 354
SECTION_HEADER = 92
GAP_SECTION = 30
NORMAL_HEIGHT, HERO_HEIGHT = 338, 462
FOOTER = 176
H = HEADER + FOOTER + sum(SECTION_HEADER + math.ceil(len(items)/cols)*(HERO_HEIGHT if cols==5 else NORMAL_HEIGHT) + GAP_SECTION for _,_,items,cols in sections)
canvas = Image.new('RGB',(W,H),'#FBFAF7')
draw = ImageDraw.Draw(canvas)
REG = r'C:\Windows\Fonts\msyh.ttc'
BOLD = r'C:\Windows\Fonts\msyhbd.ttc'
fonts = {}
def font(size, bold=False):
    key=(size,bold)
    if key not in fonts:
        fonts[key]=ImageFont.truetype(BOLD if bold else REG,size)
    return fonts[key]
def text(x,y,value,size=28,fill='#28352F',bold=False):
    draw.text((int(x),int(y)),value,font=font(size,bold),fill=fill,anchor='lt')
def fit(value, width, size, minimum=16, bold=False):
    while draw.textlength(value,font=font(size,bold)) > width and size>minimum:
        size-=1
    assert draw.textlength(value,font=font(size,bold)) <= width, value
    return size

text(M,54,'THREE HEARTHS  /  MODEL ATLAS',25,'#87705C',True)
text(M,107,'小镇模型全览',78,'#253A32',True)
text(M,219,'138 个导出条目 · 全部取自项目 GLB 实模',32,'#5C645E')
text(M,274,'2026.09.07  /  codex/pie-import-preview · 2366a8b',23,'#84877E')
stats = [(2120,'128','独立模块'),(2530,'7','组合示例'),(2880,'3','整模版本')]
for x, number, label in stats:
    text(x,98,number,92,'#B2694C',True)
    text(x+4,214,label,28,'#5C645E')
draw.line((M,331,W-M,331),fill='#DCDCD2',width=2)

y = HEADER
serial = 0
placements = []
qa = []
for section_index,(title,subtitle,items,cols) in enumerate(sections,1):
    color = '#B2694C' if section_index==1 else '#617A68'
    draw.rounded_rectangle((M,y+13,M+59,y+62),radius=8,fill=color)
    text(M+12,y+21,f'{section_index:02d}',25,'#FFFFFF',True)
    text(M+82,y+17,title,37,'#293E33',True)
    title_width=draw.textlength(title,font=font(37,True))
    text(M+104+title_width,y+29,subtitle,19,'#93968B')
    count_label = f'{len(items):02d} 项'
    count_width = draw.textlength(count_label,font=font(29,True))
    text(W-M-count_width,y+22,count_label,29,'#657365',True)
    start_y = y+SECTION_HEADER
    cell_width = (CONTENT-(cols-1)*GAP)//cols
    cell_height = HERO_HEIGHT if cols==5 else NORMAL_HEIGHT
    image_height = 336 if cols==5 else 236
    for position,item in enumerate(items):
        serial+=1
        row,col=divmod(position,cols)
        x=M+col*(cell_width+GAP)
        top=start_y+row*cell_height
        image_box=(x,top,x+cell_width,top+image_height)
        draw.rounded_rectangle(image_box,radius=12,fill='#F0EEE7')
        thumb_path=THUMBS/(item['group']+'__'+item['id']+'.png')
        thumb=Image.open(thumb_path).convert('RGBA')
        bbox=thumb.getchannel('A').getbbox()
        assert bbox is not None, f'Empty image: {item["id"]}'
        assert bbox[0] > 1 and bbox[1] > 1 and bbox[2] < thumb.width-1 and bbox[3] < thumb.height-1, f'Clipped model: {item["id"]}, {bbox}'
        # Transparent content is fitted with a margin; the source geometry is unchanged.
        content=thumb.crop(bbox)
        inner_w,inner_h=cell_width-44,image_height-28
        content.thumbnail((inner_w,inner_h),Image.Resampling.LANCZOS)
        px=x+(cell_width-content.width)//2
        py=top+(image_height-content.height)//2
        canvas.paste(content,(px,py),content)
        text(x+12,top+11,f'{serial:03d}',16,'#8A8D81')
        kind_label={'module':'','assembly':'组合示例','whole_model_version':'整模版本'}[item['kind']]
        if kind_label:
            lab_w=draw.textlength(kind_label,font=font(17))
            text(x+cell_width-lab_w-13,top+12,kind_label,17,'#928779')
        name=item['name_zh']
        extra=''
        if '（' in name:
            name,extra=name.split('（',1)
            extra=extra.rstrip('）')
        name_size=fit(name,cell_width-14,29 if cols==5 else 27,20,True)
        text(x+3,top+image_height+15,name,name_size,'#34423A',True)
        id_size=fit(item['id'],cell_width-12,19 if cols==5 else 17,13)
        text(x+3,top+image_height+54,item['id'],id_size,'#858A80')
        if extra:
            text(x+3,top+image_height+80,extra,17,'#A17F66')
        placements.append({'number':serial,'id':item['id'],'name_zh':item['name_zh'],
                           'kind':item['kind'],'group':item['group'],'source':item['path'],
                           'source_sha256':item['sha256'],'image_box':list(image_box)})
        qa.append({'id':item['id'],'alpha_bbox':list(bbox),'nonempty':True,'not_clipped':True})
    y=start_y+math.ceil(len(items)/cols)*cell_height+GAP_SECTION

draw.line((M,y+10,W-M,y+10),fill='#DCDCD2',width=2)
text(M,y+36,'按导出条目计数，含复用模块、组合与版本；各缩略图独立缩放，不表示统一尺寸。',25,'#6D766C')
text(M,y+82,'原有 Cropout 角色身体、植被与动画不计入本表。居民栏展示本次制作的配件。',25,'#6D766C')
text(W-M-534,y+123,'SOURCE: Art_Asset_Inventory.json',18,'#979B90')
assert serial == 138 and len({p['source'] for p in placements}) == 138
assert y+FOOTER <= H
canvas.save(OUTPUT,optimize=True,dpi=(200,200))
report={'png':OUTPUT.name,'dimensions_px':[W,H],'entries':len(placements),
        'counts':{'module':128,'assembly':7,'whole_model_version':3},
        'inventory_sha256':hashlib.sha256(INVENTORY.read_bytes()).hexdigest(),
        'source_ref':'2366a8bb3f49c0543b64370260e87003024bf12a',
        'preview_note':'GLB geometry, material base colors and standardized Blender Workbench studio; individually fitted scale, not UE lighting or a PBR comparison.',
        'models':placements}
(PROJECT/'Docs/Art_Models_Overview.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
(WORK/'image-qa.json').write_text(json.dumps(qa,ensure_ascii=False,indent=2),encoding='utf-8')
preview=canvas.copy();preview.thumbnail((1680,5000));preview.save(WORK/'overview-review.png')
# Full-resolution crops for visual QA of labels and model content.
canvas.crop((0,0,W,1470)).resize((1680,735)).save(WORK/'overview-top-review.png')
canvas.crop((0,3550,W,4950)).resize((1680,700)).save(WORK/'overview-middle-review.png')
canvas.crop((0,H-1550,W,H)).resize((1680,775)).save(WORK/'overview-bottom-review.png')
print(json.dumps({'png':str(OUTPUT),'dimensions':[W,H],'filesize_mib':round(OUTPUT.stat().st_size/1024**2,2),
                  'model_count':serial,'all_thumbnails_nonempty_and_uncropped':True},ensure_ascii=False))
