# AI 居民与 SAO 世界：59 项 GitHub 候选目录

核查日期：2026-09-09。配套阅读：[路线与 demo 大纲](README.md)。

这里的“采用建议”是针对本项目的判断；仓库存在、README 宣称支持和本机已跑通是不同证据。本轮检查官方文档、公开仓库及部分代码，**没有执行候选项目**。代码许可列依据所读顶层许可/项目说明；依赖、示例资产、模型权重与云服务可能使用不同条款。没有找到许可文件的项目不默认视为可自由复制。

采用级别：**优先试验**＝下一轮验证；**按需组件**＝缺口出现时加入；**借鉴**＝参考设计或局部实现；**观察**＝复现/成熟度/许可尚有不确定；**暂缓**＝当前 demo 不需要。

## A. 自主居民、社会与现成小世界（10 项）

| GitHub 项目 | 已公开的主要用途 | 对我们的价值与缺口 | 代码许可 / 建议 |
|---|---|---|---|
| [AI Town](https://github.com/a16z-infra/ai-town) | 网页像素小镇，角色移动、对话、记忆；Convex 后端 | 观察入口和异步世界可借鉴；部分活动由规则/随机决定，不能当完整自主生产社会 | MIT / 借鉴 |
| [Generative Agents](https://github.com/joonspk-research/generative_agents) | Smallville 研究复现；记忆、反思、计划 | 很适合理解居民认知结构；移植到真实游戏仍需自己的行动、存档与感知层 | Apache-2.0 / 借鉴 |
| [Concordia](https://github.com/google-deepmind/concordia) | 组件化生成式社会模拟，参与者与 GM 情境裁决 | 借角色组件和社会推演；GM 的叙述不能代替游戏实际扣款、交付与建造 | Apache-2.0 / 借鉴 |
| [AgentSociety](https://github.com/tsinghua-fib-lab/AgentSociety) | 社会模拟/实验；当前仓库含 AgentSociety2 与旧包 | 可参考社会实验；不可把旧版城市演示当成当前主线即插即用功能 | Apache-2.0，commercial 子目录例外 / 借鉴 |
| [OASIS](https://github.com/camel-ai/oasis) | Twitter/Reddit 一类社交平台的多 Agent 模拟 | 可参考传播与群体行为；百万账户宣传不能等价为百万具身居民 | Apache-2.0 / 暂缓 |
| [Mindcraft](https://github.com/mindcraft-bots/mindcraft) | 多模型、多机器人接入 Minecraft，使用现有游戏能力 | 行为原型最直接的候选；接入 SAO 人形与非方块城市需要另一路视觉工程 | MIT / 行为实验优先试验 |
| [Voyager](https://github.com/MineDojo/Voyager) | Minecraft 自动课程、反馈修正与技能库 | 借技能积累和反馈；学习既有能力的组合，不等于自动新增底层机制 | MIT / 借鉴 |
| [Project Sid](https://github.com/altera-al/project-sid) | 大规模 AI 社会研究展示与材料 | 方向很相关；公开仓库不是可直接运行的完整 PIANO 社会系统 | 未见可复用完整运行系统许可 / 研究参考 |
| [CUBE](https://github.com/echo-yiyiyi/cube) | Unity 环境中的日程、活动与相遇交流；Python/CAMEL | 比纯聊天演示更接近具身生活；场景仍需配置，经济、施工和长期迁移需补 | 顶层许可未核实 / 观察、借鉴 |
| [Agentshire](https://github.com/Agentshire/Agentshire) | 把 OpenClaw/QClaw Agent 可视化为 3D 镇民；地图/角色工具 | 展示与编辑入口有启发；默认生活为算法驱动，LLM Soul Mode 需开启，外部宿主版本有约束 | MIT；第三方资产另列 / 借鉴 |

关键代码核查：AI Town 的 [活动实现](https://github.com/a16z-infra/ai-town/blob/main/convex/aiTown/agentOperations.ts)与[角色状态](https://github.com/a16z-infra/ai-town/blob/main/convex/aiTown/agent.ts)；CUBE 的 [Unity 接入与运行说明](https://github.com/echo-yiyiyi/cube/blob/main/README.md)。

## B. 游戏内 NPC 运行时（5 项）

| GitHub 项目 | 已公开的主要用途 | 对我们的价值与缺口 | 代码许可 / 建议 |
|---|---|---|---|
| [OpenGameAgent](https://github.com/EricSun0218/OpenGameAgent) | C# Agent 内核、角色并发、记忆、工具结果与引擎适配 | 本次最贴近需求的运行时候选；当前 alpha，必须验证打包、持久化及动作恢复；游戏规则仍由项目实现 | MIT / **优先试验** |
| [AgentArena](https://github.com/JustInternetAI/AgentArena) | Godot C++ + Python，工具、记忆、目标与小场景评测 | 适合 Godot 行为实验；多语言构建和基准场景到完整游戏仍有距离 | Apache-2.0 / 观察 |
| [LLMUnity](https://github.com/undreamai/LLMUnity) | Unity 中本地/远端推理、RAG、聊天记录与函数调用示例 | 本地模型接入候选；这些组件不会自动组成永久社会或生产系统 | Apache-2.0；权重另查 / 按需组件 |
| [Llama-Unreal](https://github.com/getnamo/Llama-Unreal) | llama.cpp 的 UE 集成，组件/子系统与推理接口 | 选择 UE 且需要本地模型时使用；不替代 Agent 调度和世界规则 | MIT；依赖/权重另查 / UE 按需组件 |
| [LLM-NPC-Agents](https://github.com/lschiweck/LLM-NPC-Agents) | Unity/VR 角色实时语音、NPC 互聊与叙事管理 | 近距离多角色交谈可参考；导演驱动故事与居民自主生产是不同能力 | Apache-2.0 / 语音方案备选 |

OpenGameAgent 的 [架构边界](https://github.com/EricSun0218/OpenGameAgent/blob/main/docs/architecture.md)明确游戏掌握最终状态变化；它适合减少运行时工程，而非替我们定义整个世界。

## C. 具身模拟与学习环境（4 项）

| GitHub 项目 | 已公开的主要用途 | 对我们的价值与缺口 | 代码许可 / 建议 |
|---|---|---|---|
| [SimWorld](https://github.com/SimWorld-AI/SimWorld) | UE 环境、Python 接口、多种传感器与可交互地图 | 世界/观察桥接可参考；基础环境二进制、硬件和资产依赖较重，内容偏现代环境 | Apache-2.0；资产分开核查 / UE 备选 |
| [SimWorld Studio](https://github.com/SimWorld-AI/SimWorld-Studio) | 编码 Agent 构建环境，结合规则/视觉验证和技能迭代 | 最接近“自动改世界”的研究路线；索引可读但后续直连失败，不能作为已可复现的首发依赖 | 项目索引说明 Apache-2.0；当前取源码待复核 / 观察 |
| [UnrealCV](https://github.com/unrealcv/unrealcv) | 外部程序获取 UE 图像、对象信息与控制接口 | 构建 NPC 观察/评测工具的基础；需自行限定本人视野，不能直接给全场景真相 | MIT / UE 按需组件 |
| [ML-Agents](https://github.com/Unity-Technologies/ml-agents) | Unity 强化学习与模仿学习环境 | 后续训练移动、对抗或动作技能可用；居民身份/语言/长期故事不会自动生成 | Apache-2.0 / 暂缓训练 |

SimWorld Studio 的[官方项目页](https://simworld.org/simworld-studio/)是本轮仍可读取的主要来源。搜索索引中的说明还涉及外部 UE 项目源码以及未全部随开源版本发布的演示资产；在拿到可复现构建前，不承诺复制视频效果。

## D. 引擎、地形与 AI 制作工具（7 项）

| GitHub 项目 | 已公开的主要用途 | 对我们的价值与缺口 | 代码许可 / 建议 |
|---|---|---|---|
| [Godot](https://github.com/godotengine/godot) | 完整开源 2D/3D 引擎 | 若完全开源引擎为硬要求，优先路线；具体二次元内容仍需制作 | MIT / 引擎备选 |
| [Three.js](https://github.com/mrdoob/three.js) | 浏览器 3D 渲染库 | 网页角色预览、观察器；不是完整社会模拟和游戏编辑器 | MIT / 网页按需组件 |
| [Terrain3D](https://github.com/TokisanGames/Terrain3D) | Godot 4 可编辑地形系统 | Godot 城外与地形制作候选；不自动决定城市道路、用地和建设权 | MIT / Godot 按需组件 |
| [unity-mcp](https://github.com/CoplayDev/unity-mcp) | 编码 Agent 通过 MCP 操作 Unity 编辑器 | 场景和资产制作助力；编辑器操作不会自动变成玩家版本中的 NPC 能力 | MIT / 制作工具优先试验 |
| [unreal-mcp](https://github.com/chongdashu/unreal-mcp) | 通过 MCP 控制 UE 编辑器 | UE 制作桥接的参考；本轮未核实顶层许可证，复用代码前需补核 | 许可待核 / 观察 |
| [blender-mcp](https://github.com/ahujasid/blender-mcp) | AI 助手操作 Blender 的桥接 | 做构件、修改场景和导出；连接工具不提供美术质量保证，外接服务另计 | MIT / 制作工具优先试验 |
| [Infinigen](https://github.com/princeton-vl/infinigen) | Blender 程序化自然/室内环境与资产生成 | 植物、地貌和规则化生成参考；需要简化、风格化和实时资产处理 | BSD 系许可及部分 CC0 代码 / 按需组件 |

UE 的 [PCG 框架](https://dev.epicgames.com/documentation/en-us/unreal-engine/procedural-content-generation-framework-in-unreal-engine)也应纳入世界制作候选，但它属于引擎功能，未计入这 59 个 GitHub 候选。PCG 可生成/摆放内容，NPC 的需求、土地权和施工结算需另行定义。

## E. 二次元人物、渲染与模型导入（8 项）

| GitHub 项目 | 已公开的主要用途 | 对我们的价值与缺口 | 代码许可 / 建议 |
|---|---|---|---|
| [UniVRM](https://github.com/vrm-c/UniVRM) | Unity VRM/glTF 导入导出、VRM 动画支持 | 二次元人物主候选；支持运行时异步导入，但仍需动画/材质版本验证 | MIT；人物许可独立 / **优先试验** |
| [VRM4U](https://github.com/ruyo/VRM4U) | UE VRM 导入、卡通材质、骨骼/表情相关支持 | UE 角色主候选；锁定引擎、插件与目标平台验证 | MIT；依赖/人物许可独立 / UE 优先试验 |
| [godot-vrm](https://github.com/V-Sekai/godot-vrm) | Godot VRM 与 MToon/角色相关导入 | Godot 二次元角色候选；具体渲染、弹簧骨和动画集成需验证 | MIT / Godot 优先试验 |
| [three-vrm](https://github.com/pixiv/three-vrm) | 浏览器 Three.js VRM 实现 | 人物预览、轻量网页体验；完整游戏系统需另做 | MIT / 网页按需组件 |
| [Unity Toon Shader](https://github.com/Unity-Technologies/com.unity.toonshader) | 面向动画风格的材质与多渲染管线支持 | 场景/角色渲染参考；与 MToon 分工，统一光照，避免材质风格拼贴 | Unity Companion；样例角色另有许可 / **优先试验** |
| [VRM Add-on for Blender](https://github.com/saturday06/VRM-Addon-for-Blender) | VRM 编辑、导入导出与 Python 自动化接口 | 修改服装、头发、材质和角色导出；不凭空提供一套优质角色 | MIT/GPL 双选结构，按 LICENSE_MAIN 核对 / **优先试验** |
| [glTFast](https://github.com/atteneder/glTFast) | Unity glTF 高效导入/导出；当前包名为 com.unity.cloud.gltfast | 运行时载入新道具候选；打包时需保留相应 shader/变体 | Apache-2.0 / 按需组件 |
| [glTFRuntime](https://github.com/rdeioris/glTFRuntime) | UE 运行时 glTF 导入 | UE 中接收生成资产；材质、碰撞、使用语义仍须项目处理 | MIT / UE 按需组件 |

VRM Blender 插件的许可见 [LICENSE_MAIN](https://github.com/saturday06/VRM-Addon-for-Blender/blob/main/LICENSE_MAIN.txt)。人物文件、贴图、衣服的使用权由资产自己的许可决定，不能由导入插件的 MIT 许可推导。

## F. 开放 3D 生成项目（3 项）

| GitHub 项目 | 已公开的主要用途 | 对我们的价值与缺口 | 代码许可 / 建议 |
|---|---|---|---|
| [TRELLIS.2](https://github.com/microsoft/TRELLIS.2) | 图像到 3D 与材质，输出 GLB | 可试特殊道具；官方说明主要在 Linux 测试，需至少 24GB NVIDIA 显存；不是首发轻依赖 | MIT；依赖/权重另核 / 后续试验 |
| [Hunyuan3D-2.1](https://github.com/Tencent-Hunyuan/Hunyuan3D-2.1) | 几何与纹理生成 | 生成候选资产；README 标注几何/纹理合计约 29GB 显存需求；需资产后处理 | Tencent Hunyuan Community / 按许可与算力再选 |
| [TripoSR](https://github.com/VAST-AI-Research/TripoSR) | 单图快速重建 3D | 小规模技术验证候选；成品风格、背面、UV、拓扑与碰撞需检查 | MIT；README 默认单图约 6GB 显存 / 按需试验 |

上述显存数字来自作者说明，并非本机测得；模式、参数和优化版本会影响实际需求。[Hunyuan 许可](https://github.com/Tencent-Hunyuan/Hunyuan3D-2.1/blob/main/LICENSE)包含适用范围限制，应与 MIT/Apache 项目区分。

对首发的选择：人物用可精修的 VRM 流程；建筑用统一模块；少量特殊物件可用生成模型起稿。生成一个外观网格后，还要定义它能否坐、存放、生产、损坏以及怎样进入存档。

## G. 模型接入、通用 Agent 与记忆（6 项）

| GitHub 项目 | 已公开的主要用途 | 对我们的价值与缺口 | 代码许可 / 建议 |
|---|---|---|---|
| [elizaOS](https://github.com/elizaOS/eliza) | 通用 Agent、插件、消息/记忆与平台接入 | 可借人格/插件组织；社交连接器不是具身游戏的必要组成 | MIT / 借鉴 |
| [Mem0](https://github.com/mem0ai/mem0) | 记忆抽取、管理与检索 | 人物记忆组件候选；抽取内容可能有误，不能当产权与钱物真相源 | Apache-2.0；托管服务另计 / 按需组件 |
| [Graphiti](https://github.com/getzep/graphiti) | 时间化知识图谱、实体与关系检索 | 复杂关系历史可用；少量居民第一版通常不必先上图数据库 | 本轮读取的顶层 LICENSE 为 Apache-2.0 / 按需组件 |
| [LangGraph](https://github.com/langchain-ai/langgraph) | 带持久执行/状态的 Agent 工作流 | 后台需求审核/开发流程候选；与游戏 Agent 运行时有部分职责重叠 | MIT；托管产品独立 / 后续按需 |
| [LiteLLM](https://github.com/BerriAI/litellm) | 多模型接口、代理与用量管理 | 多提供商/统一网关需要时使用；适配 API 不会自动保留人格与世界语义 | 核心 MIT，企业功能例外 / 按需组件 |
| [llama.cpp](https://github.com/ggml-org/llama.cpp) | 本地模型推理与服务接口 | 降低离线演示对云依赖；模型能力、硬件占用和延迟仍须测 | MIT；权重独立 / 后续本地模型候选 |

记忆的建议起点是项目自己保存事件、承诺与人物摘要，再按实际检索问题引入专门库。不能同时把多个库当作世界事实来源。

## H. 行动执行、规划与导航（4 项）

| GitHub 项目 | 已公开的主要用途 | 对我们的价值与缺口 | 代码许可 / 建议 |
|---|---|---|---|
| [CrashKonijn GOAP](https://github.com/crashkonijn/GOAP) | Unity 目标导向行动规划 | 将可执行动作按前置条件组合；人格与目标来源仍由我们决定 | Apache-2.0 / 动作变复杂后引入 |
| [ReGoap](https://github.com/luxkun/ReGoap) | C# GOAP 实现 | 规划方式和动作表示参考；具体引擎集成按当前分支验证 | Apache-2.0 / 借鉴或 GOAP 备选 |
| [LimboAI](https://github.com/limbonaut/limboai) | Godot 行为树与状态机 | 可靠执行走路、工作、交互；没有语言模型记忆/人格这一层 | MIT / Godot 按需组件 |
| [Recast/Detour](https://github.com/recastnavigation/recastnavigation) | 导航网格、寻路与群体移动基础 | 理解/扩展导航；使用现成引擎时先用引擎提供的导航接口 | zlib / 底层参考 |

模型负责选择有意义的目标与应对变化；动作规划和状态机让目标逐步执行。每一步走路都调用大模型会增加延迟与成本，也不会自动增加自由度。

## I. 多人、服务器与 VR（6 项）

| GitHub 项目 | 已公开的主要用途 | 对我们的价值与缺口 | 代码许可 / 建议 |
|---|---|---|---|
| [Nakama](https://github.com/heroiclabs/nakama) | 用户、存储、社交、多人服务端能力 | 真正开放多人服务时评估；不是现成的 SAO 经济与物理服务器 | Apache-2.0 / 后续多人候选 |
| [Mirror](https://github.com/MirrorNetworking/Mirror) | Unity 游戏网络同步 | 第二名玩家进入同一个世界时可用；永久世界规则仍由项目实现 | MIT / Unity 多人候选 |
| [Colyseus](https://github.com/colyseus/colyseus) | 游戏房间与服务端状态同步 | 网页体验或跨端小型多人候选；不是完整 MMORPG 底座 | MIT / Web 多人候选 |
| [OpenXR SDK Source](https://github.com/KhronosGroup/OpenXR-SDK-Source) | XR 标准相关 SDK/loader 等源码 | 跨头显标准层；通常使用引擎 OpenXR 插件，不需自己从 SDK 重造接入 | Apache-2.0 等，文档独立标记 / 标准参考 |
| [XR Interaction Toolkit Examples](https://github.com/Unity-Technologies/XR-Interaction-Toolkit-Examples) | Unity XR 交互示例 | 抓取、移动等交互起点；舒适性、性能与 NPC 交互需实机测试 | Unity Companion / PCVR 优先试验 |
| [Godot XR Tools](https://github.com/GodotVR/godot-xr-tools) | Godot XR 常用交互场景/工具 | Godot VR 路线的组件；不等于已完成 SAO 游戏 | MIT，素材另核 / Godot XR 候选 |

不要把可扩展服务器框架的存在当成已经支持无限地图、无限在线用户或无限居民。先验证一个共享世界，再按实际负载拆分服务。

## J. 语音与口型（3 项）

| GitHub 项目 | 已公开的主要用途 | 对我们的价值与缺口 | 代码许可 / 建议 |
|---|---|---|---|
| [whisper.cpp](https://github.com/ggml-org/whisper.cpp) | 本地语音识别 | 玩家语音入口；不是语音合成或 NPC 思考系统 | MIT；权重另核 / 识别备选 |
| [sherpa-onnx](https://github.com/k2-fsa/sherpa-onnx) | 离线语音识别、合成、VAD 等运行能力 | 可组合成本可控的语音链路；声音质量/语言覆盖取决于具体模型 | Apache-2.0；每个语音模型单独核许可 / 语音候选 |
| [uLipSync](https://github.com/hecomi/uLipSync) | Unity 音频分析驱动口型，可运行时或预烘焙 | 二次元人物近景的直接加分项；需按角色与语音校准，不提供完整表演动画 | MIT；样例角色另核 / **优先试验** |

## K. NVIDIA 游戏 Agent 与数字人组件（3 项）

| GitHub 项目 | 已公开的主要用途 | 对我们的价值与缺口 | 代码许可 / 建议 |
|---|---|---|---|
| [NVIDIA Game Agent SDK](https://github.com/NVIDIA/game-agent-sdk) | 本地 SLM、Agent/Chat/RAG 接口；含两名角色共享一个模型的示例 | 端侧角色运行的重要备选；当前要求 Windows、Ampere 或更新 NVIDIA GPU、CUDA，模型另行下载 | Apache-2.0；权重/依赖另核 / 本地推理备选 |
| [NVIDIA ACE](https://github.com/NVIDIA/ACE) | 数字人与游戏角色的参考应用、语音/表情等微服务接入 | 近景角色展示可参考；公开示例不能等价为整套后端都以同一开源许可提供 | 按样例与微服务分别核查，涉及 NVAIE / 产品参考 |
| [Audio2Face-3D SDK](https://github.com/NVIDIA/Audio2Face-3D-SDK) | 音频驱动表情/情绪相关推理，C++/CUDA/TensorRT | 高质量表演候选；二次元脸型需映射与调校，硬件和模型许可有额外要求 | SDK MIT，部分模型需接受独立许可 / 后续按需 |

NVIDIA 的本地多角色示例说明“独立人物状态”可以共享模型权重；不代表这些示例已经完成了世界经济、永久经历或自主建设。选择云端模型作为第一版推理入口，是为了先验证行为与游戏闭环，之后可以把局部推理转到本地。

## GitHub 之外，也应该比较的资源

| 来源 | 为什么值得比较 | 本轮选择 |
|---|---|---|
| [VRoid Studio](https://vroid.com/en/studio) | 可以快速制作并导出二次元 VRM 人形 | 角色制作起点；不是完全开源制作器或已核实的自动生成 API |
| [Quaternius Medieval Village](https://quaternius.com/packs/medievalvillage.html) | CC0 中世纪村庄模块，可用于可公开运行的基线 | 可以借构件，不把原套 low-poly 外观直接当最终动漫美术 |
| [Meshy Text to 3D API](https://docs.meshy.ai/en/api/text-to-3d) | 托管生成任务省本地部署模型的工作 | 可选资产服务；不承担游戏规则、经济、角色记忆或模型使用语义 |
| [Convai Unity 官方说明](https://github.com/Conv-AI/Convai-Documentation/tree/main/plugins-and-integrations/convai-unity-sdk) | 语音、情绪、动作、视觉和长期记忆的产品化接入 | 适合快速比较近景 NPC 体验；商业后端不等于全部系统开源，也要核实数据可迁移性 |
| [Inworld Unity 角色交互模板](https://dev.docs.inworld.ai/Unity/runtime/demos/primitives/character) | 已有 LLM、语音识别与合成组合的角色交互示例 | 可参考角色交谈体验；不能据此推导出永久社会、自由建设或全部服务可自行托管 |

本轮没有测量这些服务价格、延迟或输出质量，因此未给出价格排名。资产 API 与 NPC 模型 API 应保持可替换，不能让更换供应商迫使居民或历史重建。

## 采用清单

第一批只验证：**Unity/URP、UniVRM、一个人物制作流程、OpenGameAgent、受限游戏动作和独立存档**。随后补齐一套场景构件、口型、视线与语音。

研究参考保留：Generative Agents、Concordia、AI Town、Mindcraft/Voyager、CUBE、SimWorld Studio。需要本地推理、复杂关系检索、多人或 VR 时，再从相应目录选组件。

最有价值的复用是节省通用工具与底层接入的工作，把精力留给**居民是否真的能生活、玩家是否愿意走进这个世界**。
