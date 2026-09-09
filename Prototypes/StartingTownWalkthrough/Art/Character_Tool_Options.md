# 二次元角色生成与绑定工具 · 2026-09-09

最新接续：用户随后明确要求优先找现成 SAO 角色与骨骼绑定，已下载并检查一版桐人，见[现成 SAO 角色核对](Existing_SAO_Characters.md)。下列生成工具仅保留为备选研究，不代表当前要先走生成路线。

用户询问是否已有能制作二次元角色并完成骨骼绑定的 AI。本次核对官方产品资料与研究仓库；没有调用生成服务、上传项目资产、安装软件或支付费用。以下是候选能力，不是实际生成质量验收，也不构成用户已选择工具。

| 工具 | 已核对能力 | 在 Level0 中的候选用途 |
|---|---|---|
| Meshy | 官方二次元角色页面支持文字/图片生成；Animate 文档支持自动绑定、动作预设以及带骨骼/动画的 FBX、GLB 导出 | 从人物设定生成角色并测试身体动作 |
| Tripo | 文字/图片生成模型，自动生成骨骼与蒙皮权重，动作库和带动画的 FBX 导出，也提供 GLB | 自定义外观生成与自动绑定候选 |
| VRoid Studio | 专用二次元角色编辑器，输出 VRM，导出流程包含骨骼，表情编辑器结果可随 VRM 保存 | 二次元脸、头发、服装和表情的对照基准；它不是文字到三维的生成 AI |
| UniRig | 开源研究实现，自动预测骨骼层级和蒙皮权重，研究覆盖二次元角色 | 对已有高质量静态模型进行自动绑定；它不负责人物外观生成 |

官方来源：

- [Meshy 二次元角色生成](https://www.meshy.ai/3d-tools/anime-character-generator)
- [Meshy Animate 技术说明](https://docs.meshy.ai/en/webapp/guides/animate)
- [Tripo 角色生成、绑定和动画](https://www.tripo3d.ai/features/ai-3d-character-animation)
- [VRoid Studio](https://vroid.com/en/studio)、[VRM 导出](https://vroid.pixiv.help/hc/en-us/articles/15760756822297-I-want-to-learn-more-about-the-VRM-export-feature)、[表情编辑](https://vroid.pixiv.help/hc/en-us/articles/4408150140825-How-to-use-the-Expression-Editor)
- [UniRig 官方仓库](https://github.com/VAST-AI-Research/UniRig)

## 制作判断，尚待实测

如果重点是 AI 按设定生成，优先比较 Meshy / Tripo；VRoid 可作为二次元角色结构与表情的基准。生成结果导入 Blender 后检查脸、发型、网格、关节变形与材质，再接入 Godot。VRM 的表情、专用材质和弹簧骨骼需要相应导入流程，不能把 VRM 简单改名为 GLB 就宣称所有特性已接通。

身体骨骼自动绑定不等于表情、口型、手指抓握和头发/裙摆均已满足项目需求。Meshy 文档也将精确的面部表情与定制动画列为需要导出后制作的情况。候选验收应包含正面/侧脸、眨眼张嘴、抬臂弯膝、行走和拿取时的变形与穿模，并使用游戏真实画面。

上一轮市场街/托尔巴纳的人物仅复用本地 V2 美术网格并烘焙静态姿态，没有走上述服务流程。其外观不能用来评价这些 AI 工具的输出质量。角色视觉替换仍须关联既有永久身份，不能因重做模型而重建人物经历或预算账本。
