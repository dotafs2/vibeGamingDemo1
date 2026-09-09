# Axe kit

`axe_kit_manifest.json` defines four independently importable static parts that share one tool origin: a sound/split handle and a sharp/chipped head. Together they are the existing axe appearance; the kit does not create an item, repair completion, inventory, material stock, skill, ownership, currency, or capability.

Use `Tools/import_aincrad_axe_kit.py` in an owned Unreal Editor Python session after the four source GLBs exist. It reuses the existing `MI_Town_*` material palette, disables asynchronous static-mesh compilation and finishes pending compilation, verifies each source SHA256/material set/bounds, and writes `UE_Axe_Kit_Import_Report.json` to the art directory. Assets use flat loader paths `/Game/ThreeHearths/Generated/AincradTownKit/{name}/{name}`.

The runtime appearance mapping remains an explanation of existing state: `edge>=100` selects `axe_head_sharp`, otherwise `axe_head_chipped`; `handle>=100` selects `axe_handle_sound`, otherwise `axe_handle_split`. The visual variants do not change the underlying axe condition or grant wood. Existing use rules still require edge=100, handle=100, owner and custodian to be the resident, a registered work station, and at least one wood; use consumes one wood and produces one kindling.

## 本地副本验证（2026-09-08 22:05 UTC）

`v2_axe_continuous_20260908_2205` 在同一既有斧 `edge=20`、`handle=20` 的状态下捕获到两个实际部件：`axe_handle_split` 与 `axe_head_chipped`，工具 actor 变换保持不变。根接受几何与状态显示；背光侧仍过暗，因此不宣称全部材质/光照画面验收通过，也不宣称 NPC 已看见、修理已完成或库存改变。完整证据在 `Docs/Validation/Two_Hour_Iteration_2026-09-08/NativeAxe/`。

首轮 `v2_axe_components_20260908_2201` 没有产生 axe PNG，`tagged_actor_count=0`；首次导入日志因 `AT_AT_` 材质角色名匹配失败，位置只在 NativeAxe 失败 metadata 中记录，未复制完整日志。

## HeldToolView 观察证据（2026-09-08 22:41–22:45 UTC）

`Docs/Validation/Two_Hour_Iteration_2026-09-08/NativeHeldToolView/` 保存两轮同一副本的工具观察证据。首轮 `held_tool_view_grade_20260908_2241` 中 Erin 的 obs56 为水平视线、obs57 低头、obs58 完成后的 fresh capture；工具仍是 edge/handle=20 的 split/chipped，但首轮刀刃近 edge-on，根拒绝可读性。最终 `held_tool_inspection_pose_20260908_2245` 的工具相对携带姿态 yaw offset 约 +110°，实际工具 yaw 为 163.1063°；工具 pivot 保持但工具 rotation 确实改变，Erin 的 eye/body facing 保持，obs57/obs58 camera orientation 相同。刀面缺口基本可辨认，根接受基本观察可用。暗材质与缺完整抬手动画仍是限制。两轮均来自 NPC 相同眼位的本地强制测试，未作为真实 Kimi 自主请求提交，也不是修理完成。

同机位 `axe_context_legacy_exposure` 与 `axe_context` 的 RGB MAE 约 `0.00007/255`；两者继承场景已有 unbound Grade，显式 Grade 几乎没有视觉变化，因此曝光差异不是暗面的根因。context helper 已取消 temporary Fill；仅 isolated character pose 保留独立补光，旧 context 的临时补光不能代表未经辅助的光照。


## SteelEye 原生可读性证据（2026-09-08 23:20 UTC）

`NativeAxeSteelEye/` 归档了 revision 2 manifest、当前 UE 导入回执、新 `IronLight`/`SteelEdge` 材质角色与源 preview，以及同一副本的 Erin obs59/60/61。两把旧 handle 的 SHA 未变；两个 head 通过 Blender 闭合面检查并含真实柄孔，UE 回执匹配新材质。根接受基本原生可读性。obs60/61 的工具 inspection yaw offset 为 +110°，本轮实际工具 yaw 为 `-136.8937°`；工具 pivot 保持但 rotation 改变。当前 body yaw 为 113.1°，与 2245 轮的 53.1° 不同，因此不能作跨版本严格同机位亮度 A/B。悬浮携带位置、完整手指抓握和抬手动画仍未完成，暗面仍受限。

首次 Boolean 产生空材质槽的失败原因和私有日志位置保留在归档索引，未复制巨型日志。证据来自 NPC 相同眼位的本地强制测试，未作为真实 Kimi 自主请求提交；不代表修理完成或居民已看见。
