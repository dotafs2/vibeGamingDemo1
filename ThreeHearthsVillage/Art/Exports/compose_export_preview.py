"""Compose a labeled preview from the actual FBX round-trip renders."""
from pathlib import Path
import json
from PIL import Image, ImageDraw, ImageFont

OUT=Path(__file__).resolve().parent
manifest=json.loads((OUT/'ThreeHearths_All_Git_Assets_ccd6757.json').read_text(encoding='utf-8'))
validation=json.loads((OUT/'FBX_Validation.json').read_text(encoding='utf-8'))
assert validation['passed']
entries=[e for e in manifest['entries'] if e['id'].startswith('NEW_')]
font=ImageFont.truetype('C:/Windows/Fonts/msyh.ttc',30)
small=ImageFont.truetype('C:/Windows/Fonts/msyh.ttc',21)
title=ImageFont.truetype('C:/Windows/Fonts/msyhbd.ttc',54)
canvas=Image.new('RGB',(2880,2050),'#eee9dc')
draw=ImageDraw.Draw(canvas)
draw.text((60,38),'新增建筑与植物 · FBX 实际导入预览',font=title,fill='#294337')
draw.text((60,113),'Git ccd6757   ·   12 组程序化组合   ·   每项单独适配视角，图片比例不同',font=font,fill='#646a5f')
for i,e in enumerate(entries):
    x=60+(i%4)*700;y=190+(i//4)*600
    draw.rounded_rectangle((x,y,x+680,y+574),radius=12,fill='#faf8f1')
    im=Image.open(OUT/'Preview_New_Assets'/(e['id']+'.png')).convert('RGBA')
    im.thumbnail((660,478),Image.Resampling.LANCZOS)
    canvas.paste(im,(x+(680-im.width)//2,y+5+(478-im.height)//2),im)
    draw.text((x+24,y+492),e['name_zh'],font=font,fill='#294337')
    draw.text((x+24,y+536),e['id'].removeprefix('NEW_'),font=small,fill='#697267')
draw.text((60,2010),'城堡为完整设计方案；不代表存档中已施工完成。',font=small,fill='#697267')
canvas.save(OUT/'New_Assets_FBX_Preview.png',optimize=True)
size=manifest['fbx_bytes']/1024/1024
text=f'''# Git 全部模型 FBX 导出

- 版本：`{manifest['git_commit']}`，分支 `codex/town-growth-20260907`。
- 主文件：[{manifest['fbx']}]({manifest['fbx']})，约 {size:.1f} MiB。
- {manifest['entry_count']} 个模型条目/源场景版本；不是 {manifest['entry_count']} 个互不重复的原创模型。
- 138 份 GLB、19 份 Blender 源场景版本、198 个 Git 中的 UE 网格（186 静态网格、12 骨骼网格），以及 12 组当前代码生成的建筑/植物组合。
- 同一模型的 GLB、UE、Blend 表示分别保留，按来源分组；所有对象可独立选择。
- 实际米制尺寸，分类分行陈列，没有统一缩放。UE 的大型水面等网格也包含在内，全景尺度较大；浏览单项时选择其命名根节点并框选聚焦。

## 新增组合

排屋、店屋、庭院工坊、仓库、旅店、王家花园城堡、橡树、白桦、果树、柏树、开花灌木、野花。

![FBX 实际导入预览](New_Assets_FBX_Preview.png)

## 保留与限制

保留模型几何、UV、可转换的常规材质/贴图、顶点颜色与骨骼。Unreal 的程序化材质图、游戏逻辑、存档、音频、动画片段、说明图等不属于此模型合辑；它们仍保留在 Git 项目中。原始 Blend 场景的摄影地板、相机、灯和隐藏辅助物未放入 FBX；源文件保持原样。

647 个生成材质从同名 GLB 源材质恢复颜色、粗糙度和金属度。11 张角色/工具源贴图已嵌入 FBX。UE 水面、地形等程序化材质需在目标软件重建；运行时水面环境采样器没有对应的静态贴图。

城堡包含完整设计方案的 1,319 个网格部件（1,276 个源模块，植物展开为多个网格），不表示游戏存档已经完成施工。

## 验证

重新导入最终 FBX 后，逐条核对根节点、网格数、三角面数、UV、骨骼数和包围盒，全部通过。

- 网格对象：{validation['actual']['mesh_objects']:,}
- 三角面：{validation['actual']['triangles']:,}
- 骨架：{validation['actual']['armatures']}，骨骼总数：{validation['actual']['bones']}
- 文件 SHA-256：`{manifest['fbx_sha256']}`
- [完整模型索引]({manifest['fbx'].replace('.fbx','.json')})
- [重新导入验证报告](FBX_Validation.json)
- [{manifest['blend']}]({manifest['blend']}) 是同场景的 Blender 可编辑副本。

导出期间未调用付费模型 API，未改写游戏存档，未开启定时任务。
'''
(OUT/'README_导出说明.md').write_text(text,encoding='utf-8')
print('EXPORT_PREVIEW_COMPLETE',size,flush=True)
