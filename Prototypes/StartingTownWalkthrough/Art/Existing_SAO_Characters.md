# 现成 SAO 角色与绑定核对 · 2026-09-09

后续进展：用户明确要求先接入现有引擎并替换原占位人物。已完成 Blender 转换和 Godot 两场景外观替换，见 [引擎接入记录](ExistingSAO/Engine_Integration.md)。下文“尚未导入”描述下载核对阶段，不再代表最新状态。

用户最新要求：优先寻找网上已经制作并绑定的《刀剑神域》角色。此次按这个方向执行，未调用角色生成 AI，也未购买模型。候选不是用户已批准的最终人物或美术标准。

## 已下载并核对实际数据：すがきれもん制作的桐人

- [作者配布页 / BowlRoll](https://bowlroll.net/file/96677)，作者すがきれもん，压缩包署名すがき。
- 通过网页公开 Download 按钮实际下载成功，无登录、无密码、无费用。
- 原包：[Kirito.zip](ExistingSAO/Kirito.zip)，30,319,307 字节（页面显示 28.91 MB）。
- SHA256：`c4e8c6e9483a6e45987af6bb7e810fd89bbc9d7b33a75a205e59823b841f3b44`。
- 解压位置：`Art/ExistingSAO/sugaki_kirito/Kirito/`。旧 ZIP 的日文文件名需恢复 CP932；已正确恢复，16 个引用贴图路径均存在。
- [实际数据核对结果](ExistingSAO/pmx_validation.json)。读取 PMX 二进制数据所得，非网页宣传参数。

| 项目 | 普通骨骼版 | 準標準ボーン版 |
|---|---:|---:|
| 文件 | キリト.pmx | キリト（準標準ボーン）.pmx |
| 文件内版本 | 1.11.19 | 1.11.16 |
| 顶点 / 三角面 | 31,083 / 49,600 | 31,083 / 49,600 |
| 骨骼 | 200 | 227 |
| 带骨骼影响的顶点 | 31,083 | 31,083 |
| 形变控制 | 36 | 36 |
| 材质 / 贴图引用 | 39 / 16 | 39 / 16 |
| 刚体 / 关节 | 142 / 374 | 142 / 374 |

两版包含躯干、四肢、手指、眼睛、头发、衣摆与衣领骨骼，以及脚部 IK。36 个形变控制包含 32 个顶点形变、4 个材质形变，涉及五种元音口型、眨眼、单眼眨眼、惊讶、眉毛、脸红、汗和外套显示等；其中也有脚趾控制，不能把 36 个全部叫作面部表情。

原包附带四张作者表情示例，已实际查看 `表情例/1.png`。它是作者随包提供的效果图，**不是本项目 Blender 或 Godot 渲染截图**。

边界：当前完成下载与数据结构核对。尚未在 Blender 导入、试姿势或在 Godot 验证；不能宣称走路、拿取、口型驱动以及 MMD 物理已经接通。PMX 数据通过 MMD Tools 官方仓库的独立 PMX 读取模块检查；模块仅用于数据读取，未安装 Blender 插件，来源、哈希和许可证保存在 `ExistingSAO/inspection_dependency/`。

作者页面说明造型参考小说第 10 卷封面及当时官方插画，不能据此认定为第一层开服时的初始装备。当前仅作为 SAO 角色美术与动画接入候选；没有改变既有人物身份或把原作人物生平写入居民存档。

随包 `readme.txt`：允许修改，禁止原数据再配布，修改数据再配布需告知作者。本次只保存在本机隔离目录，该目录下的原包、解压文件和检查依赖已加入局部 `.gitignore`；没有提交或上传。随包条款没有明确提供商业游戏发行授权。

## 已确认的其他具体候选

| 角色 / 作者 | 来源与页面信息 | 当前核验状态 |
|---|---|---|
| 亚丝娜高模 / hafidzco | [BlendSwap 18380](https://blendswap.com/blend/18380)，免费、CC-BY、Blender 2.7x、7.57 MB；作者说明附身体绑定 | 尚未取得文件。下载流程需要登录；评论报告缺贴图及打开后变形，需实测，不能当成无问题的成品 |
| 亚丝娜低模 / RajSinghStudio | [CGTrader](https://www.cgtrader.com/free-3d-models/character/woman/asuna-yuuki-idle-low-poly-rigged)，免费、基本绑定，4,051 多边形，含 Maya 与 FBX 等格式 | 网页声明已绑定；本机未下载、未验证。浏览器此次只载入页面外壳，未取得下载入口。低模不自动代表低质量，也不因免费而默认入选 |
| 桐人 / ladyuna1992 | [TurboSquid 1855723](https://www.turbosquid.com/3d-models/kirito-3d-model-tpose-shape-keys-rigged-3d-model-1855723)，核对时页面标价 $3.50，Blender 2.79 / FBX 等，声明 102 根骨骼、手和头发绑定、面部 Shape Keys | 未购买，未验证。平台标注 Editorial Uses Only，作者描述限非营利用途；仅作有价格与条款的备选，不当作商业发行资产 |
| 桐人低模 / chelyzmarx | [BlendSwap 14849](https://blendswap.com/blend/14849)，免费、CC-BY、Blender 2.7x、446 KB | 已到下载页，明确要求登录。作者评论谈到 Warcraft 3 版绑定与动画，未证明这个下载包必定包含相同绑定 |

## 已排除或暂不采用的线索

- [BlendSwap 的 Sinon 教程模型](https://blendswap.com/blend/13555)虽然出现在 rigged 分类搜索结果，作者正文明确说没有时间完成绑定，不能算现成绑定角色。
- [亚丝娜未完成贴图版](https://blendswap.com/blend/14895)作者明确说贴图尚未完成，绑定也未确认。
- [GilsonAnimes 的诗乃](https://sketchfab.com/3d-models/sinon-sao-anime-3d-model-blender-044e0ca5a72342729bafd57b8bbe32f8)页面说明含 Rigify / Metarig、Shape Keys 与 Toon Shader，可作质量对照；未取得文件，不是第一层居民服装。
- MMD 索引里的 EndressStorm 亚丝娜、莉兹贝特、西莉卡已标为非公开，不把失效链接列作能直接下载的成果。
- BOOTH 的 Ichinose Asuna 是《蔚蓝档案》角色，与 SAO 结城明日奈不同，已排除；打印 STL、未确认来源的游戏提取件也未下载。

## 下一步与本次运行事实

优先将已下载桐人作为候选，导入 Blender 检查正侧脸、抬臂弯膝、口型与眨眼，再转换到游戏做真实渲染与动画验收。MMD 的材质、IK 和刚体约束需要转换与实测，不能仅转换扩展名。若之后选中该外观，仍需结合第一层时期服装处理，保留已有世界与居民永久身份。

本次没有改动两个场景、世界存档或预算账本。Kimi 请求 0，购买 0。自建无头 Edge 主进程 PID 594376 已关闭，记录在 `ExistingSAO/browser_process.json`。没有操作用户原有浏览器或引擎。
