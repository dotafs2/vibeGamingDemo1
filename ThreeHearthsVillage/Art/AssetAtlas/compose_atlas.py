"""Lay out existing asset renders as a labeled catalog PNG, without redrawing assets."""
import json
import math
import pathlib
from collections import Counter

from PIL import Image, ImageDraw, ImageFont

HERE = pathlib.Path(__file__).resolve().parent
PROJECT = HERE.parents[1]
REPO = PROJECT.parent
inventory = json.loads((PROJECT / 'Docs/Art_Asset_Inventory.json').read_text(encoding='utf-8'))
records = inventory['records']
MODELS = [r for r in records if r['kind'] in ('module', 'assembly', 'whole_model_version')]
LEGACY = [r for r in records if r['kind'] == 'legacy_project_art']
W, MARGIN, GAP = 5120, 80, 16
COLS, CARD_H = 8, 500
CARD_W = (W - MARGIN * 2 - GAP * (COLS - 1)) // COLS
BG, CARD, INK, MUTED = '#eef0eb', '#ffffff', '#20382f', '#63756c'
FONT = 'C:/Windows/Fonts/msyh.ttc'
BOLD = 'C:/Windows/Fonts/msyhbd.ttc'
MONO = 'C:/Windows/Fonts/consola.ttf'


def font(size, bold=False, mono=False):
    return ImageFont.truetype(MONO if mono else BOLD if bold else FONT, size)


groups = [
    ('村庄建筑', 'VillageKit', 'module', '地基 / 柱梁 / 墙体 / 门窗 / 屋面 / 施工搬运件'),
    ('城堡、街市与工坊', 'SocietyKit', 'module', '塔楼 / 城墙 / 门楼 / 市集 / 职业工位 / 身份与商品'),
    ('居民外观配件', 'ResidentKit', 'module', '16 件配件；人物身体、骨骼和基础动画沿用 Cropout'),
    ('家具与日常生活', 'HomeLifeKit', 'module', '床 / 座椅 / 餐桌 / 食物 / 容器 / 庭院用品'),
    ('可搬运商品', 'GoodsKit', 'module', '黏土 / 砖瓦 / 石灰 / 颜料 / 金属件'),
    ('劳动工具', 'ToolKit', 'module', '伐木 / 农耕 / 采矿 / 木工 / 砌筑'),
    ('公共城墙构件', 'PublicWallKit', 'module', '4 个安装条目；其中石墙段复用城堡石墙几何'),
    ('木材加工半成品', 'WoodProductionKit', 'module', '原木加工中的可见阶段；源资产候选'),
    ('城镇拼接与转角', 'TownKit', 'module', '直角 / 通道 / 屋顶节点 / 楼梯；源资产候选'),
    ('组合场景示例', None, 'assembly', '7 个可编辑装配示例；同一组件家族的组合用途'),
    ('小屋整模迭代', None, 'whole_model_version', '原始 / 共享 UV / 高光改进，分别保留的三个版本'),
]
sections = []
y = 315
for title, group, kind, subtitle in groups:
    items = [r for r in MODELS if r['kind'] == kind and (group is None or r['group'] == group)]
    rows = math.ceil(len(items) / COLS)
    sections.append((title, subtitle, items, y))
    y += 105 + rows * (CARD_H + GAP) + 34
legacy_top = y
LCOLS, LH = 10, 330
LW = (W - MARGIN * 2 - GAP * (LCOLS - 1)) // LCOLS
H = legacy_top + 200 + math.ceil(len(LEGACY) / LCOLS) * (LH + GAP) + 155
canvas = Image.new('RGB', (W, H), BG)
draw = ImageDraw.Draw(canvas)


def text(value, xy, size, fill=INK, bold=False, mono=False):
    draw.text(xy, value, font=font(size, bold, mono), fill=fill)


def centered(value, x, y, width, size, fill=INK, bold=False, mono=False):
    f = font(size, bold, mono)
    while draw.textlength(value, font=f) > width and size > 12:
        size -= 1
        f = font(size, bold, mono)
    length = draw.textlength(value, font=f)
    draw.text((x + (width - length) / 2, y), value, font=f, fill=fill)


def place_image(path, rect):
    x, y, width, height = rect
    with Image.open(path) as src:
        src = src.convert('RGBA')
        src.thumbnail((width, height), Image.Resampling.LANCZOS)
        dest = (int(x + (width - src.width) / 2), int(y + (height - src.height) / 2))
        canvas.paste(src, dest, src)


