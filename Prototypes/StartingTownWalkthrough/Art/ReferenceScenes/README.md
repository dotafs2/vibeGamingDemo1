> 最新用户裁决：停止 V4 按截图反投影的造型路线。改做正常、精致、细节更多且符合 SAO 设定的完整三维建筑，不要求截图完全一致。见 validation/market_craft_v5/Direction.md；下文 V4 为已否决历史。

# Level0：市场街与托尔巴纳场景模型

当前执行（覆盖下方旧逐轮记录）：两处均已按正常完整三维方式升级，托尔巴纳四栋临广场建筑、旅店内部、树木、喷泉与园林已产生真实引擎证据，见 [两处 V5 最新状态](../../validation/tolbana_craft_v5/README.md)。默认工程与启动脚本使用两处 V5；旧 HQ/V3/V4 保留归档。当前开始画风讨论，最终材质、光照、后处理与雾仍待用户决定。


当前源文件与启动候选为 [MarketCraftV5](MarketCraftV5/README.md)。两栋近景店屋按正常尺度与完整三维结构重建，实际店内外验证见 [V5](../../validation/market_craft_v5/README.md)。下文 V4 当前入口描述已经被用户最新方向覆盖，仅作历史。

当前制作入口为 [MarketArchitectureV4](MarketArchitectureV4/README.md)，当前源文件是 `MarketArchitectureV4/StartingTown_Market_ArchitectureV4.blend`。用户明确要求在原因诊断后修复；V4 修正独立立面、窗洞遮挡、棚布及门桥/圆塔/穹顶，实际图见 [V4 验证](../../validation/market_architecture_v4/README.md)。`./Run.ps1` 与 `-Mode market-demo` 均进入 V4。下文 V3/HQ 为历史，视觉通过结论未恢复。

最新状态：**r10 建筑一致性未通过，此前内部模型/构图通过意见已撤回。** 当前模型仍使用通用建筑结构近似参考，见 [建筑不一致诊断](../../validation/market_demo_v3/Architecture_Mismatch_Diagnosis.md)。以下 HQ/V3 名称、合法几何和工程通过记录不能视为建筑还原通过。

最新市场街候选为 [MarketDemoV3/StartingTown_Market_V3.blend](MarketDemoV3/StartingTown_Market_V3.blend)，由 `assemble_market_demo.py` 与 `craft_landmarks.py` 复建。两名子代理分别建模与复核，主代理负责组装和 Godot；最新实际证据、拒绝过的版本、可运行命令及未批准事项见 [单机位复核](../../validation/market_demo_v3/README.md)。`.\Run.ps1 -Mode market-demo` 使用 r10 候选；下述 HQ 文件和默认清单保留历史，不代表用户已认可其模型或画风。

最新人物接续：按用户要求，Godot 运行时已将以下 HQ 模型内的旧静态人物隐藏，保留站位并换为下载的桐人骨骼模型；建筑 GLB 与本页 Blender 场景源仍保留原数据作为归档。当前人物源文件为 `../ExistingSAO/Kirito_Godot.blend`，接入与验证见 [说明](../ExistingSAO/Engine_Integration.md)。原人物描述和旧截图属于替换前阶段。

## 当前版本：按动画方向高质量重建

用户在拒绝 V1 后给出 HearthCottage 示例，并明确：只有两个小场景，精度可以超过示例，按模仿动画的方向高质量建模。该要求覆盖旧版“最小实现”的低精度解释。原因诊断见 [建模质量原因记录](Model_Quality_Diagnosis_2026-09-09.md)。

当前源文件为 [Level0_Anime_Reference_Scenes_HQ.blend](Level0_Anime_Reference_Scenes_HQ.blend)，生成脚本为 [build_reference_scenes_hq.py](build_reference_scenes_hq.py)。文件内仍有市场街、托尔巴纳和第一层地理总览三个 Blender Scene；保持米制坐标、可编辑网格、材质分区及 UV。旧脚本的低层几何函数继续复用，不再使用其整栋通用房屋与球形树生成方案。

重建包括：分别设计两侧市场立面与托尔巴纳住宅、真实窗洞和深窗台、弧形叠瓦及四坡屋顶脊瓦、门板和五金、弧垂遮棚、木箱和桶板、分层喷泉曲面、五条主分枝及次级树枝、成簇独立叶片、弯曲细草叶。人物只复用既有 V2 美术网格并烘焙站姿/动作快照，不导入旧身份、记忆或世界状态，也不是自主 NPC。

