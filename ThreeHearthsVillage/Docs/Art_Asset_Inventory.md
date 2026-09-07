# 美术资产完整盘点

按本地实际文件生成。每项的路径、字节数、SHA-256 和同名 UE 包记录见 [CSV](Art_Asset_Inventory.csv) 与 [JSON](Art_Asset_Inventory.json)。

本次中世纪项目有 **128 个独立模块、7 个组合示例、3 个小屋整模版本，共 138 个 GLB 导出条目；19 份 Blender 源文件**。计数含整理和复用的模块条目，组合和版本分别计数，不代表同等数量的全新原创几何。

另记录 758 个生成目录中的 UE 原生资产包（包含模型、材质等），48 张/份美术预览与 UV 图，以及 98 份同仓库旧二维项目素材/设计图。完整文件清单同时收录制作脚本、规格和验收媒体。

Cropout 原有村民身体、骨骼、动画、植被、作物、岛屿和音效属于复用素材，不计入本次自制资产。ResidentKit 是 16 件原创建模配件及 10 套外观组合，未新增 10 个独立角色身体。

## 使用状态的边界

- VillageKit 原始 38 件已增加两件木屋顶组件，当前是 40 件。ResidentialVariants 的 40 件审计与这批相同，不能重复加算。
- 木/石住宅、公共墙和私人陶瓦屋顶已有实际施工验收；制瓦订单、运输和四个陶瓦坡面见 Tile_Workshop_Acceptance.md。其余模块是否参与玩法须逐项看运行证据。
- TownKit 六件连接件，以及 WoodProductionKit 两件半成品，在本次文件匹配中没有同名 UE 包；它们的源资产存在，不能据文件存在宣称已在游戏使用。
- PublicWallKit 石墙段复用 SocietyKit 几何，其余三件为配套组件。城堡、市集和室内组合是可编辑装配示例，不等于 NPC 已具备完整城堡建设或居住玩法。
- GoodsKit 中石灰、颜料、铁锭、铁钉、蓝灰瓦等商品模型已制作；相应生产链不因模型存在而自动完成。

## 独立模块、组合示例和版本（逐项）

### VillageKit — 43 个导出条目

