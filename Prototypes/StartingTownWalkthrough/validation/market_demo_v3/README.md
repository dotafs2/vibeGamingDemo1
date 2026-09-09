# 市场街：单机位模型与构图复核

2026-09-09。用户再次拒绝模型细化与布局，明确要求两个子代理相互验证，先保证至少一个 demo 视角正确。本轮限定起始之城市场街；托尔巴纳、旧 UE 世界和持久人物存档保持原状。全局渲染风格仍由用户裁决。

## 当前可运行交付

最新完整实机版本为 **r10，但建筑一致性已被用户再次否决；此前模型/构图内部通过结论撤回**。重新查看画面及源码后，确认通用建筑结构与少数点位对齐不能满足按动画还原的要求，见 [建筑不一致原因](Architecture_Mismatch_Diagnosis.md)。两位代理原先的通过意见在 [独立复核](Review.md) 与 [模型员实机复核](r10/modeler_review.md) 中仅作为历史保留。用户视觉批准与全局渲染批准均为 false。失败候选仍可在 `C:\vibeGamingDemo1\Prototypes\StartingTownWalkthrough` 执行以下命令复现：

```powershell
.\Run.ps1 -Mode market-demo
```

`4` 市场街、`5` 托尔巴纳、`R` 参考视角、`O` 局部俯瞰，WASD 行走。该入口将新市场街安装在原区域锚点，并保留托尔巴纳。默认 `play` 仍使用原 HQ 清单，避免把本轮候选自动写成已批准的全局资产标准。

重新截图使用 `.\Run.ps1 -Mode market-demo-capture`，输出到 `manual/`；不会覆盖 r10 证据。绘图脚本为 `Art/ReferenceScenes/present_market_demo_v3.cjs`，只把动画与真实引擎帧并排展示，没有重绘引擎画面。

- [动画与实机同画幅对照](r10/comparison.png)
- [真实门桥、圆塔近景](r10/details.png)
- [完整对照与投影表](r10/comparison.html)
- [原始引擎结果](r10/capture.json)
- [独立复核及已纠正的误判](Review.md)
- [参考坐标合同](reference_landmarks.json)

本轮没有把截图、图片平面或概念图用作场景模型。保留独立三维实体、连续镜头视差和真实碰撞检查。

## 本轮模型改动与验证范围

模型员独占 `craft_landmarks.py` 和 Blender 导出，复核员独立检查参考、实际截图及几何实现；主代理组装街景、调整建筑和棚布位置、接入 Godot，并复核两位代理的结论。没有创建额外用户任务或第三名代理。

重建了真实可穿行的门桥、沿开口裁切的错缝砌块、圆柱连续连拱塔廊、鼓座与穹顶；左右楼体按参考的高低层级与前后遮挡重新布置。大棚沿街道纵深延伸，屋面、下垂布边、接缝和支架均为几何。两种布边标记从各自材质分区的实际网格顶点选择。人物继续复用已接入的桐人骨骼资产，调整真实距离、朝向及遮挡，共 36 个外观实例，不创建 36 个居民身份。

地理锚点保持 Godot `(-163,0,-18)`、Y 轴 78°；第一层东/北坐标 `(-163,-4752)`。托尔巴纳仍位于 `(0,0,-7670)`，两镇保持超过 7 km 的空间分离。锚点与精确街道尺寸属于现有项目补全，不能冒充官方完整测绘。

Godot 4.7.2 / Forward+ / RTX 4060 实机：r10 的 12 项工程检查通过；`CharacterBody3D` 穿过门洞约 **7.50 m** 后仍在地面；保存主视角、近景、俯瞰及连续移动镜头三帧。r10 stderr 无输出。独立可玩入口的语法与实际启动另行检查，见 `demo_parse_*` 和 `playable_demo_*` 日志。

七个可见标记点的最大横向误差为 **0.0130 画宽**，最大纵向误差为 **0.0213 画高**。这是人工参考点与实际 Camera3D 投影的坐标差，不是全画面相似度，也不能替代形状、遮挡或用户验收。前景人物头顶来自实时骨骼变形后的最高网格顶点。

当前 GLB SHA-256：`ce7a7f9d0264e35691b62e046e54909594ed7280ee313342ffea4f4b97f5d87e`，105,582,544 bytes。清单同时记录两份建模源脚本的 SHA-256，防止修改源码后仍把旧导出视为最新成果。网格规模与检查数量均不作为美术质量通过依据。

## 没有通过的早期版本与经验

初始 V3 生成器曾仅按对象名称存在就输出 `all_pass=true`，已明确作废为历史报告。后续 r3–r7 的实机复核持续拒绝过错误棚布方向、楼体遮挡、门拱重复面与塔廊形状；r8/r9 是未采集实机的中间导出，不能描述为通过。

圆塔的独立平面拱、柱头高度和拱肩实现曾不能在目标视角读成宽拱。最后改为围绕圆柱角度的连续内外墙带，拱顶恒低于檐口 0.22 m，柱止于起拱线。棚布则由对着相机的横幅改为沿街纵深的布面，分别校准上缘与下缘。

复核也发生过错误：早期布边点落在人群上；对屋顶尖度与右前人物朝向的笼统判断不符合参考；依据 `white_hem` 变量名推断绿色布点无效也不成立。这些都已在 Review.md 撤回或更正。互审结论必须经实际图像和数据核对，不能自动等同正确。

## 保留的边界与下一步

最终用户视觉批准、完整动画复原、全局画风批准均为 **false**。现有基础材质、石墙色调、脸部阴影、光照、后处理与雾没有被本轮自动认可；人物服装与姿态也不是动画原镜头逐人复原。没有接入 NPC 自主行为、交易、记忆、模型切换或 Kimi。

独立形状复核的最新裁决以 Review.md 末尾及 r10/review_result.json 为准。仍可见右墙略重、棚布偏规则、人物重复，以及窗序并非逐像素复原。用户判断本轮模型后，再按反馈调整全局渲染；托尔巴纳仍须按同样的真实对照流程处理。本轮没有将已失败的旧 HQ 记录改写成成功，也没有重复旧世界调研或恢复付费任务。

Kimi 请求 0、费用 0；累计预算与旧预留未变。未执行 Git、提交、上传或旧世界存档修改。进程证据见 `root_owned_processes.jsonl`、`cleanup.json` 与 `Art/ReferenceScenes/MarketDemoV3/owned_processes.jsonl`。

## 当前实现文件

- `Art/ReferenceScenes/MarketDemoV3/assemble_market_demo.py`：场景组装、测量点、导出清单。
- `Art/ReferenceScenes/MarketDemoV3/craft_landmarks.py`：门桥、圆塔和穹顶。
- `Art/ReferenceScenes/MarketDemoV3/StartingTown_Market_V3.blend`：可编辑 Blender 源。
- `assets/reference_scenes/StartingTown_Market_V3.glb`、`market_v3_manifest.json`：引擎候选。
- `Art/ReferenceScenes/capture_market_demo_v3.gd`：真实引擎投影、移动镜头和穿门检查。
- `Art/ReferenceScenes/run_market_demo_v3.gd`：保留原区域的可玩入口。
- `Art/ReferenceScenes/present_market_demo_v3.cjs`：原参考与实际渲染并排展示。
- `main.gd`、`reference_scenes.gd`：增加可选清单路径，保留默认行为。
- `Run.ps1`：增加 `market-demo` 与 `market-demo-capture`。