游戏中的地理锚点沿用下表。两处约 7.65 km 的空间分离保留，未将托尔巴纳搬进起始之城。局部布局与未见背面仍属参考推断，不能把本版称为官方精确地图或逐像素完全还原。

运行 `./Run.ps1`；数字 `4` 市场街、`5` 托尔巴纳、`R` 参考机位、`O` 俯瞰，`1` 返回旧中央广场。实际截图和机器验证在 [reference_scenes_hq](../../validation/reference_scenes_hq/validation.json)：两处各五个固定视角，以及连续移动过程的三帧。当前 `passed` 仅表示几何安装、位置、截图、地面与行走检查通过，`visual_approval=false` 保留用户验收状态。

审模使用 Godot 4.7.2 / RTX 4060 / Forward+ Vulkan，1600×900、方向光、天空补光、SSAO 和抗锯齿；普通雾与体积雾均关闭。改变渲染路径用于看清模型接触和凹凸，**不构成最终渲染风格或全局引擎迁移决定**。动画背景的绘画纹理、叶片着色和流动水、人物着色、后处理及体积雾仍需后续调整，不能用本次临时光照顶替用户美术验收。

复建命令：

```powershell
& 'C:\Program Files (x86)\Steam\steamapps\common\Blender\blender.exe' --background --threads 4 --python-exit-code 1 --python 'C:\vibeGamingDemo1\Prototypes\StartingTownWalkthrough\Art\ReferenceScenes\build_reference_scenes_hq.py'
```

被拒绝的 V1 源和清单留在 `Rejected_V1/`，截图留在 `validation/reference_scenes_v1/`。下文是 V1 历史记录，其“初版完成”“GL Compatibility”等信息不描述当前版本。最新验证数值与未完成项见 [本轮重建记录](HQ_Revision_2026-09-09.md)。

## V1 历史记录（模型质量被拒绝）

2026-09-09。用户确认还原的是参考拼图上方的起始之城市场街与下方的托尔巴纳广场，并要求先做最小模型实现、统一风格，再调整渲染、后处理与体积雾。此决定取代先做单店面美术样板的顺序。

## 本次模型

- `Level0_Anime_Reference_Scenes.blend`：本机 Blender 5.2.1 生成的可编辑源文件。场景列表中有 `StartingTown_Market`、`Tolbana_Plaza`，以及将两处放在同一第一层坐标系中的 `Level0_Geographic_Placement`。总览使用链接网格，编辑源网格能同步反映。
- `build_reference_scenes.py`：确定性建模脚本，统一材料库、建筑与构件尺度语言，保留米制 UV 供下一步纹理与 Shader 调整。未导入第三方模型或参考截图作为纹理。
- `scene_manifest.json`：材质色板、区域 ID、位置、轴向、模型名称、碰撞范围与导出 SHA-256。
- 游戏模型位于 `../../assets/reference_scenes/StartingTown_Market.glb` 与 `Tolbana_Plaza.glb`。

市场街包含两侧石砌店面、门窗与百叶、实际门洞、条纹遮棚、摊位、木桶、铺装、挂旗、远端桥式拱门和圆塔及远处穹顶轮廓。托尔巴纳包含喷泉、大树、草地与灌木、步道、摊位和长椅、错层住宅及方塔、谷地地形和南北接近路段。人物为尺度与构图占位，不是永久居民或自主 NPC。

这是一版按可见形体搭建的模型重建。单张镜头不能确定背面、完整平面与精确尺寸，隐藏面和数值仍属项目补全；不声称已逐像素或完整城镇复原。材质颜色和基本形体共用同一套来源与规则，但尚未完成动画渲染、绘画纹理、树叶着色、后处理和体积雾调试，也尚未由用户验收。

## 地理放置

区域锚点复用 `ThreeHearthsVillage/Plugins/ThreeHearths/Source/ThreeHearths/Private/HearthAincradFloorPlan.cpp`。第一层单位米，X 东、Y 北、Z 上；起始之城广场原点在第一层 `(0,-4770)`，托尔巴纳在 `(0,2900)`，迷宫在 `(0,4200)`。这些数值是现有工程锚点，不是官方测绘值。

