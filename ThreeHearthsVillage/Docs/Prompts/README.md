# 主线提示词入口

默认只读根目录 [AGENTS.md](../../../AGENTS.md) 与 [CURRENT_STATE.md](../../../CURRENT_STATE.md)。其他内容按任务加载。

| 内容 | 入口 |
| --- | --- |
| 开发AI短接手提示 | [AI_PROMPT.md](../../AI_PROMPT.md) |
| 有界派工模板 | [Agent_Workflow.md](Agent_Workflow.md) |
| 居民身份与初始人设 | [resident_profiles.json](resident_profiles.json)，不等于完整存档 |
| 当前SAO运行时指令索引 | [runtime_prompt_catalog.json](runtime_prompt_catalog.json)，自动生成，按需查找 |

NPC的实际个人请求由 `HearthAincradResidentRuntime.cpp` 及其状态模块生成；动态视野、记忆、合法动作、食物和合同事实以真实世界为准。真实修理交易/进食/休息/采集已有证据，但长期自给、第二模型接管及Godot生活迁移尚未完成。

源码索引只保留当前Aincrad主线，不混入旧村庄、国王和接口测试提示词。历史源码和Git记录仍可查，不删除游戏所需运行时逻辑。重新生成使用 `Tools/export_project_prompt_catalog.py`；它不读取API配置或费用账本。