| 名称 | 稳定 ID | 类别 | UE 同名包 |
| --- | --- | --- | --- |
| 红陶瓦灰泥小屋组合 | [cottage_terracotta](../Art/VillageKit/examples/cottage_terracotta.glb) | 组合示例 | 有 |
| 蓝灰瓦木长屋组合 | [longhouse_slateblue](../Art/VillageKit/examples/longhouse_slateblue.glb) | 组合示例 | 有 |
| 红陶瓦双层石屋组合 | [townhouse_terracotta](../Art/VillageKit/examples/townhouse_terracotta.glb) | 组合示例 | 有 |
| 2米木梁 | [beam_timber_2m](../Art/VillageKit/modules/beam_timber_2m.glb) | 独立模块 | 有 |
| 木长凳 | [bench_timber](../Art/VillageKit/modules/bench_timber.glb) | 独立模块 | 有 |
| 蓝灰瓦雨棚 | [canopy_slateblue_2m](../Art/VillageKit/modules/canopy_slateblue_2m.glb) | 独立模块 | 有 |
| 红陶瓦雨棚 | [canopy_terracotta_2m](../Art/VillageKit/modules/canopy_terracotta_2m.glb) | 独立模块 | 有 |
| 搬运原木包 | [carry_logs](../Art/VillageKit/modules/carry_logs.glb) | 独立模块 | 有 |
| 搬运木板包 | [carry_planks](../Art/VillageKit/modules/carry_planks.glb) | 独立模块 | 有 |
| 搬运灰浆桶 | [carry_plaster](../Art/VillageKit/modules/carry_plaster.glb) | 独立模块 | 有 |
| 搬运石料箱 | [carry_stones](../Art/VillageKit/modules/carry_stones.glb) | 独立模块 | 有 |
| 搬运瓦片箱 | [carry_tiles](../Art/VillageKit/modules/carry_tiles.glb) | 独立模块 | 有 |
| 橡木门 | [door_oak](../Art/VillageKit/modules/door_oak.glb) | 独立模块 | 有 |
| 楼梯井开口楼板 | [floor_opening_2m](../Art/VillageKit/modules/floor_opening_2m.glb) | 独立模块 | 有 |
| 木楼板 | [floor_timber_2m](../Art/VillageKit/modules/floor_timber_2m.glb) | 独立模块 | 有 |
| 石地基 | [foundation_stone_2m](../Art/VillageKit/modules/foundation_stone_2m.glb) | 独立模块 | 有 |
| 木结构框架 | [frame_timber_2m](../Art/VillageKit/modules/frame_timber_2m.glb) | 独立模块 | 有 |
| 灰泥山墙 | [gable_plaster_4m](../Art/VillageKit/modules/gable_plaster_4m.glb) | 独立模块 | 有 |
| 石山墙 | [gable_stone_4m](../Art/VillageKit/modules/gable_stone_4m.glb) | 独立模块 | 有 |
| 木山墙 | [gable_timber_4m](../Art/VillageKit/modules/gable_timber_4m.glb) | 独立模块 | 有 |
| 门廊木柱 | [porch_post_timber](../Art/VillageKit/modules/porch_post_timber.glb) | 独立模块 | 有 |
| 门廊石台阶 | [porch_steps_stone_2m](../Art/VillageKit/modules/porch_steps_stone_2m.glb) | 独立模块 | 有 |
| 2.4米木柱 | [post_timber_2_4m](../Art/VillageKit/modules/post_timber_2_4m.glb) | 独立模块 | 有 |
| 木栏杆 | [railing_timber_2m](../Art/VillageKit/modules/railing_timber_2m.glb) | 独立模块 | 有 |
| 蓝灰瓦屋脊 | [roof_ridge_slateblue_2m](../Art/VillageKit/modules/roof_ridge_slateblue_2m.glb) | 独立模块 | 有 |
| 红陶瓦屋脊 | [roof_ridge_terracotta_2m](../Art/VillageKit/modules/roof_ridge_terracotta_2m.glb) | 独立模块 | 有 |
| 木屋脊 | [roof_ridge_timber_2m](../Art/VillageKit/modules/roof_ridge_timber_2m.glb) | 独立模块 | 有 |
| 蓝灰瓦坡面 | [roof_slope_slateblue_2m](../Art/VillageKit/modules/roof_slope_slateblue_2m.glb) | 独立模块 | 有 |
| 红陶瓦坡面 | [roof_slope_terracotta_2m](../Art/VillageKit/modules/roof_slope_terracotta_2m.glb) | 独立模块 | 有 |
| 木屋顶坡面 | [roof_slope_timber_2m](../Art/VillageKit/modules/roof_slope_timber_2m.glb) | 独立模块 | 有 |
| 鼠尾草绿窗板 | [shutter_sage](../Art/VillageKit/modules/shutter_sage.glb) | 独立模块 | 有 |
| 折返楼梯 | [stairs_switchback_2x4m](../Art/VillageKit/modules/stairs_switchback_2x4m.glb) | 独立模块 | 有 |
| 公共木桌 | [table_communal](../Art/VillageKit/modules/table_communal.glb) | 独立模块 | 有 |
| 带门洞灰泥墙 | [wall_door_plaster_2m](../Art/VillageKit/modules/wall_door_plaster_2m.glb) | 独立模块 | 有 |
| 带门洞石墙 | [wall_door_stone_2m](../Art/VillageKit/modules/wall_door_stone_2m.glb) | 独立模块 | 有 |
| 带门洞木墙 | [wall_door_timber_2m](../Art/VillageKit/modules/wall_door_timber_2m.glb) | 独立模块 | 有 |
| 灰泥实墙 | [wall_plaster_2m](../Art/VillageKit/modules/wall_plaster_2m.glb) | 独立模块 | 有 |
| 石实墙 | [wall_stone_2m](../Art/VillageKit/modules/wall_stone_2m.glb) | 独立模块 | 有 |
| 木实墙 | [wall_timber_2m](../Art/VillageKit/modules/wall_timber_2m.glb) | 独立模块 | 有 |
| 带窗灰泥墙 | [wall_window_plaster_2m](../Art/VillageKit/modules/wall_window_plaster_2m.glb) | 独立模块 | 有 |
| 带窗石墙 | [wall_window_stone_2m](../Art/VillageKit/modules/wall_window_stone_2m.glb) | 独立模块 | 有 |
| 带窗木墙 | [wall_window_timber_2m](../Art/VillageKit/modules/wall_window_timber_2m.glb) | 独立模块 | 有 |
| 木工工作台 | [workbench_carpenter](../Art/VillageKit/modules/workbench_carpenter.glb) | 独立模块 | 有 |

### SocietyKit — 34 个导出条目

