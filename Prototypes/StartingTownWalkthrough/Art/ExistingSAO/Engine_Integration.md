# 桐人接入 Godot · 2026-09-09

用户明确要求先把下载的角色放进现有引擎，代替原来自行制作的占位人物，然后一起调整渲染风格。本次已执行外观替换，未改动永久身份、经历、世界存档或 Kimi 账本。

## 实现

- 原始来源为すがきれもん的 [BowlRoll 配布](https://bowlroll.net/file/96677)。原包与日文文件保留。
- [import_kirito.py](import_kirito.py) 使用上一轮下载的独立 PMX 读取模块，经 Blender 5.2.1 构造原网格、原 UV/法线、原骨骼层级、蒙皮权重和顶点形变，身高设为 1.72 m。
- 可编辑的适配源文件为 [Kirito_Godot.blend](Kirito_Godot.blend)；[Kirito_MMD_Imported.blend](Kirito_MMD_Imported.blend) 是网格/骨骼/形变转换阶段快照，不是完整 MMD 效果或物理复刻。
- 游戏模型为 `assets/characters/kirito/Kirito.glb`，包含 200 骨骼、32 个顶点形变、39 个材质表面，以及合并身体与眨眼通道的 `Idle` 动画。原贴图嵌入 GLB；通过基础材质显示，不把 MMD 的球面贴图、Toon 贴图或专用 Shader 声称为等价转入。
- [kirito_actor.gd](../../kirito_actor.gd) 保留独立骨架实例和共享网格，播放实时站姿与眨眼，提供右肘弯曲、闭眼和口型检查。修正透明设置：实体材质进入不透明渲染，头发使用 Alpha Scissor，原隐藏材质保持隐藏，避免贴图中存在 Alpha 通道就把整个人物都当透明物件。
- [reference_scenes.gd](../../reference_scenes.gd) 将市场街 15 处、托尔巴纳 5 处原人物网格隐藏，使用相同站位/朝向放置桐人外观。建筑、地面、碰撞和两地地理锚点没有改动。仍是美术样板，没有接入 NPC 决策，也没有把所有居民身份改成桐人。

## 查看与复验

在 `C:\vibeGamingDemo1\Prototypes\StartingTownWalkthrough` 执行 `./Run.ps1`：

- `4` 市场街，`5` 托尔巴纳。
- `C` 循环人物正面、侧面、背面近景，`R` 回参考视角。
- `X` 切换右肘弯曲、闭眼与张嘴检查；再次按下恢复站姿与自动眨眼。
- 点击或 WASD 回正常移动，Esc 释放鼠标。
- `./Run.ps1 -Mode character-capture` 生成两处各 5 个视角与连续移动的 3 帧，共 16 张真实引擎图。

[真实引擎验证](../../validation/kirito_in_engine/validation.json)检查两地替换数量、旧人物隐藏、200 骨骼、Skin、32 个顶点形变、网格尺寸、实时头部动画变化、右肘旋转、闭眼形变、原站位高度以及截图保存。`passed` 是技术检查，`visual_approval=false` 继续保留。未重新跑不受影响的全城导航烘焙。

本轮已经实际查看市场街正面、侧面、表情/抬手图及托尔巴纳正面；最后检查背面机位，避免喷泉遮住人物。截图表现为有纹理的 SAO 人物进入现有三维环境；脸颊和下巴的阴影偏灰、材质高光与环境关系尚未达到最终动画画风。后续以此真实引擎基线共同修改人物着色、灯光、后处理和雾；本轮没有批准或定稿全局美术。

## 明确未转入的部分

原 PMX 的 4 个材质形变、142 刚体 / 374 关节及 MMD IK 求解未接入游戏。骨骼和顶点形变可实时驱动，但尚无行走动画、寻路人物、抓取、衣摆/头发物理、居民对话与交易。当前站姿/眨眼是用于接入验证的新动画，不是原作者提供的一整套动作。

## 失败与修正记录

1. 已查本机没有完整 MMD Tools。缓存官方 4.5.14 源后，加载发现缺 `opencc`；包含下载依赖与启动 Blender 的组合命令被自动审批策略拦截。未重试该安装，改用已具备的独立 PMX 读取模块，完整插件没有安装到用户 Blender 配置。
2. 首次 GDScript 编译的三个动态整数类型推断失败，已改为显式 `int`，最终引擎日志无脚本错误。
3. 首次转换中创建 Shape Key 默认混入先前形变，造成巨大形变边界与衣服遮满镜头。源 PMX 尺寸正确，问题在本次转换脚本。已修正为 `from_mix=False`、显式零权重及从原顶点独立生成偏移，并增加导出坐标界限与引擎网格尺寸检查。失败证据保留在 [failed_morph_conversion.json](../../validation/kirito_in_engine/failed_morph_conversion.json)，不能用该轮早期通过的骨骼数量检查冒充画面通过。
4. glTF 导出报告多个材质复用相同图片采样器的警告；这些材质使用相同采样设置。已核对真实引擎贴图与透明头发，最终导出成功，无缺图报告。

原包、修改后的 `.blend` 和 GLB 已设置本地忽略；未提交、上传或重新分发作者模型。费用 0，Kimi 请求 0。自建 Blender/Godot PID 保存在 `owned_processes.jsonl`，任务结束核对见 `../../validation/kirito_in_engine/process_cleanup.json`。
