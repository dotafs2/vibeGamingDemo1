# 第一段公共城墙：PublicWallKit

本组只有四类独立安装构件：石基座、石墙段、木墙顶步道和木护栏。石墙段直接复用 SocietyKit 的 `castle_wall_stone_2m` 几何、UV 和石材；其他三件为配套的新最小构件，沿用其蓝灰石材以及 VillageKit 栗木、蜂蜜木板和切面橡木 PBR。既有资产未改动，也未生成整座城墙的一体 GLB。

源文件 `PublicWallKit.blend` 只含四个独立局部网格。逐件 GLB 在 `modules/`，规格与材料输入在 `catalog.json`，配方在 `recipes.json`，15 个稳定安装实例及依赖在 `assembly.json`。本次由 Blender MCP 实际创建、导出和渲染；未操作 UE 导入、C++ 或 Git 提交。

## 模块与材料输入

单位为米，+Z 向上、-Y 为前、长度沿 +X，按 2 米节距拼接。GLB 按标准转换为 +Y 向上；转回作者坐标为 `(x,-z,y)`。四件均为单个局部网格节点、单位变换，并含 UV 和自包含 PBR，无外部图片或程序材质依赖。

| 稳定组件 ID | 实际尺寸 X×Y×Z（米） | 三角形 | 单件输入草案 |
| --- | --- | ---: | --- |
| `public_wall_foundation_2m` | 1.994 × 0.894 × 0.280 | 352 | 4 stone |
| `public_wall_stone_2m` | 1.994 × 0.400 × 2.394 | 1,584 | 8 stone |
| `public_wall_walkway_2m` | 2.000 × 0.900 × 0.220 | 528 | 3 planks + 2 beams |
| `public_wall_parapet_2m` | 2.000 × 0.140 × 0.740 | 440 | 2 planks + 1 beam |

共 2,904 三角形。每件只有 3 个非金属材质，粗糙度 0.43–0.57；颜色和粗糙度直接取自引用资产，没有重新猜色。原始引用和哈希见 `reuse-manifest.json`。

新基座、步道、护栏原点位于底面中心 Z=0，安装锚点为 `[0,0,0]`。复用石墙保留原 SocietyKit 结构基准，几何 Z=0.003…2.397 米，底部 3 毫米留缝属于原始几何；不要把新原点再次平移到可见石块底面。侧向石块拼缝为约 6 毫米，按 2 米结构网格安装。

## NPC 搬运与配方范围

NPC 搬运材料包，整面墙、步道和护栏只作为现场安装结果。所有模块明确 `npc_handheld: false`、`execution_unit: one_site_installation`。可复用的携带视觉为：

| 输入键 | 现有库存字段 | 搬运视觉 |
| --- | --- | --- |
| `stone` | `StoneStock` | `VillageKit/modules/carry_stones.glb` |
| `planks` | `PlankStock` | `VillageKit/modules/carry_planks.glb` |
| `beams` | `BeamStock` | `SocietyKit/modules/goods_beams_bundle.glb` |

`beams` 对应现有房梁库存；SocietyKit 搬运视觉目录里的旧资源别名为 `timber_beams`。这里明确记录别名，避免误当成第二种房梁库存。单次交付可按 1 个库存单位处理，画面里的石块或木板数量不定义库存单位。

所有数量均为这组公共建设的运行时配方。`HearthPublicVisuals.cpp` 已将完成构件绑定到 `/Game/ThreeHearths/Generated/PublicWallKit/{asset}/{asset}.{asset}`，但材料扣除、手部挂接、施工动作和储存逻辑仍以公共工程运行时代码及其引擎验收为准；本目录不把静态 Blender 预览当作运行时证据。

## 六米短墙装配

示例为沿 X 的三个 2 米墙段，中心 X=-2、0、2。每段安装一件基座、一件石墙、一件步道和前后各一件护栏，共 15 个独立组件实例、10,032 三角形。

| 顺序 | 安装位置 Z（米） | 依赖 |
| --- | ---: | --- |
| 石基座 | 0.000 | 地面定位 |
| 石墙段 | 0.280 | 对应基座已安装 |
| 木步道 | 2.677 | 对应石墙已安装 |
| 两侧护栏 | 2.897 | 对应步道已安装 |

护栏中心 Y=±0.38 米，顶部 Z=3.637 米，步道两护栏之间约有 0.62 米净宽。安装按阶段追加，保留之前的稳定组件 ID。全段输入草案为 **36 stone、21 planks、12 beams**。可先只完成一个 2 米段：12 stone、7 planks、4 beams，5 次逐件安装。

这是第一段可见公共城墙。楼梯、墙角、门洞、塔楼及步道通行连接未加入本组；预览中的 1.557 米标尺仅供角色尺度对照，不表示已验证 NPC 通行或登墙施工。

## 碰撞与验证

`catalog.json` 为四件各提供一个简单 Box 建议，均为作者坐标。墙与基座的 Box 覆盖装饰性石缝；步道用平坦 Box 连通板缝；护栏建议整体阻挡。它们尚未写入引擎，不能据此声称导航或碰撞已启用。

`validate_public_wall_kit.py` 独立读取 GLB 字节流，检查索引、非退化三角形、UV、单位法线、单一单位变换节点、尺寸、原点、资源元数据和 PBR。复用石墙的顶点位置、三角形朝向与 UV 逐三角形完全相同；其他引用资产的哈希保持不变。还检查了 15 个稳定 ID、安装依赖顺序、12 个支撑接触、材料总量与配方一致性。支撑间最大可见间隙为保留的 3 毫米石墙底缝。

`previews/PublicWall_components.png` 是四件独立构件总览，`previews/PublicWall_assembly.png` 是 6 米装配与材料包对照。它们是 Blender Cycles 实渲，CPU 3 线程、16 samples；不是 UE 运行截图。临时预览 `.blend` 和运行日志在 `Saved/ThreeHearths/PublicWallKit/`，源资产 `.blend` 与临时展示场景分开。

复查运行 `validate_public_wall_kit.py`；创建与预览脚本及 MCP 请求已随包保留。最后的 `artifact-manifest.json` 记录全部交付文件 SHA-256。