| 场景 | 第一层东/北坐标（m） | Godot 位置（m） | 朝向 | 依据边界 |
|---|---|---|---|---|
| 起始之城市场街 | `(-163,-4752)` | `(-163,0,-18)` | Y 轴 78° | 位于既有起始之城西侧市场街；具体街道方位为原型补全 |
| 托尔巴纳广场 | `(0,2900)` | `(0,0,-7670)` | 0° | 保留北部、迷宫附近的谷地城镇身份；广场局部朝向为补全 |

坐标换算为 `Godot=(东,高,-(北+4770))`。两处没有拼成起始之城内的相邻广场。Blender 总览使用第一层坐标，两个局部 Blender 场景使用各自原点建模；在 Godot 内均使用上表实位。两城之间约 7.67 km 的完整可行走道路尚未铺设，数字键切换是开发跳转，不是传送玩法已实现。

Godot 安装模型时只隐藏市场街范围内 35 个重叠的原型住宅/屋顶/任务店面对象，同时移除它们的碰撞；原始 GLB 保留。中央广场、拱廊和外围城墙未整体替换。UE 地图、存档、居民身份及预算账本没有修改。

## 运行与复建

在 Godot 原型目录执行 `./Run.ps1`，默认进入市场街。`4` 市场街、`5` 托尔巴纳、`R` 对应参考机位、`O` 局部俯瞰、`1` 旧中央广场。参考机位先固定；点击或按 WASD 开始正常行走，保留鼠标、跳跃和 Shift 快走。

执行 `./Run.ps1 -Mode reference-capture`，保存两处各四个真实引擎视角，并检查地面和实际 CharacterBody 行走。结果在 `../../validation/reference_scenes/validation.json`；图片包括参考、近景、反向和俯瞰。检查通过不等于视觉还原度已由用户认可。

复建使用已安装的 Blender，无需下载：

```powershell
& 'C:\Program Files (x86)\Steam\steamapps\common\Blender\blender.exe' --background --threads 4 --python-exit-code 1 --python 'C:\vibeGamingDemo1\Prototypes\StartingTownWalkthrough\Art\ReferenceScenes\build_reference_scenes.py'
```

后续先对照两处模型的轮廓、构图与尺度反馈，再调整共同的材质 Shader、光照和后处理。体积雾尚未开启；当前仍用 GL Compatibility，不能把普通距离雾称为体积雾，渲染路径与其支持范围需在那一步单独验证。

## 本轮实测与已知差距

已在 Godot 4.7.2 / RTX 4060 / GL Compatibility 中导入。空间导航重新烘焙并保存，53 个路径点到达市场目标、1,939 个导航多边形，等待地图版本推进且区域边界就绪的修复仍保留；约 35 m 的原拱廊行走通过。新两处分别进行约 7.5 m 的实际 CharacterBody 行走，地面与区域分离检查通过。截图与最新模型哈希由 `reference-capture` 写入 JSON，不能用 Blender 离线预览代替。

保留失败记录：首轮接入有一个 GDScript 类型推断错误，已显式声明字符串类型修复；第一次截图显示市场街两侧店面朝向外侧，已翻转为向街道；托尔巴纳屋顶端面遮挡山墙，已去除多余封面；市场桥洞上方补了实体填充，并缩短参考机位至拱门的距离。以上均经过后续真实引擎重新截图检查。

视觉上仍是初版：居民是简化尺度人形，树冠与草叶还没有动画式材质，石材/瓦片的颜色变化偏程序化，灯光沿用原型基础配置，尚未复现原参考的冷暖、通透感及水流表现。当前交付解决两处场景模型与正确区域放置，不能宣称最终风格通过或单帧严格复刻。下一步以用户对这两处的反馈调整模型，再进行共同的渲染调试。

本轮 Kimi 请求 0、费用 0；未执行 Git 操作、提交或上传，未改动 UE 世界、Saved 或预算账本。自建 Blender/Godot PID 为 362372、576760、585332、552032、585916、506404、588720、587124，均已退出；清理核对见 `../../validation/reference_process_cleanup.json`。最终两份 GLB 的实际 SHA-256 与模型清单、实机截图验证所记录的 SHA-256 均一致。
