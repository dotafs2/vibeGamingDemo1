> 已被用户否决并归档。当前正常三维建模版本为 ../MarketCraftV5/README.md，Run.ps1 默认入口为 V5。

# 市场街建筑修复 V4

用户在否决 V3 r10 建筑一致性后明确要求修复。本目录是新的可编辑市场街候选，保持原第一层地域锚点与相机。它不是用户已经通过的模型/全局渲染标准。

当前主实现：

- `assemble_architecture_v4.py`：每段街墙各自的原图轮廓、窗口、转角、棚布、地理清单与导出。
- `v4_geometry.py`：把参考轮廓投到真实建筑平面；生成有厚度的墙、洞口、窗框、内嵌窗面与背面补全。坐标之外仍需目视检查造型及遮挡。
- `central_architecture.py`：主代理重写的门桥、塔鼓座/外挑阳台/格栅栏板/拱廊、宽穹顶建筑及独立小穹顶塔。
- `StartingTown_Market_ArchitectureV4.blend`：可编辑 Blender 源。`assets/reference_scenes/StartingTown_Market_ArchitectureV4.glb` 为实际游戏资产。

`landmark_architecture.py` 是被审计否决、未被组装器导入的代理初稿，保留失败证据。`validation/market_architecture_v4/facade_traces_px.json` 和其旧叠图亦未被采用；其中部分边界划入天空。实际使用的数据由组装器保存到 `authored_facades.json`。

实际采集见 [V4 验证记录](../../../validation/market_architecture_v4/README.md)。旧 V3 r10、托尔巴纳、旧 UE 世界均保留。所有不可见背面、街道尺寸与深度是项目补全；未冒充官方测绘。没有使用动画图片作模型贴图、引擎背景或广告牌。

从项目目录运行 `./Run.ps1` 或 `./Run.ps1 -Mode market-demo` 可进入此候选；`R` 恢复参考机位，`O` 俯瞰，`WASD` 行走。`-Mode play` 保留原 HQ 入口。采集使用 `./Run.ps1 -Mode market-demo-capture -Revision manual`。

本轮没有修改全局 Shader、光照、后处理和雾；现有源材质是候选资产材质，未自动升级为全局标准。Kimi 请求为 0，未加载/新建居民持久身份或改动预算。