| 名称 | 稳定 ID | 类别 | UE 同名包 |
| --- | --- | --- | --- |
| 行会市集院落组合 | [guild_market_yard](../Art/SocietyKit/examples/guild_market_yard.glb) | 组合示例 | 有 |
| 国王城门院落组合 | [kings_gate_courtyard](../Art/SocietyKit/examples/kings_gate_courtyard.glb) | 组合示例 | 有 |
| 城堡石扶壁 | [castle_buttress_stone](../Art/SocietyKit/modules/castle_buttress_stone.glb) | 独立模块 | 有 |
| 4米城门石拱 | [castle_gate_arch_4m](../Art/SocietyKit/modules/castle_gate_arch_4m.glb) | 独立模块 | 有 |
| 双扇橡木城门 | [castle_gate_oak_pair](../Art/SocietyKit/modules/castle_gate_oak_pair.glb) | 独立模块 | 有 |
| 塔楼垛口顶段 | [castle_tower_battlement_cap_4m](../Art/SocietyKit/modules/castle_tower_battlement_cap_4m.glb) | 独立模块 | 有 |
| 塔楼层段 | [castle_tower_storey_4m](../Art/SocietyKit/modules/castle_tower_storey_4m.glb) | 独立模块 | 有 |
| 城墙木步道 | [castle_walkway_timber_2m](../Art/SocietyKit/modules/castle_walkway_timber_2m.glb) | 独立模块 | 有 |
| 城墙垛口段 | [castle_wall_battlement_2m](../Art/SocietyKit/modules/castle_wall_battlement_2m.glb) | 独立模块 | 有 |
| 城堡石墙段 | [castle_wall_stone_2m](../Art/SocietyKit/modules/castle_wall_stone_2m.glb) | 独立模块 | 有 |
| 成品房梁捆 | [goods_beams_bundle](../Art/SocietyKit/modules/goods_beams_bundle.glb) | 独立模块 | 有 |
| 砖瓦混装箱 | [goods_bricks_tiles_crate](../Art/SocietyKit/modules/goods_bricks_tiles_crate.glb) | 独立模块 | 有 |
| 涂料桶组 | [goods_paint_pails](../Art/SocietyKit/modules/goods_paint_pails.glb) | 独立模块 | 有 |
| 成品木板捆 | [goods_planks_bundle](../Art/SocietyKit/modules/goods_planks_bundle.glb) | 独立模块 | 有 |
| 蓝色王国旗 | [kingdom_banner_blue](../Art/SocietyKit/modules/kingdom_banner_blue.glb) | 独立模块 | 有 |
| 红色王国旗 | [kingdom_banner_red](../Art/SocietyKit/modules/kingdom_banner_red.glb) | 独立模块 | 有 |
| 市场橡木桶 | [market_barrel_oak](../Art/SocietyKit/modules/market_barrel_oak.glb) | 独立模块 | 有 |
| 市场橡木箱 | [market_crate_oak](../Art/SocietyKit/modules/market_crate_oak.glb) | 独立模块 | 有 |
| 蓝色市集摊位 | [market_stall_blue_2m](../Art/SocietyKit/modules/market_stall_blue_2m.glb) | 独立模块 | 有 |
| 红色市集摊位 | [market_stall_red_2m](../Art/SocietyKit/modules/market_stall_red_2m.glb) | 独立模块 | 有 |
| 木匠帽 | [profession_carpenter_cap](../Art/SocietyKit/modules/profession_carpenter_cap.glb) | 独立模块 | 有 |
| 石匠兜帽 | [profession_mason_hood](../Art/SocietyKit/modules/profession_mason_hood.glb) | 独立模块 | 有 |
| 铁匠围裙 | [profession_smith_apron](../Art/SocietyKit/modules/profession_smith_apron.glb) | 独立模块 | 有 |
| 国王皇冠 | [regalia_king_crown](../Art/SocietyKit/modules/regalia_king_crown.glb) | 独立模块 | 有 |
| 木工锤 | [tool_carpenter_hammer](../Art/SocietyKit/modules/tool_carpenter_hammer.glb) | 独立模块 | 有 |
| 木工锯 | [tool_carpenter_saw](../Art/SocietyKit/modules/tool_carpenter_saw.glb) | 独立模块 | 有 |
| 石匠凿 | [tool_mason_chisel](../Art/SocietyKit/modules/tool_mason_chisel.glb) | 独立模块 | 有 |
| 石匠抹刀 | [tool_mason_trowel](../Art/SocietyKit/modules/tool_mason_trowel.glb) | 独立模块 | 有 |
| 村庄灯柱 | [village_lantern_post](../Art/SocietyKit/modules/village_lantern_post.glb) | 独立模块 | 有 |
| 村庄告示牌 | [village_notice_board](../Art/SocietyKit/modules/village_notice_board.glb) | 独立模块 | 有 |
| 铁匠锻炉 | [workshop_blacksmith_forge](../Art/SocietyKit/modules/workshop_blacksmith_forge.glb) | 独立模块 | 有 |
| 木工工位 | [workshop_carpenter_station](../Art/SocietyKit/modules/workshop_carpenter_station.glb) | 独立模块 | 有 |
| 石工工位 | [workshop_mason_station](../Art/SocietyKit/modules/workshop_mason_station.glb) | 独立模块 | 有 |
| 烧瓦窑 | [workshop_tile_kiln](../Art/SocietyKit/modules/workshop_tile_kiln.glb) | 独立模块 | 有 |

### ResidentKit — 16 个导出条目

