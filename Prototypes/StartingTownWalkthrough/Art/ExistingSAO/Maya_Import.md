# 桐人导入 Maya · 2026-09-10

用户要求把另一任务已下载的桐人放进当前打开的 Maya，并说明完全不会使用 Maya。本轮已在用户原有 Maya 2026.2（PID 23272）里打开并保存 [Kirito_Start.ma](Maya/Kirito_Start.ma)，默认显示带贴图的自然站姿和完整全身。

## 保留与验证

- 使用同一作者原始 `sugaki_kirito/Kirito/キリト.pmx`，未修改原包或已有 Blender/Godot 文件。
- 31,083 顶点、49,600 三角面、200 骨骼、原始蒙皮权重、32 个顶点形变、39 材质。11 张实际使用的基础贴图另存到 `Maya/textures`，未缺图。高度 172 cm。
- [import_kirito_maya.py](import_kirito_maya.py) 使用现有独立 PMX 读取模块及 Maya 原生 API 构建可编辑网格、UV、法线、关节、SkinCluster、BlendShape；没有安装 Maya 插件。
- 已在独立转换进程验证弯肘与眨眼；从磁盘重新打开至用户 Maya 后再次验证：弯肘最大位移 18.9782 cm，眨眼最大位移 2.2207 cm，恢复姿势误差 0。骨骼只是默认隐藏，蒙皮继续正常工作。
- 已实际查看 [Maya 视口截图](Maya/maya_viewport.png)，确认全身入镜、材质可见。见 [实时验证](Maya/live_validation.json) 和 [转换验证](Maya/conversion_validation.json)。这属于模型导入验收，没有定稿整体项目美术风格。

## 初次使用

- 按住 **Alt + 鼠标左键拖动**：围绕人物转动视角。
- **滚轮**：拉近或拉远。
- 选中人物后按 **F**：镜头重新对准人物。
- 下次用 Maya 的 File → Open 打开 `Maya/Kirito_Start.ma`。贴图放在同一 Maya 目录内；当前贴图引用为本机绝对路径，搬动整个目录后需重新指定贴图目录。
- `Kirito_Expressions` 保留眨眼、口型、眉毛等英文别名，原名对照保存在转换验证 JSON。尚未制作新手用姿势控制器界面。

## 边界与修正

MMD 球面/Toon 专用效果、142 刚体/374 约束、MMD IK 求解和 4 个材质形变没有转换。当前是 Maya 基础材质显示，衣摆/头发物理与动画控制器尚未接通。

第一次 Maya ASCII 保存把日文备注写成系统代码页，实时打开出现语法错误。已将 Maya 文件中的备注改为 ASCII 码点记录，原始日文对照保留在 UTF-8 JSON；修正后的文件已成功重开、验证并保存。第一次失败文件与错误记录留在被忽略的 Maya 目录用于追溯。

本轮仅启动了转换用 mayapy PID 16588，已退出且无该 PID 的直接子进程；用户原有 Maya 保持打开。没有启动收费 API、居民模拟、其他代理或新任务，没有提交或上传。`Maya/` 已加入本地 `.gitignore`，避免提交作者原始/衍生模型。

## 后续：两个已有姿势与过渡演示

用户随后要求展示已有两个姿势，并询问能否 AI 插帧。已核对 `../../kirito_actor.gd`：A 是自然站姿；B 是此前工程中的检查姿势，右肘弯曲 -0.8 弧度、眨眼权重 1、口型「あ」权重 0.65。它们属于项目已有状态，不能说成原作者附带两段动画。

- 演示另存为 [Kirito_Two_Poses.ma](Maya/Kirito_Two_Poses.ma)，最初的 `Kirito_Start.ma` 保留。
- 默认并排显示 A/B 的网格快照；原有绑定人物保留于场景中，点击过渡按钮后显示。
- [show_kirito_poses_maya.py](show_kirito_poses_maya.py) 提供“并排看 A 和 B”“只看 A”“只看 B”“播放过渡：A → B → A”“停止播放”。按钮窗口属于当前 Maya 会话；重新打开场景后，可在 Maya 内再次执行该脚本恢复按钮。
- 过渡使用 Maya 原生 plateau 关键帧曲线：1/13 帧 A，49/61 帧 B，97/109 帧回 A，24 fps 单次播放。此演示没有调用 MotionMaker 或其他 AI，不能称为 AI 生成结果。
- 已实测两份快照与原始绑定网格的顶点误差均为 0；过渡中间帧形变权重连续，端点一致。实际点击播放按钮后采样帧 9、30、61、109；109 帧已停止，最终恢复并排视图。见 [姿势验证](Maya/two_poses_validation.json) 和 [实际视口截图](Maya/two_poses_comparison.png)。
- 比较网格继承了绑定网格的变换锁，第一次移动失败；已仅解锁比较副本的 X 平移并恢复构建，失败记录保留。

Maya 原生关键帧可自动补间；[MotionMaker 官方工作流](https://help.autodesk.com/cloudhelp/2026/ENU/Maya-MotionMaker/files/GUID-473B9D09-124C-4A5C-94F8-F3B4B89FAB18.html)是路线/动作条件生成及 HumanIK 重定向。尚未验证此版本以任意两个人工姿势为端点的 AI 动作补全，桐人也尚未完成 HumanIK 定义。骨骼和蒙皮已实际可用，用户无需重新从零绑定。

此后续没有启动新 Maya/助手进程或收费调用；播放已停止，用户 Maya 与姿势按钮保持可用。没有提交或上传。
