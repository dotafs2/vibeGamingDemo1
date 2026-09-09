# 用 MIT 代码与可分发素材搭建最小 AI 世界

日期：2026-09-09。目标：从现成工程与资源出发，做出能让玩家进入、让协作者完整复现的三渲二 AI 小世界。本轮交付为选型与实施大纲，未安装或运行候选组合。

同日设定校正：内容设计先以 [SAO 第一层资料基线](SAO_First_Floor_Canon_Baseline.md) 核查。木匠扩建酒馆是先前用于说明执行闭环的例子，不作为已确认的第一层首发故事。保留技术候选与三人规模，优先验证有原作依据的服务、任务和装备使用；NPC 自主建设属于明确标注的项目扩展。

## 决策

**针对这版强调 MIT 复用与 GitHub 协作的 MVP，优先验证 Godot + Kenney Starter Kit + OpenGameAgent + godot-vrm + CC0 场景/人物资源。**

上一份调研在“二次元人物工具链与后续 VR 优先”条件下倾向 Unity。本方案提高了源码可掌握、许可明确、基础样板可直接复用的权重，因此选择 Godot 做有界验证。它并不证明 Godot 在画面或人物工具链上全面优于 Unity，也不构成对旧工程的迁移操作。

我们自己的游戏代码可以采用 MIT；素材保留各自的 CC0 等许可，依赖保留原许可。这里不是把所有素材、引擎依赖、模型权重或云服务统称为 MIT。

## 1. 直接复用的最小组合

