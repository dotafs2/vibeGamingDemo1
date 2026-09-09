# Aincrad cold forge source and import order

先运行 `build_forge_kit.py` 生成 `Modules/` 与 `forge_kit_manifest.json`，再运行 `build_workshop_kit.py` 保持木工台和铁匠工作台源模块最新。随后运行 `build_town_kit.py --only-smithy`，它从已有 `smithy_cold_forge.glb` 和 `smithy_workbench.glb` 组装并只更新 `SM_Smithy`、`SM_Smithy_Details` 及对应 recipe。最后在 Unreal 中按需运行 `Tools/import_aincrad_forge_kit.py` 和现有 workshop/town importer；这些工具会分别写入独立报告。

`smithy_cold_forge` 是静态视觉设施：它不创建燃料、库存、热量、锻造能力、合同、金钱或居民动作。`SM_Smithy` 的 recipe stable id 为 `cold_forge`，`SM_Smithy_Details` 不包含完整冷炉模块，避免与旧灰色基座叠加。导入报告只证明源 hash、静态网格 bounds 和材质绑定检查；它不代表运行时安装、碰撞交互或工作行为已经启用。

冷炉场景复核的首次记录 `v2_default_cold_forge_20260908_1959` 保留为失败证据：旧相机局部偏移 `(-300,-400,280)` 叠加炉体在店铺局部 `x=-280 cm` 的位置后越过约 `x=-500 cm` 的墙体，截图只看到空白墙面。复核相机现改为局部 `(-160,-320,225) cm`，目标仍为 `(-10,0,70) cm`；只改变复核相机，不隐藏墙、不移动 NPC 或改变建筑。