| 名称 | 稳定 ID | 类别 | UE 同名包 |
| --- | --- | --- | --- |
| 亚麻短围裙 | [apron_linen_short](../Art/ResidentKit/modules/apron_linen_short.glb) | 独立模块 | 有 |
| 背包与铺盖卷 | [backpack_bedroll](../Art/ResidentKit/modules/backpack_bedroll.glb) | 独立模块 | 有 |
| 皮革斜挎包 | [bag_crossbody_leather](../Art/ResidentKit/modules/bag_crossbody_leather.glb) | 独立模块 | 有 |
| 整齐银胡须 | [beard_neat_silver](../Art/ResidentKit/modules/beard_neat_silver.glb) | 独立模块 | 有 |
| 梅紫商人帽 | [cap_merchant_plum](../Art/ResidentKit/modules/cap_merchant_plum.glb) | 独立模块 | 有 |
| 王室蓝披风 | [cape_royal_blue](../Art/ResidentKit/modules/cape_royal_blue.glb) | 独立模块 | 有 |
| 赤褐辫发 | [hair_braid_auburn](../Art/ResidentKit/modules/hair_braid_auburn.glb) | 独立模块 | 有 |
| 深色发髻 | [hair_bun_dark](../Art/ResidentKit/modules/hair_bun_dark.glb) | 独立模块 | 有 |
| 深色短发 | [hair_cropped_dark](../Art/ResidentKit/modules/hair_cropped_dark.glb) | 独立模块 | 有 |
| 银色侧梳发 | [hair_swept_silver](../Art/ResidentKit/modules/hair_swept_silver.glb) | 独立模块 | 有 |
| 栗色波浪发 | [hair_waves_chestnut](../Art/ResidentKit/modules/hair_waves_chestnut.glb) | 独立模块 | 有 |
| 宽檐草帽 | [hat_straw_wide](../Art/ResidentKit/modules/hat_straw_wide.glb) | 独立模块 | 有 |
| 鼠尾草绿头巾 | [headwrap_sage](../Art/ResidentKit/modules/headwrap_sage.glb) | 独立模块 | 有 |
| 双腰包 | [pouch_belt_double](../Art/ResidentKit/modules/pouch_belt_double.glb) | 独立模块 | 有 |
| 红围巾 | [scarf_red](../Art/ResidentKit/modules/scarf_red.glb) | 独立模块 | 有 |
| 赭黄披肩 | [shawl_ochre](../Art/ResidentKit/modules/shawl_ochre.glb) | 独立模块 | 有 |

### HomeLifeKit — 14 个导出条目

| 名称 | 稳定 ID | 类别 | UE 同名包 |
| --- | --- | --- | --- |
| 4×4米居住角组合 | [cabin_living_4x4m](../Art/HomeLifeKit/examples/cabin_living_4x4m.glb) | 组合示例 | 有 |
| 十人公共餐区组合 | [common_meal_10_seats](../Art/HomeLifeKit/examples/common_meal_10_seats.glb) | 组合示例 | 有 |
| 浆果篮 | [basket_berries](../Art/HomeLifeKit/modules/basket_berries.glb) | 独立模块 | 有 |
| 空篮 | [basket_empty](../Art/HomeLifeKit/modules/basket_empty.glb) | 独立模块 | 有 |
| 双人床 | [bed_double_2x2m](../Art/HomeLifeKit/modules/bed_double_2x2m.glb) | 独立模块 | 有 |
| 单人床 | [bed_single_1_1x2m](../Art/HomeLifeKit/modules/bed_single_1_1x2m.glb) | 独立模块 | 有 |
| 靠背长凳 | [bench_backed_1_8m](../Art/HomeLifeKit/modules/bench_backed_1_8m.glb) | 独立模块 | 有 |
| 宽橡木椅 | [chair_oak_wide](../Art/HomeLifeKit/modules/chair_oak_wide.glb) | 独立模块 | 有 |
| 矮木围栏 | [fence_low_2m](../Art/HomeLifeKit/modules/fence_low_2m.glb) | 独立模块 | 有 |
| 柴火堆 | [firewood_stack](../Art/HomeLifeKit/modules/firewood_stack.glb) | 独立模块 | 有 |
| 面包餐盘 | [food_tray_bread](../Art/HomeLifeKit/modules/food_tray_bread.glb) | 独立模块 | 有 |
| 粮食箱 | [grain_chest](../Art/HomeLifeKit/modules/grain_chest.glb) | 独立模块 | 有 |
| 2.6米公共餐桌 | [table_communal_2_6m](../Art/HomeLifeKit/modules/table_communal_2_6m.glb) | 独立模块 | 有 |
| 1.2米家用餐桌 | [table_dining_1_2m](../Art/HomeLifeKit/modules/table_dining_1_2m.glb) | 独立模块 | 有 |

### GoodsKit — 8 个导出条目

