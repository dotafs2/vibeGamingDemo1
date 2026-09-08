# 工坊与门前生活组件

根代理亲自用 Blender 制作并查看了 `MarketLifeKit_Source_Preview.png`。五个模块可独立组合，结构、表面、装备/布面为分离网格层。图中重复的陶罐都是同一个模块实例，不作为新模型计数。

| 模块 | 层 | 用途 |
| --- | --- | --- |
| bench_low | structure / finish | 两人木长凳，坐面高0.505米，榫接横撑和分离表面细节 |
| work_table | structure / finish | 空工作桌，台面高0.93米，可作货台或晾坯支撑 |
| tool_rack | structure / finish / equipment | 带支脚工具架；锤、钳、锯属于可隐藏的装备层 |
| linen_canopy | structure / cover / finish | 2.42米宽独立布棚，可按2.4米间隔接成多跨 |
| clay_jar | structure / finish | 单个空陶罐，有真实内壁、口沿和把手 |

共5 FBX、5 GLB、12个静态网格层和可编辑 `Medieval_Market_Life_Kit.blend`。米制、底部原点、前方-Y；材质使用既有 MedievalLife 色板。manifest记录每件源哈希、实测尺寸、锚点与三角面数量。

根于北京时间2026-09-08 07:21实际执行原生导入，随后独立冷加载验收通过全部12层：`UE_Import_Report.json`、`UE_ColdAudit.json`。索引位于 `Content/ThreeHearths/Data/MedievalMarketLifeCatalog.json`。验证了UV、材质、源文件、轴转换、厘米尺寸和Nanite关闭。

长凳、工作桌、空工具架现已接入明确需求、可达场地、步行施工、材料付款和冷存档。实测只有米拉的真实Kimi `request_2` 与现有配方匹配：她走到门边，施工5模拟秒，支付3币、消耗2木板与1房梁，建成一张长凳。重启保留同一件物品和同一笔付款。施工中的旅行/工作也通过冷恢复测试；缺料、撤销需求、重复请求不会扣款。

第一次实景审阅暴露长凳占了道路，根拒绝该结果并保留记录；加入整件物品的道路余量与真实房屋网格检查后，从施工前检查点重新验收。最终长凳在门边草地，距道路边界扣除整个物品包围半径后仍有64.5厘米余量，源脚底贴地。见 `../RuntimeReview/居民自建长凳_实际游戏.png` 和 `Saved/ThreeHearths/MedievalReview/MarketLiveReview/market-live-report.json`。

完成的物件会进入NPC事实和新的视觉观察签名，避免把已经建好的长凳再次当作缺失物件。布棚与陶罐目前提供原生资源；布料/成品陶器生产、工具装备层、坐姿动画和工作桌生产交互仍待接入。没有匹配的居民请求时不会自动摆满村庄，也不会赠送库存。

北京时间08:59，第二轮也通过实际验收：Kimi看到了第一张凳，明确提出另一侧追加一张，形成独立request_11。现在按请求而不是按模块去重，允许有明确目的的额外实例，每屋容量16。居民真实再次施工，第二次独立支付3币/2木板/1房梁，两件相距930.6cm。600现实秒稳定运行与35秒冷恢复均正常结束，market-multiplicity-report.json passed，最终双凳原始截图见 `../RuntimeReview/居民两轮自建长凳_实际游戏.png`；这仍只有一种bench_low美术模块。
