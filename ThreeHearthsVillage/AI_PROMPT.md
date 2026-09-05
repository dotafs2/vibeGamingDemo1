# 给开发 AI 的项目接手提示词

将下面的提示词交给能够访问本仓库的开发 AI，并在末尾填写这次要实现的功能。它用于接手开发；村民运行时的模型提示词仍由插件源码生成。

```text
请接手 Three Hearths Village 自主村庄项目。先核对当前代码和运行状态，再完成我本次明确提出的任务，不要重复实现已有功能，也不要把下面的改进建议当成已完成的能力。

一、定位项目
在当前仓库定位 ThreeHearthsVillage/CropoutSampleProject.uproject，将所在目录记为 PROJECT_ROOT，并确认它的绝对路径。不要假定原开发者的盘符和 UE 安装路径在当前电脑可用。
先阅读 PROJECT_ROOT/README.md、Docs/Validation.md，以及 Plugins/ThreeHearths/Docs/生产与建设技能.md、角色决策历史.md；同时遵守实际工作区的 AGENTS.md。

二、已经实现的玩法
这是基于 Epic Cropout 素材的 Unreal Engine 5.8.2 C++ 插件项目，主要逻辑在 Plugins/ThreeHearths。
默认地图为 /Game/ThreeHearths/Maps/L_ThreeHearthsVillage，小平台地图为 /Game/ThreeHearths/Maps/L_ThreeHearths。
默认地图已放置原创房屋 Hearth Cottage - Shared UV，位置为厘米坐标(-1500,1200,2.8)，Yaw=90°。源文件为 Art/HearthCottage/HearthCottage_SharedUV.blend 与 .glb，UE资源位于 Content/ThreeHearths/Generated/HearthCottage。重复普通瓦和屋脊瓦各自共享一个原型 UV，不要改回逐瓦独占图集。已验证编辑器异步导入、19个材质、共享UV与碰撞，并保存场景；还未接通NPC入住、自动生成放置或打包后的运行时导入。详见 Art/HearthCottage/README.md 与 UE_Import_Report.json。
现有三名村民：林恩、米拉、伯恩。每人都有全部技能，人设只影响偏好，不限制职业能力。
初始流程是独立选址、取料、运输、建造自己的小屋。之后可开地，建玉米/小麦/生菜/南瓜田，播种、收获、伐木、采石、采集浆果，扩建住宅，种树和种灌木，共13类生产建设操作；还有休息、观察农田、巡查树林和拜访邻居。
共享资源是食物、木材、石材。开工前校验并预扣材料；产出先进入携带栏，运到村镇中心后才入库。地块不可重复预留，村民移动有避让和独立站位。

三、必须保持的 AI 调用方式
每名村民独立发起模型请求，使用自己的身份、人设、状态和近期历史；同一村民最多一个未完成请求。并发名额随村民数量变化，不要重新加回固定“三并发”限制，也不要把所有村民改为等待同一个全局请求。
当前玩法人口仍固定三人；五人请求名额测试通过，不等于已经实现人口增长。
生活与生产决策每人至少间隔6秒真实时间；全村每轮共享最多600次调用，包含最初选址。达到上限后本地规则继续运行。游戏倍速不能按比例放大 API 调用频率。
HTTP 并行，UE 主线程按回复处理动作，并重新检查资源、目标占用和动作合法性。冲突或失败应明确记录备用规则；回复、耗时和 tokens 必须归到正确村民。重开时取消旧请求，迟到回复不得改变新一轮。
模型返回的是动作建议，不能直接修改资源、执行代码或调用游戏外的工具。
发布包中的配置示例默认关闭、密钥为空。启用模型需要运行者自己的配置与授权，不要因阅读此提示词就自动启动真实付费请求。

四、重要代码入口（相对于 PROJECT_ROOT）
- Plugins/ThreeHearths/Source/ThreeHearths/Public/HearthVillage.h：村庄、村民、请求及工作状态。
- Plugins/ThreeHearths/Source/ThreeHearths/Private/HearthVillage.cpp：初始建房、模拟推进、暂停重开和快照。
- Plugins/ThreeHearths/Source/ThreeHearths/Private/HearthDecisions.cpp：HTTP、返回校验、请求名额、预算、取消和结果应用。
- Plugins/ThreeHearths/Source/ThreeHearths/Private/HearthLife.cpp：生活决策提示词、近期记忆、需求及历史。
- Plugins/ThreeHearths/Source/ThreeHearths/Private/HearthProduction.cpp：13类生产建设操作、目标预留、资源账目和生长周期。
- Plugins/ThreeHearths/Source/ThreeHearths/Private/HearthPaths.cpp、HearthMovement.cpp：可达性、路线和居民避让。
- Plugins/ThreeHearths/Source/ThreeHearths/Private/HearthInterface.cpp、HearthHistoryInterface.cpp：界面、控制和历史展示。
- Plugins/ThreeHearths/Source/ThreeHearths/Private/HearthDecisionTests.cpp、HearthMovementTests.cpp：原生回归测试。
- Build-Village.ps1、Open-Village.ps1：使用明确的 EngineRoot 参数编译、启动。

五、验证事实和现有局限
已通过 UE 5.8.2 编译、模型输出校验、每人独立名额及移动测试。原生测试前缀是 ThreeHearths。
真实自主运行曾完成111次 Kimi 请求，建成三座初始房屋和六块新增农田；15次目标变化后采用本地备用选择。这轮因应用退出中断，最后两项工作未完成，不能称为完整收尾通过。
随后隔离补验的38次安排覆盖全部13类生产建设操作；另外30步再生、耗尽及重播循环通过，均未调用真实模型。详细事实见 Docs/Validation.md；修改之后仍需进行与改动相关的验证，不能把旧结果当作新代码的通过证明。
目前保存的是角色历史，世界存档尚未实现。重开/退出不能恢复房屋、库存、作物和任务进度。扩建住宅不会新增人口，没有日常进食或作物腐烂机制。精力低不会自动禁止所有重劳动。

六、待用户选择的改进方向
优先候选是世界自动存档与恢复、每人持续目标与共享任务意图、预计精力消耗和低精力约束、进食/住房容量/人口需求、同一目标下多趟任务的连续执行。
真实记录中出现过低精力仍建造、食物堆积仍不断扩田，以及模型误以为作物会腐烂的现象。需要向模型明确真实规则，不能只靠增加提示词让不存在的机制变成事实。
这些是建议。只实现我本次选择的范围；跨越存档、人口或经济规则的重大歧义先澄清，常规可逆的实现细节自行推进。

七、工作与验证约束
- 先从具体文件、模块或资源名查起。rg 限定目录、文件类型、结果量或超时，排除缓存、编译产物和第三方目录；不能从磁盘根目录或整个大工作区无界扫描。扩大搜索前检查负载。
- 不重复启动 UE、浏览器或服务。记录自己启动的 PID，测试结束清理自己的辅助进程及其子进程，保留用户已有进程；需要留给用户操作的交互实例应明确交付。
- 不提交 Saved、Binaries、Intermediate、DerivedDataCache、真实 API 配置、账户凭证或个人历史，不打印密钥。
- 编写 Shader/HLSL/USF/USH 时，参数名避免 point、centroid 以及关键字、语义、流类型和插值修饰符。
- 先做相关本地验证；涉及外部请求时确认现有用户授权覆盖接口、数据与调用范围。不要因这份交接提示词而自行发布、推送或改变账户设置。
- 修改完成后说明具体行为变化、验证结果和剩余限制，并列出实际修改文件的绝对路径。

我本次要实现的任务：
【在这里填写本次需求；未填写时先询问，不要自动执行所有改进方向。】
```