| 名称 | 稳定 ID | 类别 | UE 同名包 |
| --- | --- | --- | --- |
| 烧制砖块箱 | [goods_bricks_crate](../Art/GoodsKit/modules/goods_bricks_crate.glb) | 独立模块 | 有 |
| 铁锭捆 | [goods_iron_ingots_bundle](../Art/GoodsKit/modules/goods_iron_ingots_bundle.glb) | 独立模块 | 有 |
| 石灰桶 | [goods_lime_pail](../Art/GoodsKit/modules/goods_lime_pail.glb) | 独立模块 | 有 |
| 铁钉盒 | [goods_nails_box](../Art/GoodsKit/modules/goods_nails_box.glb) | 独立模块 | 有 |
| 颜料罐组 | [goods_pigment_pots](../Art/GoodsKit/modules/goods_pigment_pots.glb) | 独立模块 | 有 |
| 黏土篮 | [goods_raw_clay_basket](../Art/GoodsKit/modules/goods_raw_clay_basket.glb) | 独立模块 | 有 |
| 蓝灰瓦商品箱 | [goods_tiles_slateblue_crate](../Art/GoodsKit/modules/goods_tiles_slateblue_crate.glb) | 独立模块 | 有 |
| 红陶瓦商品箱 | [goods_tiles_terracotta_crate](../Art/GoodsKit/modules/goods_tiles_terracotta_crate.glb) | 独立模块 | 有 |

### ToolKit — 8 个导出条目

| 名称 | 稳定 ID | 类别 | UE 同名包 |
| --- | --- | --- | --- |
| 斧头 | [tool_axe](../Art/ToolKit/modules/tool_axe.glb) | 独立模块 | 有 |
| 铁锤 | [tool_hammer](../Art/ToolKit/modules/tool_hammer.glb) | 独立模块 | 有 |
| 锄头 | [tool_hoe](../Art/ToolKit/modules/tool_hoe.glb) | 独立模块 | 有 |
| 木槌 | [tool_mallet](../Art/ToolKit/modules/tool_mallet.glb) | 独立模块 | 有 |
| 镐 | [tool_pickaxe](../Art/ToolKit/modules/tool_pickaxe.glb) | 独立模块 | 有 |
| 手锯 | [tool_saw](../Art/ToolKit/modules/tool_saw.glb) | 独立模块 | 有 |
| 铲子 | [tool_shovel](../Art/ToolKit/modules/tool_shovel.glb) | 独立模块 | 有 |
| 砌筑抹刀 | [tool_trowel](../Art/ToolKit/modules/tool_trowel.glb) | 独立模块 | 有 |

### PublicWallKit — 4 个导出条目

| 名称 | 稳定 ID | 类别 | UE 同名包 |
| --- | --- | --- | --- |
| 公共城墙石基座 | [public_wall_foundation_2m](../Art/PublicWallKit/modules/public_wall_foundation_2m.glb) | 独立模块 | 有 |
| 公共城墙木护栏 | [public_wall_parapet_2m](../Art/PublicWallKit/modules/public_wall_parapet_2m.glb) | 独立模块 | 有 |
| 公共城墙石墙段（复用城堡墙几何） | [public_wall_stone_2m](../Art/PublicWallKit/modules/public_wall_stone_2m.glb) | 独立模块 | 有 |
| 公共城墙木步道 | [public_wall_walkway_2m](../Art/PublicWallKit/modules/public_wall_walkway_2m.glb) | 独立模块 | 有 |

### WoodProductionKit — 2 个导出条目

| 名称 | 稳定 ID | 类别 | UE 同名包 |
| --- | --- | --- | --- |
| 原木加工房梁半成品 | [wip_log_to_beam](../Art/WoodProductionKit/modules/wip_log_to_beam.glb) | 独立模块 | 未匹配 |
| 原木锯板半成品 | [wip_log_to_planks](../Art/WoodProductionKit/modules/wip_log_to_planks.glb) | 独立模块 | 未匹配 |

### TownKit — 6 个导出条目

| 名称 | 稳定 ID | 类别 | UE 同名包 |
| --- | --- | --- | --- |
| 街区直角石墙连接件 | [town_corner_stone_2m](../Art/TownKit/modules/town_corner_stone_2m.glb) | 独立模块 | 未匹配 |
| 木屋顶端封板 | [town_gable_end_timber_2m](../Art/TownKit/modules/town_gable_end_timber_2m.glb) | 独立模块 | 未匹配 |
| 屋脊连接节点 | [town_roof_ridge_joint_2m](../Art/TownKit/modules/town_roof_ridge_joint_2m.glb) | 独立模块 | 未匹配 |
| L/T屋顶谷线节点 | [town_roof_valley_joint_2m](../Art/TownKit/modules/town_roof_valley_joint_2m.glb) | 独立模块 | 未匹配 |
| 八级木楼梯段 | [town_stair_timber_2m](../Art/TownKit/modules/town_stair_timber_2m.glb) | 独立模块 | 未匹配 |
| 带中央通道木墙 | [town_wall_gate_timber_2m](../Art/TownKit/modules/town_wall_gate_timber_2m.glb) | 独立模块 | 未匹配 |