| 层 | 具体项目 | 已核实的许可与能力 | 采用方式 |
|---|---|---|---|
| 引擎 | [Godot](https://github.com/godotengine/godot) | 引擎 MIT | 桌面版起步；选用 .NET 版本接入下方运行时 |
| 可玩项目起点 | [Kenney Starter Kit 3D Platformer](https://github.com/KenneyNL/Starter-Kit-3D-Platformer) | README 明确代码 MIT、模型/图片/音效 CC0；提供移动、相机和手柄支持 | 复用移动与相机，把关卡换成街区；按生活游戏调整运动方式 |
| NPC 运行时 | [OpenGameAgent](https://github.com/EricSun0218/OpenGameAgent) | 顶层 LICENSE 为 MIT；公开角色调度、记忆、工具、图像输入和动作回执能力 | 固定版本，只接必要模块；先测真实动作、取消与重启恢复 |
| 动漫人物与材质 | [godot-vrm](https://github.com/V-Sekai/godot-vrm) | 代码/MToon 实现 MIT；附带样例人物有独立许可 | 只导入选定且可分发的人物，接动画/表情；不打包未知许可样例 |
| 日常执行与保存 | Godot 导航、动画与项目自己的世界状态 | 现有引擎组件可用；完整钱物与承诺系统需要开发 | 先做少量受限动作与可冷恢复的世界存档 |

第一人称备选是 [Kenney Starter Kit FPS](https://github.com/KenneyNL/Starter-Kit-FPS)，其 README 同样明确代码 MIT、素材 CC0；也可仅取 [FirstPersonStarter](https://github.com/Whimfoome/godot-FirstPersonStarter) 的 MIT 控制器。首个版本只选一种玩家控制方式，避免重复集成。

许可直接证据：[Kenney README](https://raw.githubusercontent.com/KenneyNL/Starter-Kit-3D-Platformer/main/README.md)、[OpenGameAgent LICENSE](https://raw.githubusercontent.com/EricSun0218/OpenGameAgent/main/LICENSE)、[godot-vrm LICENSE](https://raw.githubusercontent.com/V-Sekai/godot-vrm/master/LICENSE)。

### 为什么仍需一次接入验证

Kenney 当前模板标注 Godot 4.6，而 OpenGameAgent 当前文档的 Godot 适配验证目标为 4.7.1 .NET。不能据此宣称下载后直接拼起来就能运行。第一步应在选定的同一引擎版本里导入模板、导入一个角色、接一个动作并导出桌面运行包。[模板配置](https://raw.githubusercontent.com/KenneyNL/Starter-Kit-3D-Platformer/main/project.godot)、[运行时接入说明](https://github.com/EricSun0218/OpenGameAgent/blob/main/docs/engine-integration.md)。

OpenGameAgent 当前为 alpha。若只在运行时适配上失败，保留 Godot 世界与动作协议，替换这一层；不让角色 ID、产权或存档格式依赖某个 Agent 包。

## 2. 现成素材的具体来源

| 用途 | 可取得的资源入口 | 许可/版本注意点 | MVP 使用方式 |
|---|---|---|---|
| 城墙、街道建筑与小城轮廓 | [Kenney Fantasy Town Kit](https://kenney.nl/assets/fantasy-town-kit) | 官方标注 CC0、160 个文件；文件数量不等于独立建筑数量 | 选为主要建筑套件，组合 4–6 栋立面和一处公共空间 |
| 木构建筑与生活道具备选 | [Quaternius Medieval Village](https://quaternius.com/packs/medievalvillage.html) | 官方标注 CC0，含 FBX/OBJ/Blend | 与 Kenney 二选一作主要建筑语言；补充件须重新统一材质与比例 |
| 植物 | [Kenney Nature Kit](https://kenney.nl/assets/nature-kit) | 官方标注 CC0 | 树、灌木与地面植被形成两三组有层次的聚落 |
| 二次元基础人物 | VRoid 官方 [β Ver AvatarSample_1](https://vroid.pixiv.help/hc/en-us/articles/360012381793)、[β Ver AvatarSample_2](https://vroid.pixiv.help/hc/en-us/articles/360014900273) | 两个具体旧版样例官方明确 CC0；不推广到新版全部样例 | 从官方链接取得对应文件后核对版本与元数据，修改发型、配色、衣装成为独立居民 |
| 统一人形与服装的备选 | [Universal Base Characters](https://quaternius.com/packs/universalbasecharacters.html) + [Fantasy Outfits](https://quaternius.com/packs/modularcharacteroutfitsfantasy.html) | 官方页面标注 CC0；免费基础版、完整/Source 版内容不同 | 若 VRM 文件取得或动画适配阻断，可用于玩法验证；其外观不直接算最终 SAO 人物验收 |
| 动作素材 | [Universal Animation Library](https://quaternius.com/packs/universalanimationlibrary.html) | CC0；免费版、完整/Source 版区分，具体动画要核实际包 | 取站立、走路、坐下等必要动作；搬运、工作与人物骨骼适配仍需补做 |

本轮核查了资源页与许可说明，尚未下载这些资产包，不能保证宣传页列出的每一项都在免费包内。下载后以文件清单和所附许可确认。现代 VRoid 样例采用自己的条款，不能统一改标 CC0；见 [VRoid 官方样例说明](https://vroid.pixiv.help/hc/en-us/articles/4402614652569-Do-VRoid-Studio-s-sample-models-come-with-conditions-of-use)。

现成素材省去起步建模工作，但精致三渲二仍需要统一人物脸部、衣装、明暗、描边和场景色彩。第一版把精修集中在 3 名角色、一个店面和玩家走近的公共区域。

## 3. 首发只承诺一个小而完整的世界

**3 名永久 NPC + 玩家 + 一段街道 + 一件能引起协作或分歧的生活事件。**

- 3 人均有稳定身份、私人目标、关系、有限观察、记忆与可支配资源。
- 玩家能走近交流、给出建议、提出交易，也能被拒绝；NPC 能在玩家不发指令时继续已有计划。
- 空间建议 80–120 米，4–6 栋立面、一个可进入公共室内、一个可改变的院落。数字是 MVP 范围建议，不是原作测绘。
- 首个事件围绕有出处的服务或物品使用；自主改造空间后续按真实需求加入，并标记为项目扩展，核实钱物、位置和实际使用。
- 退出、重启后，人物、钱物、关系和未完成委托保留。
- 用第二个真实模型继续同一存档，观察未完成事项的接续。

世界机制由代码核实，角色从自己的摄像头与可感知事件中得到观察。画面中的物体身份、距离和可见性可以由感知层整理；不能直接把全世界数据库交给所有居民。

## 4. 先从原作收敛实际情境

原作第一层篇可直接核实 NPC 锻冶者、面包店及提供住宿的 NPC 农家；证据和具体地点见 [资料基线](SAO_First_Floor_Canon_Baseline.md)。因此候选题材改为“出城前补给—一次野外活动—返回处理物品—实际使用结果”。第一批只实现其中一笔完整交易与一次使用。

首批居民的姓名、私人动机和自主协商属于项目扩展；具体店铺若位于原文没有确认的起始之城街区，也应标为地点补全。不能将托尔巴纳的具体原作场景不加说明地移入起始之城。

测试布置物品、预算与适用规则，不预写交易一定成功。观看者应能看见居民选择、实际交付、系统核验、使用与记忆。武器强化等操作按游戏规则返回实际结果，不能让模型承诺必然成功。

先前木匠扩建酒馆的例子不作为首发故事定案。居民之后提出系统还没有的功能，仍需保存真实需求并完成“开发、测试、版本化安装、原档使用、第二人复用”，再扩展更多居民。构件组合与真正增加能力要分开验收。

## 5. 最少需要我们新增的四块代码

| 模块 | 最小职责 | 完成证据 |
|---|---|---|
| `world` | 身份、钱物、关系、委托、构件实例与版本；保存和恢复 | 同一个物品只有一个归属，重启不丢未完成事项 |
| `perception` | 本人摄像头、可见/可听事件、已经知道的事实 | 角色不会读到邻居未公开的秘密或隔墙物体 |
| `actions` | 将意图转成移动、协商、搬运、交易、制作、摆放和使用 | 只有实际完成才写成功回执；重试不重复扣款/生成 |
| `agent_bridge` | 每人上下文、调用调度、工具暴露、结果反馈和用量记录 | 真实模型能选择行动；不同人独立运行，冲突提交被处理 |

目录只表达边界，不必先做通用平台。动作系统需要覆盖少量真实对象，可以先以十个以内的动作接口起步。新的动作或配方通过注册进入可选能力，避免给某位角色编写专属分支。

## 6. 每一步的交付量

| 步骤 | 工作范围 | 验收 |
|---|---|---|
| 设定基线 | 核查第一层地域、角色类别、事件和物品规则 | 每个首发角色/活动有出处或明确补全标签；原作版本和地点不混用 |
| 0 | 在统一 Godot 版本里运行模板、一个 VRM 与一次真实模型动作 | 桌面导出包可运行；记录版本与用量；明确 alpha 适配结果 |
| 1 | 现成构件搭一段街道，精修一名角色与店面 | 玩家能走近，转头/行走/光照下脸部与材质稳定 |
| 2 | 加入 3 个身份、物品、委托与保存 | 冷启动后状态一致，错误重试不会重复完成交易 |
| 3 | 接本人观察、交流与几种动作 | 至少一次真实协商、接受/拒绝或改计划；结果可见 |
| 4 | 兑现事件中的服务或物品效果，再让角色观察并继续选择 | 钱物与结果一致，实际使用能看见，未完成事项持续保存 |
| 5 | 换模型接续、打包、在干净环境复现、录视频 | 他人按 README 能运行；样例/实测/回放标识明确 |
| 后续首项 | 从运行中选择一个真实能力缺口，开发安装 | 原存档实际使用，并由第二名居民复用 |

不以拍脑袋工期承诺替代实际验证。第 0 步通过后，才根据实际导入、动画与模型延迟估计剩余工作量。

## 7. 怎样让别人愿意一起做

对外的一句话：**一个由独立 AI 居民生活、记忆和改造的开源二次元小世界；更换模型，继续同一段人生。**

首发包需要同时提供：

1. 一个约 60–90 秒的实机片段，展示走近人物、协商、可见变化和保存后继续。
2. Windows 运行包与对应源码版本；运行包支持玩家自带 API 或本地端点，发布者不嵌入自己的密钥。
3. 明确标注的免密钥回放，让人先看画面和安装结果；真实 AI 模式单独可切换、可验证。
4. `README`、架构小图、资产来源、依赖版本、三名角色介绍、已知限制与运行成本记录。
5. 几个能独立完成的小任务：一个新动作、一种可使用道具、一段工作动画、一个模型适配器、一份信息边界回归案例。

接受贡献时要求动作模块附一个可运行例子，资源包含使用位置/碰撞/连接点与原始许可。贡献者可以改一个局部模块，不必理解整个社会模拟。

多人同时在线和多人协同开发是两件不同的工作；这个 MVP 先用 GitHub 分工与 PR 协作。玩家联网、VR、全城模拟和复杂资产生成随后按实际需求加入。

## 8. 为什么不直接把其他完整项目搬过来

- [AI Town](https://github.com/a16z-infra/ai-town)：MIT，很适合直接复用对话/观察设计，但改成 3D 仍需重做具身行动与场景。它不会省掉这个 MVP 最关键的物品和施工规则。
- [Mindcraft](https://github.com/mindcraft-bots/mindcraft)：MIT，在 Minecraft 里最快看到有效行为；转成二次元 SAO 视觉是另一项较大的工作。
- [The Mirror](https://github.com/mirror-engine/the-mirror)：公开仓库标为 V1 Classic，V2 另需访问；带协作编辑平台的方向有价值，但不应在最小生活 demo 阶段先承担整个平台集成。
- [GDQuest 第三人称模板](https://github.com/gdquest-demos/godot-4-3d-third-person-controller)：代码 MIT，当前 LICENSE 中美术为 CC-BY-NC-SA；不能将整套工程当成 MIT+CC0 素材包。可以单独评估代码复用。

## 本轮验证边界

已完成：核查上述主要仓库的 README/许可、具体 CC0 资源页、运行时接入版本差异，并收敛 MVP 工作范围。

未完成：模板与插件的同版本编译、素材下载与导入、真实模型验收、桌面包、GitHub 发布。因此这里的“可复用”指许可与代码接口具备复用条件，整套可运行性必须由第 0 步证明。
