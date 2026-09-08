"""Package unaltered Blender renders into labelled review sheets (Pillow)."""
import json
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

ROOT=Path(__file__).resolve().parent
P=ROOT/'Previews'
CAT=json.loads((ROOT/'catalog.json').read_text(encoding='utf-8'))
FONT='C:/Windows/Fonts/msyh.ttc'
BG='#F1EFE5';INK='#263E35';MUTED='#66736B'
def font(size):return ImageFont.truetype(FONT,size)
def text(draw,xy,value,size=38,fill=INK):draw.text(xy,value,font=font(size),fill=fill)
def centered(draw,y,value,size,width):
    bounds=draw.textbbox((0,0),value,font=font(size))
    text(draw,((width-(bounds[2]-bounds[0]))/2,y),value,size)
def image_fit(board,path,box):
    image=Image.open(path).convert('RGB');image.thumbnail((box[2]-box[0],box[3]-box[1]),Image.Resampling.LANCZOS)
    board.paste(image,(box[0]+(box[2]-box[0]-image.width)//2,box[1]+(box[3]-box[1]-image.height)//2))

def three_board(filename,title,subtitle,entries):
    im=Image.new('RGB',(3000,1260),BG);d=ImageDraw.Draw(im)
    text(d,(65,35),title,64);text(d,(68,126),subtitle,30,MUTED)
    for i,(mid,name,desc) in enumerate(entries):
        x=i*1000
        image_fit(im,P/(mid+'.png'),(x+20,200,x+980,1080))
        text(d,(x+45,1100),name,42)
        text(d,(x+45,1164),desc,27,MUTED)
    im.save(P/filename)

three_board('Masters_Overview.png','可生长的中世纪住宅 · Blender 样板',
            '3 栋主样板，共用 28 种构件；屋面、侧翼、院落与高度经过单独设计。',[
    ('carpenter_court','木匠院落','低侧翼 + 工作棚 + 木料与工作台'),
    ('merchant_steps','商人住宅','双层主屋 + 阳台 + 低侧屋与摊位'),
    ('family_cluster','家庭围院','多方向屋面 + 两翼小院 + 加高卧室')])
three_board('Family_Growth.png','同一户人家，逐步把房子盖大',
            '实际构件组合示范：保留旧构件，拆除新共墙，再加入侧翼与上层。此图不是游戏中的 NPC 历史记录。',[
    ('family_starter','01 · 最初的小屋','结构占地 16 m² · 四个 2 × 2 m 单元'),
    ('family_side_wing','02 · 增加低侧翼','结构占地 24 m² · 新增空间连通旧屋'),
    ('family_cluster','03 · 围院与加高卧室','结构占地 36 m² · 上层增加 4 m²')])

im=Image.new('RGB',(2800,1410),BG);d=ImageDraw.Draw(im)
text(d,(65,30),'同一构件：结构、表面与附加层',58)
text(d,(68,115),'真实网格分层；可以逐层安装、移除，或仅更换表面配色。',30,MUTED)
image_fit(im,P/'Material_Layers_Raw.png',(0,170,2800,1220))
for i,s in enumerate(('01  梁架与窗框','02  安装墙面和窗扇','03  添加修补、藤蔓、花箱','04  保留梁架，换表面配色')):
    text(d,(i*700+34,1250),s,31)
im.save(P/'Material_Layers.png')

NAMES={
 'wall_plain_2m':'实墙与斜撑','wall_window_2m':'窗墙与木窗扇','wall_door_2m':'门墙与铁箍木门','wall_passage_2m':'开放通道门框',
 'wall_knee_2m':'阁楼矮墙','foundation_cell_2m':'石基础单元','stone_skirt_2m':'石砌外缘','floor_cell_2m':'木地板与托梁',
 'roof_gable_4x2m':'宽屋面分段','gable_end_4m':'宽山墙与通风口','roof_gable_2x2m':'窄屋面分段','gable_end_2m':'窄山墙',
 'overlay_lime_repair':'灰泥修补与苔痕','overlay_ivy':'攀墙藤蔓','attachment_flowerbox':'木制花箱','attachment_steps_040':'低石阶',
 'attachment_steps_080':'高石阶','attachment_chimney':'中空石烟囱','attachment_work_canopy_2m':'木工作棚','attachment_balcony_2m':'阳台分段',
 'attachment_market_awning':'条纹摊棚','prop_workbench':'木匠工作台','prop_log_stack':'堆叠原木','prop_barrel':'箍铁木桶',
 'prop_bench':'庭院长凳','attachment_garden_fence_2m':'庭院栅栏','prop_produce_crate':'果蔬货箱','attachment_shop_sign':'悬挂店招'}
im=Image.new('RGB',(2400,4470),BG);d=ImageDraw.Draw(im)
text(d,(55,28),'28 种可组合构件 · 完整图集',54)
text(d,(59,110),'每一项均有独立 GLB、材质槽和连接锚点。缩略图为 Blender 实际模型渲染。',25,MUTED)
for i,entry in enumerate(CAT['modules']):
    col=i%4;row=i//4;x=col*600;y=190+row*600;mid=entry['id']
    image_fit(im,P/'Modules'/(mid+'.png'),(x+12,y,x+588,y+460))
    text(d,(x+23,y+470),f'{i+1:02d}  {NAMES[mid]}',29)
    text(d,(x+23,y+521),mid,20,MUTED)
im.save(P/'All_28_Modules.png')
print('REVIEW_BOARDS_COMPLETE',flush=True)
