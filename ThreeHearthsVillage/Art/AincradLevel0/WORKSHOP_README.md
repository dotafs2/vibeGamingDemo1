# Workshop Kit（AincradLevel0）

本套为 `aincrad_anime_town_v1` 的静态美术构件。原生单位为 Blender 米；UE 导入换算为厘米，坐标为 `(x,-y,z)*100`，原生立面朝 `-Y`。

| 资产 ID | 用途 | 原生尺寸（UE cm，X×Y×Z） |
|---|---|---:|
| `carpentry_workbench` | 替换现有木工工作台 | 180×105.5×101.9438 |
| `smithy_workbench` | 替换现有铁匠工作台 | 180×81.9125×123.9938 |
| `bench_vise` | 可复用木工台虎钳 | 32×40.1×23 |
| `hand_plane` | 可复用手刨 | 27×9.3×13.25 |
| `bench_anvil` | 可复用安装式铁砧 | 79.5×25×33 |
| `smith_hand_tools` | 可复用锤与钳 | 38.75×22.8318×7 |

## 源文件与导入顺序

1. 修改或重跑 `build_workshop_kit.py`（Blender；脚本会生成 `Modules/*.glb`、清单，并把两份 Recipes 的 `working_table` 指向对应工作台）。
2. 若城镇基础 kit 有更新，先重跑 `build_town_kit.py`，再重跑本脚本。
3. 在 UE 中运行 `Tools/import_aincrad_workshop_kit.py`，导入上述 GLB；按 `UE_Workshop_Kit_Import_Report.json` 核对尺寸和原生碰撞设置。

## 范围限制

这是静态美术替换与可复用摆件：不新增库存、财富、技能、动作、维修动画或居民强制选择；不能记作居民自行施工，也不会创建模拟库存或合约。