### HearthCottage — 3 个导出条目

| 名称 | 稳定 ID | 类别 | UE 同名包 |
| --- | --- | --- | --- |
| 原始小屋整模 | [HearthCottage](../Art/HearthCottage/HearthCottage.glb) | 整模版本 | 未匹配 |
| 共享UV小屋整模 | [HearthCottage_SharedUV](../Art/HearthCottage/HearthCottage_SharedUV.glb) | 整模版本 | 未匹配 |
| 共享UV与屋顶高光改进小屋整模 | [HearthCottage_SharedUV_Polished](../Art/HearthCottage/HearthCottage_SharedUV_Polished.glb) | 整模版本 | 有 |

## Blender 源文件（逐项）

- [GoodsKit/GoodsKit.blend](../Art/GoodsKit/GoodsKit.blend)
- [HearthCottage/HearthCottage.blend](../Art/HearthCottage/HearthCottage.blend)
- [HearthCottage/HearthCottage_SharedUV.blend](../Art/HearthCottage/HearthCottage_SharedUV.blend)
- [HearthCottage/HearthCottage_SharedUV_Polished.blend](../Art/HearthCottage/HearthCottage_SharedUV_Polished.blend)
- [HearthCottage/HearthCottage_UV.blend](../Art/HearthCottage/HearthCottage_UV.blend)
- [HomeLifeKit/cabin_living_4x4m.blend](../Art/HomeLifeKit/examples/cabin_living_4x4m.blend)
- [HomeLifeKit/common_meal_10_seats.blend](../Art/HomeLifeKit/examples/common_meal_10_seats.blend)
- [HomeLifeKit/HomeLifeKit.blend](../Art/HomeLifeKit/HomeLifeKit.blend)
- [PublicWallKit/PublicWallKit.blend](../Art/PublicWallKit/PublicWallKit.blend)
- [ResidentKit/ResidentKit.blend](../Art/ResidentKit/ResidentKit.blend)
- [SocietyKit/SocietyKit.blend](../Art/SocietyKit/SocietyKit.blend)
- [SocietyKit/SocietyKit_guild_market_yard.blend](../Art/SocietyKit/SocietyKit_guild_market_yard.blend)
- [SocietyKit/SocietyKit_kings_gate_courtyard.blend](../Art/SocietyKit/SocietyKit_kings_gate_courtyard.blend)
- [Stage4HouseAudit/TimberRoofVariants.blend](../Art/Stage4HouseAudit/TimberRoofVariants.blend)
- [ToolKit/ToolKit.blend](../Art/ToolKit/ToolKit.blend)
- [VillageKit/VillageKit.blend](../Art/VillageKit/VillageKit.blend)
- [VillageKit/VillageKit_Examples.blend](../Art/VillageKit/VillageKit_Examples.blend)
- [VillageKit/VillageKit_RoofStudy.blend](../Art/VillageKit/VillageKit_RoofStudy.blend)
- [WoodProductionKit/WoodProductionKit.blend](../Art/WoodProductionKit/WoodProductionKit.blend)

## 同仓库较早的矿场二维项目

这部分包含已制作的多时代场景与透明拆件，以及用户提供的参考/原图；不全部宣称原创。来源说明见仓库 public/art/ASSET_PROVENANCE.md。按目录列出全部文件，避免遗漏旧项目成果。

### docs（1）

- [boss-concept-v01.png](../../docs/boss-concept-v01.png)

### docs/art-direction（1）

- [continuous-master-layout.svg](../../docs/art-direction/continuous-master-layout.svg)

### public/art/boss-arena（2）

- [excavator-hall-past.png](../../public/art/boss-arena/excavator-hall-past.png)
- [excavator-hall-present.png](../../public/art/boss-arena/excavator-hall-present.png)

### public/art/elevator（4）

- [elevator-lab-far-2047-master.png](../../public/art/elevator/elevator-lab-far-2047-master.png)
- [elevator-lab-far-2147-master.png](../../public/art/elevator/elevator-lab-far-2147-master.png)
- [elevator-lab-mid-2047-master.png](../../public/art/elevator/elevator-lab-mid-2047-master.png)
- [elevator-lab-mid-2147-master.png](../../public/art/elevator/elevator-lab-mid-2147-master.png)

### public/art/gate（5）

- [surface-facility-right-extension-v2.png](../../public/art/gate/surface-facility-right-extension-v2.png)
- [surface-facility-shared.png](../../public/art/gate/surface-facility-shared.png)
- [surface-facility-transition-v2.png](../../public/art/gate/surface-facility-transition-v2.png)
- [surface-models-master-v3.png](../../public/art/gate/surface-models-master-v3.png)
- [surface-terrain-only-v3.png](../../public/art/gate/surface-terrain-only-v3.png)

### public/art/lab（8）

