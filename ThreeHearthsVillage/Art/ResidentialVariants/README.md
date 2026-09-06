# 住宅组件与材质接入清单

当前 VillageKit 的 40 个独立组件都有 UE 原生包，源 GLB 哈希与导入记录一致，尺寸检查通过。没有必要重做现有墙、框架或陶瓦。`component-map.json` 给出每件的完整原生路径、实测包围盒/原点、PBR、资源配方来源、当前绑定状态和下一步接入动作，可作为 Planner 注册的输入清单；它不会自动修改运行时。

## 现在可以使用的 7 件

以下已经同时出现在 `HearthStructureCatalog` 和居民 Planner 配方中。当前每次安装消耗下表的 1 份资源，来自实际 C++ 配方；旧 Art catalog 的多资源数量仅作为作者设计记录保存，不能当成当前运行时平衡数值。

| 组件 ID | 尺寸（米） | 当前资源 |
| --- | --- | --- |
| `foundation_stone_2m` | 2 × 2 × 0.24 | stone |
| `floor_timber_2m` | 2 × 2 × 0.16 | plank |
| `post_timber_2_4m` | 0.18 × 0.18 × 2.4 | beam |
| `beam_timber_2m` | 1.82 × 0.18 × 0.2 | beam |
| `wall_timber_2m` | 1.812 × 0.16 × 2.08 | plank |
| `wall_door_timber_2m` | 1.812 × 0.22 × 2.115 | plank |
| `roof_slope_timber_2m` | 2.2934 × 2 × 1.4141 | plank |

原生资产路径均为 `/Game/ThreeHearths/Generated/VillageKit/<ID>/<ID>.<ID>`；JSON 保存了从实际导入记录读取的完整路径。组件原点各不相同，例如地基向 Z=0 以下延伸、屋面以墙顶基准为原点；接入时保留实测 datum，不能统一改成底面中心。

## 优先补齐的组件

| 组件或变体 | 资产情况 | 距离真正可执行还缺什么 |
| --- | --- | --- |
| `wall_window_timber_2m` | 已导入，1.812 × 0.362 × 2.08 米；Adapter 已支持 plank | StructureCatalog 包围盒/支撑、Planner 选窗墙、窗台外凸间隙 |
| `roof_ridge_timber_2m` | 已导入，0.3 × 2 × 0.189 米；Adapter 已支持 plank | 屋面支撑注册、坡顶位置和接缝验证；资产原点不在瓦脊底面 |
| `gable_timber_4m` | 已导入，4.052 × 0.255 × 1.297 米；Adapter 已支持 plank | 按实际 4 米跨放置与验证，不能硬塞进任意 2 米房间 |
| 石材普通墙/门墙/窗墙/山墙 | 4 件均已导入；现有 stone 可用 | Catalog、Adapter、真实石料配方和支撑接触；普通石墙比木墙低约 1.2 厘米，需按测量值验证 |
| 灰泥普通墙/门墙/窗墙/山墙 | 4 件均已导入 | 先接 plaster 的库存/取得/搬运/扣款，再绑定组件和选择偏好 |
| 红陶/蓝灰坡顶、屋脊、雨棚 | 6 件均已导入、屋面高光审计通过 | 先接 tiles 的真实资源链，再注册支撑、成本和材料偏好；蓝灰为现有陶瓦色系变体，不能宣称另有石板资源 |
| `frame_timber_2m` | 完整开放框架已导入，2.18 × 2.192 × 2.4 米 | 明确一次框架安装的资源/施工依赖；与独立柱梁择一组装，避免重叠重复柱梁 |
| 门廊柱、石阶、栏杆 | 已导入 | 门口/道路锚点、台阶高差、碰撞通行、施工配方 |
| `door_oak`、`shutter_sage` | 已导入 | 独立附件锚点、开合空间/交互、归属与安装任务 |
| 楼梯和开口楼板 | 已导入 | 实际楼梯井、净高、上下楼寻路与层间支撑，视觉存在不等于可通行 |

同类别变体不是按材质槽序号随意替换：普通墙的木/灰泥/石版本实际边界最大相差约 4–6 毫米，门墙/窗墙也有差别。屋坡、屋脊和山墙各自的材质变体边界一致，但仍需绑定对应真实资源和支撑。JSON 的 `material_variant_groups` 给出成组映射和逐件差值；替换整件组件后按新边界重新验证。

## 画风与高光

材质沿用当前 VillageKit 调色板：栗木框架 roughness 0.48；木墙浅橡木/蜂蜜木板 0.43/0.46；奶油灰泥 0.57；灰蓝石墙约 0.47–0.57。重复的同名材质在这 40 件源文件中的 PBR 完全一致。金属五金保留原有金属参数，不把所有部件都强制成非金属。

陶瓦 4 档 roughness 为 0.34/0.38/0.36/0.40，木瓦为 0.48/0.51/0.53/0.50；木瓦反光更柔和。8 件坡顶/瓦脊/雨棚已与 `Art/RoofMaterialAudit/native-audit.json` 的源哈希逐项匹配，法线平滑比例约 46.6%–47.1%，PBR/Nanite 原生检查通过。UE 会重算法线和切线，未逐顶点比对最终法线；既有同光照 UE 图验证的是 Polished 陶瓦的可见改善。本轮不声称新拍了所有墙/框架的 GPU 材质对比图。

在当前 VillageKit 内，木质雨棚和另一种结构柱梁材质族还缺独立资产。石墙只是填充墙，不等于已经有石柱/石梁框架。新增这些件有明确价值；现有红/蓝瓦、木/石/灰泥墙无需重复生成。

## 最小执行单元与复验

NPC 拿走的是配方中的真实材料；一个墙板、框架或楼板可以作为现场安装任务单位，整层/整房是依赖图的组合结果。`carry_*` 仅是对应库存的携带外观，不把完整房屋当作手持物。屋面/墙材选择还需记录为居民偏好并经过预算和资源检查，清单不会自行宣称居民已经能选择全部变体。

仓库根运行源和导入溯源检查：

```powershell
python ThreeHearthsVillage/Plugins/ThreeHearths/Tools/audit_residential_variants.py
```

没有其他 UE 进程时，可执行只读原生检查（不启动 PIE、不保存资产，完成自动退出）：

```powershell
& 'D:/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe' 'D:/Dev/vibeGamingDemo1/ThreeHearthsVillage/CropoutSampleProject.uproject' /Engine/Maps/Entry '-ExecutePythonScript=D:/Dev/vibeGamingDemo1/ThreeHearthsVillage/Plugins/ThreeHearths/Tools/audit_residential_variants.py' -unattended -nop4 -nosplash -NoSound -HearthDisableApi -HearthNoWorldPersistence -HearthVariantsAuditQuit
```

该模式输出 `native-validation.json`，重新比较 40 份原生网格的全部 PBR、三角形、UV、尺寸与 Nanite 关闭状态。缺少该报告或报告失败时，不将源检查冒充成原生现场查询。此清单依据报告中记录的三份 C++ 源哈希生成；Planner 改动后重新运行以刷新绑定状态。
