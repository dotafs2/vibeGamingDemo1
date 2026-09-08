# 货运构件

根代理在Blender中制作，复用MedievalLife的分层材质与比例。`Medieval_Freight_Kit.blend`为可编辑源文件，四个模块各有FBX、GLB；`manifest.json`列出层、坐标与材质，`FreightKit_Source_Preview.png`为源文件陈列图。

| 模块 | 层数 | 用途 |
| --- | ---: | --- |
| cargo_planks | 2 | 单块木板，表面和端面；一个instance对应一个库存单位 |
| cargo_beams | 2 | 单根房梁，保留木纹、端面和凿削痕迹 |
| cart_traces | 2 | 可分离的皮革与铁件，连接车辕和马匹挽具 |
| freight_depot_rack | 3 | 空装货架、加固件与标牌，不虚构可用库存 |

真实UE导入和独立冷审计均通过：`UE_Import_Report.json`、`UE_ColdAudit.json`。9个StaticMesh，保留UV和材质，按厘米读回联合尺寸，关闭Nanite。运行目录为 `Content/ThreeHearths/Data/MedievalFreightKitCatalog.json`。源预览与运行截图分开保存，实际运行图见 `../RuntimeReview/马车运送房梁_实际游戏.png`。

目前实际货运只支持已有公共工程授权的房梁。车夫从仓库实际装货，牵马运输、让行并卸货，货物和工资各结算一次；存档保存车辆位置、里程、运单阶段和货物。加载前不显示货物，饥饿中断时保留车与货物，车夫用真实工资购买食物再返回。取消未装货运单可以退回预留，已装货不会瞬移归还。

木板资产已导入，但没有改动原有木板交易链。尚无坐姿驾车、骑乘、马匹饮食或倒车动画；车辆使用带转弯半径的前进路线，不能穿越不支持的窄口。现有21模块加本套4模块，合计25模块/54个静态网格层；人物、马匹的骨骼资源另计，不把动画或材质组合当作额外新模型。
