# 本轮可移植验收证据

这里保留本轮上传前已有的关键验收结果，不是重新运行产生的结果。完整本机日志仍在忽略的 Saved 目录。

- native-regression.json：最后一次原生回归 100 项，94 成功、6 项带警告成功、0 失败；保留测试名称与事件，移除机器清单。
- market-multiplicity-report.json：Kimi 两轮提议对应的独立施工、费用、材料、位置和冷恢复检查。
- budget-final-status.json：累计费用与旧未决预留的汇总；不是密钥或预算数据库。其 deadline_utc 是基础账本字段，实际当晚网关/客户端另有 09:00 硬停止。
- review-lineage.json：实测分支的存档来源，避免误称所有场景连续沿用同一次运行。
- overnight-closeout.json：09:01 结束自身进程和定时任务的记录。
- organic-*.json：此前有机建筑原生样板、居民选择的专项测试原始结果。

可恢复的最终世界与历史在 ../../../Content/ThreeHearths/Data/MedievalShowcase/，包括校验和及默认关闭 API 的启动说明。实际画面和美术冷回导报告保存在 Art/MedievalLife 与 Art/OrganicVillageMasters。

本轮新增 59 次已结算 Kimi 请求、0.9474469 元。第二单货运仍卡在房屋边，6 根梁留在车上；婚育、战斗、坐姿等尚未完成。完整交付说明见 ../../Medieval_Night_Delivery_2026-09-08.md。

上传前补充检查：Kimi 预算的 18 项 Python 单元测试通过（临时账本，无付费调用）；所有候选 Python/JSON 可解析，最终世界/历史与 manifest 的 SHA256 一致。预算多进程测试首次受沙箱 Windows 命名管道权限限制，获准在本机环境重跑后全部通过。