- [lab-2047-far.png](../../public/art/lab/lab-2047-far.png)
- [lab-2047-longscroll.png](../../public/art/lab/lab-2047-longscroll.png)
- [lab-2047-mid.png](../../public/art/lab/lab-2047-mid.png)
- [lab-2047-near.png](../../public/art/lab/lab-2047-near.png)
- [lab-2147-far.png](../../public/art/lab/lab-2147-far.png)
- [lab-2147-longscroll.png](../../public/art/lab/lab-2147-longscroll.png)
- [lab-2147-mid.png](../../public/art/lab/lab-2147-mid.png)
- [lab-2147-near.png](../../public/art/lab/lab-2147-near.png)

### public/editor/assets/asset-1786172676762-7yyh4（7）

- [v-1786172676762-direct-past.png](../../public/editor/assets/asset-1786172676762-7yyh4/v-1786172676762-direct-past.png)
- [v-1786172676762-direct-present.png](../../public/editor/assets/asset-1786172676762-7yyh4/v-1786172676762-direct-present.png)
- [v-1786172676762-past.png](../../public/editor/assets/asset-1786172676762-7yyh4/v-1786172676762-past.png)
- [v-1786172676762-present.png](../../public/editor/assets/asset-1786172676762-7yyh4/v-1786172676762-present.png)
- [v3-scene-match-past.png](../../public/editor/assets/asset-1786172676762-7yyh4/v3-scene-match-past.png)
- [v3-scene-match-present.png](../../public/editor/assets/asset-1786172676762-7yyh4/v3-scene-match-present.png)
- [v4-dual-era-present.png](../../public/editor/assets/asset-1786172676762-7yyh4/v4-dual-era-present.png)

### public/editor/assets/scene-ceiling-cables（7）

- [source-pair-alpha.png](../../public/editor/assets/scene-ceiling-cables/source-pair-alpha.png)
- [source-pair.png](../../public/editor/assets/scene-ceiling-cables/source-pair.png)
- [v1-past.png](../../public/editor/assets/scene-ceiling-cables/v1-past.png)
- [v1-present.png](../../public/editor/assets/scene-ceiling-cables/v1-present.png)
- [v3-scene-match-past.png](../../public/editor/assets/scene-ceiling-cables/v3-scene-match-past.png)
- [v3-scene-match-present.png](../../public/editor/assets/scene-ceiling-cables/v3-scene-match-present.png)
- [v4-dual-era-present.png](../../public/editor/assets/scene-ceiling-cables/v4-dual-era-present.png)

### public/editor/assets/scene-conveyor（7）

- [source-pair-alpha.png](../../public/editor/assets/scene-conveyor/source-pair-alpha.png)
- [source-pair.png](../../public/editor/assets/scene-conveyor/source-pair.png)
- [v1-past.png](../../public/editor/assets/scene-conveyor/v1-past.png)
- [v1-present.png](../../public/editor/assets/scene-conveyor/v1-present.png)
- [v3-scene-match-past.png](../../public/editor/assets/scene-conveyor/v3-scene-match-past.png)
- [v3-scene-match-present.png](../../public/editor/assets/scene-conveyor/v3-scene-match-present.png)
- [v4-dual-era-present.png](../../public/editor/assets/scene-conveyor/v4-dual-era-present.png)

### public/editor/assets/scene-crusher（7）

- [source-pair-alpha.png](../../public/editor/assets/scene-crusher/source-pair-alpha.png)
- [source-pair.png](../../public/editor/assets/scene-crusher/source-pair.png)
- [v1-past.png](../../public/editor/assets/scene-crusher/v1-past.png)
- [v1-present.png](../../public/editor/assets/scene-crusher/v1-present.png)
- [v3-scene-match-past.png](../../public/editor/assets/scene-crusher/v3-scene-match-past.png)
- [v3-scene-match-present.png](../../public/editor/assets/scene-crusher/v3-scene-match-present.png)
- [v4-dual-era-present.png](../../public/editor/assets/scene-crusher/v4-dual-era-present.png)

### public/editor/assets/scene-gate（7）

- [source-pair-alpha.png](../../public/editor/assets/scene-gate/source-pair-alpha.png)
- [source-pair.png](../../public/editor/assets/scene-gate/source-pair.png)
- [v1-past.png](../../public/editor/assets/scene-gate/v1-past.png)
- [v1-present.png](../../public/editor/assets/scene-gate/v1-present.png)
- [v3-scene-match-past.png](../../public/editor/assets/scene-gate/v3-scene-match-past.png)
- [v3-scene-match-present.png](../../public/editor/assets/scene-gate/v3-scene-match-present.png)
- [v4-dual-era-present.png](../../public/editor/assets/scene-gate/v4-dual-era-present.png)

### public/editor/assets/scene-gate-leaf（3）

