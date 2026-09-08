# 项目提示词与决策入口

本目录收集本项目可维护的提示词、人设、需求与派工约定。运行时实际文本以对应源码和已提交的真实请求证据为准；动态个人视野、库存、记忆及可选动作由原存档生成，不把它们写死进人设。

| 内容 | 入口 | 状态 |
| --- | --- | --- |
| 用户世界目标与最新决定 | [World_Brief.md](World_Brief.md) | 当前有效；整理稿，不是逐字聊天导出 |
| 13 位永久居民的人设与初始故事 | [resident_profiles.json](resident_profiles.json) | 从当前 SAO 存档提取，只保留身份与人设字段 |
| 项目源码中的提示词文本及位置索引 | [runtime_prompt_catalog.json](runtime_prompt_catalog.json) | 自动提取候选指令文本；动态拼接逻辑仍保存在对应源码 |
| 工程代理的可复用任务模板 | [Agent_Workflow.md](Agent_Workflow.md) | 本轮分工及边界的整理稿 |
| 三方路线评审与最终取舍 | [持续世界路线评审](../Design/Persistent_World_Route_Review_2026-09-08.md) | 当前路线 |
| 逐步执行及验收标准 | [起始之城执行计划](../Design/Starting_Town_Iteration_Plan.md) | S4 交易内核已有代码与本地测试；真实新流程仍待验证 |
| 原作地图与风格基准 | [第一层基准](../Design/Level0_First_Floor.md)、[美术标准](../Design/Starting_Town_Art_Style.md) | 当前有效；来源不确定部分标为项目补全 |
| 真实 Kimi 请求及个人图片 | [样板街验收](../Validation/StartingTown_2026-09-08/README.md)、[本轮报告](../Validation/Life_Checkpoint_2026-09-08/README.md) | 保留真实证据，区分改动前与改动后 |

当前个人决策提示词在 `HearthAincradResidentRuntime.cpp` 的 `DispatchDecision` 中，分为既有四动作模式和新增生活委托模式。后者把出生故事、个人记忆、眼部截图和 `life_options` 组合成请求；它已经编译，但截至本次上传尚未完成真实 Kimi 委托验收。

历史中世纪提示词也保留在源码索引与原文档中，作为档案；国王、税收、皇家城堡不是当前 SAO 世界规则。提示词文件的存在不表示其对应功能正在运行。

仓库根 `AGENTS.md` 保存项目协作要求。这里只归档项目拥有的提示词与需求；不是平台内部指令、账户凭据或完整聊天记录的导出。私有配置、费用数据库、运行时鉴权信息不进入版本库。

重新生成目录：使用本机 Python 执行 `Tools/export_project_prompt_catalog.py`。脚本只读取限定的项目源码和指定 SAO 存档身份字段，不读取 API 配置或费用账本。
