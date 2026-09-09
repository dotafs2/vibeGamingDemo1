# Trade signs

`build_trade_signs.py` produces the three original local-XZ GLB modules and `trade_sign_manifest.json`. The town builder imports them with the existing town material palette; `--only-businesses` writes only the three business Assembly/Details pairs, recipes, a merged manifest, and `AincradBusinesses.blend`. It leaves ordinary houses, base modules, workbenches, and the cold forge outputs untouched. `--only-smithy` remains the separate smithy-only path.

Each business sign uses recipe key `trade_sign`. Its local wall bracket is X=-0.71m; the builder places the sign center at `(w*.28, d/2+.71, 2.35)` with Z rotation +90, putting the bracket on the source front wall y=+d/2. Ordinary houses retain the former blank support/plate sign.

Import with `Tools/import_aincrad_trade_signs.py` after the source GLBs exist. The importer uses existing `MI_Town_*` materials, disables and finishes asynchronous static-mesh compilation, checks source SHA256/bounds/materials, and writes `UE_Trade_Signs_Import_Report.json`. Assets are installed at flat loader paths `/Game/ThreeHearths/Generated/AincradTownKit/{module}/{module}` to match the existing recipe loader. Signs are static identification fixtures; they do not add shops, inventory, transactions, or resident behavior.

## 首轮画面状态（2026-09-08 21:17 UTC）

`v2_trade_signs_20260908_2117` 的 3 张 capture 均生成成功，但根目视拒绝：relief 法线朝内导致黑面，旅店招牌与阳台支撑相交。已准备的修正是源脚本对 relief 使用 bmesh 重算法线并闭合检查；town builder 仅将旅店安装 X 从 `w*.28` 调为 `w*.43` 以避开阳台，另外两家仍为 `w*.28`，Y/Z/90° 不变。重导入和复核尚未完成，不能宣称已修好。

## 修正后副本状态（2026-09-08 21:23 UTC）

`v2_trade_signs_normals_20260908_2123` 的 inn、smithy、carpentry 三张原始图已归档到 `Docs/Validation/Two_Hour_Iteration_2026-09-08/NativeTradeSigns/success/`。helper 对每个实例的数量、材质、世界变换和捕获均通过数值检查（`numeric_validation=true`），但保留 `visual_validation=false`；根已接受修正后的几何与牌面。阴影侧仍偏暗，属于本轮光照限制。该证据是 coordinator/local verification，不是 NPC 视野，也不证明居民已安装或看见招牌。首轮 21:17 失败图及法线/旅店阳台原因保留在 `NativeTradeSigns/failure/`，不与当前成功图的哈希混用。