- [v3-scene-match-past.png](../../public/editor/assets/scene-gate-leaf/v3-scene-match-past.png)
- [v3-scene-match-present.png](../../public/editor/assets/scene-gate-leaf/v3-scene-match-present.png)
- [v4-dual-era-present.png](../../public/editor/assets/scene-gate-leaf/v4-dual-era-present.png)

### public/editor/assets/scene-maintenance（7）

- [source-pair-alpha.png](../../public/editor/assets/scene-maintenance/source-pair-alpha.png)
- [source-pair.png](../../public/editor/assets/scene-maintenance/source-pair.png)
- [v1-past.png](../../public/editor/assets/scene-maintenance/v1-past.png)
- [v1-present.png](../../public/editor/assets/scene-maintenance/v1-present.png)
- [v3-scene-match-past.png](../../public/editor/assets/scene-maintenance/v3-scene-match-past.png)
- [v3-scene-match-present.png](../../public/editor/assets/scene-maintenance/v3-scene-match-present.png)
- [v4-dual-era-present.png](../../public/editor/assets/scene-maintenance/v4-dual-era-present.png)

### public/editor/assets/scene-rails（7）

- [source-pair-alpha.png](../../public/editor/assets/scene-rails/source-pair-alpha.png)
- [source-pair.png](../../public/editor/assets/scene-rails/source-pair.png)
- [v1-past.png](../../public/editor/assets/scene-rails/v1-past.png)
- [v1-present.png](../../public/editor/assets/scene-rails/v1-present.png)
- [v3-scene-match-past.png](../../public/editor/assets/scene-rails/v3-scene-match-past.png)
- [v3-scene-match-present.png](../../public/editor/assets/scene-rails/v3-scene-match-present.png)
- [v4-dual-era-present.png](../../public/editor/assets/scene-rails/v4-dual-era-present.png)

### public/editor/assets/scene-shaft（7）

- [source-pair-alpha.png](../../public/editor/assets/scene-shaft/source-pair-alpha.png)
- [source-pair.png](../../public/editor/assets/scene-shaft/source-pair.png)
- [v1-past.png](../../public/editor/assets/scene-shaft/v1-past.png)
- [v1-present.png](../../public/editor/assets/scene-shaft/v1-present.png)
- [v3-scene-match-past.png](../../public/editor/assets/scene-shaft/v3-scene-match-past.png)
- [v3-scene-match-present.png](../../public/editor/assets/scene-shaft/v3-scene-match-present.png)
- [v4-dual-era-present.png](../../public/editor/assets/scene-shaft/v4-dual-era-present.png)

### public/editor/assets/scene-vent-housing（7）

- [source-pair-alpha.png](../../public/editor/assets/scene-vent-housing/source-pair-alpha.png)
- [source-pair.png](../../public/editor/assets/scene-vent-housing/source-pair.png)
- [v1-past.png](../../public/editor/assets/scene-vent-housing/v1-past.png)
- [v1-present.png](../../public/editor/assets/scene-vent-housing/v1-present.png)
- [v3-scene-match-past.png](../../public/editor/assets/scene-vent-housing/v3-scene-match-past.png)
- [v3-scene-match-present.png](../../public/editor/assets/scene-vent-housing/v3-scene-match-present.png)
- [v4-dual-era-present.png](../../public/editor/assets/scene-vent-housing/v4-dual-era-present.png)

### public/editor/assets/system-vent-fan（5）

- [v3-scene-match-past.png](../../public/editor/assets/system-vent-fan/v3-scene-match-past.png)
- [v3-scene-match-present.png](../../public/editor/assets/system-vent-fan/v3-scene-match-present.png)
- [v4-dual-era-present.png](../../public/editor/assets/system-vent-fan/v4-dual-era-present.png)
- [vent-fan-2047.png](../../public/editor/assets/system-vent-fan/vent-fan-2047.png)
- [vent-fan-2147.png](../../public/editor/assets/system-vent-fan/vent-fan-2147.png)

### public/editor/references（2）

- [zero-echo-style-reference-2047.png](../../public/editor/references/zero-echo-style-reference-2047.png)
- [zero-echo-style-reference-2147.png](../../public/editor/references/zero-echo-style-reference-2147.png)

### public/editor/scene-art（4）

- [upper-room-2047.png](../../public/editor/scene-art/upper-room-2047.png)
- [upper-room-2147.png](../../public/editor/scene-art/upper-room-2147.png)
- [upper-room-background-2047-v1.png](../../public/editor/scene-art/upper-room-background-2047-v1.png)
- [upper-room-background-2147-v1.png](../../public/editor/scene-art/upper-room-background-2147-v1.png)

## 可复现与上传核对

运行 `python ThreeHearthsVillage/Art/build_asset_inventory.py` 可重新扫描。JSON 和 CSV 逐文件带哈希，可与 GitHub 当前交付分支核对。此清单记录本地存在性与包名匹配；上传完成状态须以最终远端 commit 和实际推送结果为准。
