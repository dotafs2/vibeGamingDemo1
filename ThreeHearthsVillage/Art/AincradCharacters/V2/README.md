# 起始之城人物样板 V2

三位原创居民的外观样板，由 Astra 在 Blender 中制作。角色身份、故事和财产仍来自既有世界；模型替换不创建新居民，也不改变他们的选择。

| 样板 | 对应居民 | 身高 | 特征 |
|---|---|---|---|
| Aileen | 艾琳／旅店 | 173.36 cm | 赭色低马尾、绿裙与浅色围裙 |
| Takuma | 拓真／铁匠 | 182.69 cm | 深色短发、较宽下颌、蓝衣皮围裙 |
| Kashiwagi | 柏木／木工 | 176.00 cm | 浅棕分束短发、苔绿衣、工具腰包 |

源文件为 `AincradCharacters.blend`；生成脚本在上级 `build_characters_v2.py`。`Exports` 保留三个人物 FBX 和每人 Idle/Walk 两段动画。每套使用 18 骨骼，连续袖管和裤腿在关节处混合权重，头发和脸部细节随头骨运动。

每人分为 12 个材质语义槽。服装、皮革、靴子、头发、眼睛可各自换色，后续同骨架服装组件可复用；目前没有接入染衣、换装、面部表情或生产这些衣物的玩法。围裙和发束仍是样板阶段，不能把材质槽数量算成新增人物数量。

`manifest.json` 记录源几何、骨骼、颜色与文件。`UE_Import_Report.json` 记录一次导入结果；必须再检查独立编辑器冷加载和 `Previews/run_*/UE_Preview_Report.json` 中的实际结果，才证明绑定持久有效。仅进程退出0或生成报告文件不表示图片验收通过。首次冷加载已发现材质为空，修复和原始失败保存在夜间验证记录中。

UE导入使用 `Tools/import_aincrad_characters_v2.py`，目标限定在 `/Game/ThreeHearths/Generated/AincradCharactersV2`。原生预览使用 `Tools/preview_aincrad_characters_v2.py`，在未保存临时世界中拍摄固定时间的Idle/Walk、X/Y两个方向。Blender源预览与UE原始截图分别保存；朝向要看真实图后确认。

游戏接入必须先在禁用API的独立世界副本检查相对朝向、脚底、动画与原有胶囊通行，再让同一世界继续。不会用新模型替换NPC身份，也不会把协调者预览作为NPC自己的眼部观察。
## 当前画面验收状态（2026-09-08）

`Saved/ThreeHearths/AincradLevel0/CharacterReview/characters_v2_face_surface_20260908_1920/` 的 15 张原始 PNG 已归档到 `Docs/Validation/Two_Hour_Iteration_2026-09-08/NativeCharacters/face-surface-before-render-sync/`，并附相对路径 `index.json`、逐图 SHA256 与当前 `UE_Import_Report.json` 副本。CPU pose delta、材质槽和骨骼结构检查通过，但肉眼看到 Walk PNG 仍像 Idle；这不能证明渲染后的 Walk 姿态或自动播放已经同步。因此 `rendered_walk_pose_validated=false`、`automatic_animation_playback_validated=false`，V2 仍为 `pending render-sync validation`，未正式启用。历史 `garments-before-face` 证据保留，其 `passed` 只表示当时 helper 的结构检查结果。
## 2026-09-08 19:57 UTC 验收与接入

三人的手动 Walk 姿态已在原始引擎 PNG 中确认，眼部表面、服装形状及 12 个材质槽保留。此前同一帧截图仍显示旧姿态的失败保留在验证目录中；评审工具现在显式提交当前骨骼渲染数据。随后自然播放检查跨 6 个游戏帧、约 0.2 秒，三人动画时间各推进 0.2 秒，手脚最大位移约 27.8／29.3／28.2 cm，Actor 和胶囊位置不变。数字检查不等于艺术定稿。

当前代码将这三位既有居民默认使用 V2，朝向 -90 度；缺失资源仍保留旧外观，也可用 `-AincradLegacyAppearance` 回退。正式新批次的冷启动仍需核对，不据此宣称新增人物、装备交易或自动完成工作。原生副本工具为 `Tools/review_aincrad_characters_v2.py --stop-at-utc <已授权截止>`；编辑器临时预览保留为历史失败，不再作为主要动画验收入口。

可移植证据：`Docs/Validation/Two_Hour_Iteration_2026-09-08/NativeCharacters/face-render-sync/` 与 `natural-playback/`。

## 自然播放证据

UE_Animation_Data_Report.json 是旧阶段诊断，只说明导入/动画数据层检查，不能替代当前自然播放证据。当前自然播放记录归档于 Docs/Validation/Two_Hour_Iteration_2026-09-08/NativeCharacters/natural-playback/index.json；人物 V2 正式状态仍以代码构建、独立副本和连续正式 run 门控为准。