draw.rectangle((0, 0, W, 265), fill='#20382f')
text('炉火与王国  /  美术资产全览', (MARGIN, 45), 76, '#ffffff', True)
text('128 个独立模块  ·  7 个组合示例  ·  3 个整模版本  =  138 份真实模型导出', (MARGIN, 150), 37, '#dbe8d8')
text('直接由项目 GLB 渲染 · 中文逐项标注 · 各格独立缩放展示，非同一比例 · 2026.09.07', (MARGIN, 215), 25, '#afc4b6')
covered = []
for idx, (title, subtitle, items, top) in enumerate(sections, 1):
    draw.rounded_rectangle((MARGIN, top, MARGIN + 60, top + 60), radius=12, fill='#bf7048')
    centered(f'{idx:02}', MARGIN, top + 10, 60, 30, '#ffffff', True)
    text(f'{title}  ·  {len(items)} 件', (MARGIN + 85, top - 3), 43, bold=True)
    text(subtitle, (MARGIN + 85, top + 53), 25, MUTED)
    for n, item in enumerate(items):
        row, col = divmod(n, COLS)
        x = MARGIN + col * (CARD_W + GAP)
        cy = top + 105 + row * (CARD_H + GAP)
        draw.rounded_rectangle((x, cy, x + CARD_W, cy + CARD_H), radius=16, fill=CARD, outline='#dce3db', width=2)
        source = HERE / 'thumbnails' / f"{item['group']}__{item['id']}.png"
        if not source.exists():
            raise FileNotFoundError(source)
        place_image(source, (x + 25, cy + 8, CARD_W - 50, 395))
        centered(item['name_zh'], x + 14, cy + 411, CARD_W - 28, 28, bold=True)
        centered(item['id'], x + 14, cy + 458, CARD_W - 28, 19, MUTED, mono=True)
        covered.append(item['path'])

text('附录  /  早期矿场二维项目', (MARGIN, legacy_top), 52, bold=True)
text(f'{len(LEGACY)} 份场景、透明拆件、年代版本与设计/参考图；部分参考由用户提供，来源详见资产清单。', (MARGIN, legacy_top + 80), 28, MUTED)
text('地上矿场、闸门、升降井、实验室、Boss 大厅，以及设施的分层图片与历史版本。', (MARGIN, legacy_top + 125), 25, MUTED)
for n, item in enumerate(LEGACY):
    row, col = divmod(n, LCOLS)
    x = MARGIN + col * (LW + GAP)
    cy = legacy_top + 200 + row * (LH + GAP)
    draw.rounded_rectangle((x, cy, x + LW, cy + LH), radius=12, fill='#ffffff', outline='#dce3db', width=2)
    path = REPO / item['path']
    if path.suffix.lower() == '.svg':
        # The lone SVG is a technical world-layout diagram, rasterized by a separate renderer.
        path = HERE / 'legacy-layout.png'
    place_image(path, (x + 12, cy + 10, LW - 24, 240))
    centered(pathlib.PurePosixPath(item['path']).parent.name, x + 10, cy + 257, LW - 20, 20, bold=True)
    centered(pathlib.PurePosixPath(item['path']).stem, x + 10, cy + 292, LW - 20, 17, MUTED, mono=True)
    covered.append(item['path'])

text('素材存在 ≠ 相应玩法全部完成。完整文件、来源、使用状态与 SHA-256 见 Art_Asset_Inventory.md / .csv / .json。', (MARGIN, H - 91), 27, MUTED)
text('原始模型和已有图片均保持不变；此图仅作资产目录浏览。', (MARGIN, H - 48), 23, MUTED)
target = HERE / 'ThreeHearths_All_Assets.png'
canvas.save(target, compress_level=6)
preview = canvas.copy()
preview.thumbnail((1600, 6000), Image.Resampling.LANCZOS)
preview.save(HERE / 'atlas-preview.png')
assert len(covered) == len(set(covered)) == len(MODELS) + len(LEGACY)
(HERE / 'atlas-manifest.json').write_text(json.dumps({'output': target.name, 'width': W, 'height': H, 'model_entries': len(MODELS), 'legacy_entries': len(LEGACY), 'model_counts': dict(Counter(r['kind'] for r in MODELS)), 'covered_sources': covered}, ensure_ascii=False, indent=2), encoding='utf-8')
print(json.dumps({'output': str(target), 'size': [W, H], 'bytes': target.stat().st_size, 'covered': len(covered)}, ensure_ascii=False))
