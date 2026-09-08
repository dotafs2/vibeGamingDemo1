# 项目协作规则

适用于整个仓库。用户当前明确指令优先；阶段文档中的日期、暂停、模型与上传限制属于当时任务，不能覆盖后续授权。当前目标入口：[Level0：艾恩葛朗特第一层](ThreeHearthsVillage/Docs/Design/Level0_First_Floor.md)。旧交付仅作历史：[中世纪社会本轮交付](ThreeHearthsVillage/Docs/Medieval_Night_Delivery_2026-09-08.md)。

## 2026-09-08 最新世界方向（覆盖下文旧国王目标）

用户明确要求完全转向 SAO 艾恩葛朗特第一层地理与设定，项目代号 Level0，废弃中央高地城堡与国王征税路线，并选择“新建 SAO 世界，旧世界留档”。新世界使用独立地图 `L_AincradLevel0` 和 `Saved/ThreeHearths/AincradLevel0/world.json`；禁止让旧社会启动逻辑、旧皇家工程或旧人物记忆混入新世界。原世界保留为档案。新世界建立后，继续保持同一批新人物及存档，不因模型升级或重启随意重建。

参考按原作/Progressive/动画区分，资料中未确定的精确地图细节标为项目补全，不把游戏改编或同人想象图冒充官方测绘。当前先验证地理及城市体量，再接入居民自主生活；能力和建模提议必须区分待实现、已批准、实际可用。

## Kimi 驱动迭代（用户 2026-09-08 明确要求）

用户指出 Kimi 调用相对 Luna 太少，工程投入没有足够转化为真实社会模拟。默认把更多运行时间和已授权 Kimi 预算用于真实 NPC 观察、交流、选择与回访；不要长时间只派发 Luna 扩展功能。

1. 每轮先继续一个可玩的真实 UE 存档，让 Kimi 居民经历生产、交易、社交、建造或值勤情境，检查他们实际遇到的问题与提出的需求。离线与模拟 API 检查只负责技术验证，不能代替真实 Kimi 验收。
2. 从运行证据中选出最小可执行改进，再派发有边界的 Luna 工程任务。若真实运行被崩溃、配置或功能缺口阻断，先修复阻断点，并记录原因和恢复实测的下一步。
3. 每批可运行改动合并后，先回到同一社会继续实测、让相关 NPC 观察结果并作出后续选择，再扩展下一批功能。连续两批 Luna 开发之间必须有真实运行反馈；阻断修复可例外，但不能把例外变成长时间脱离游戏的开发。
4. 验收记录必须包含：运行时长与世界/存档标识、真实 Kimi 请求数和费用、关键居民观察/决策、由此产生的工程任务、落地行为与下一轮反馈。分别标明 Kimi 决策、本地回退、人工指定；模型数量、测试数量、代码行数和 API 次数本身均不能证明社会进展。不掌握 Luna 消耗时不得编造两者比例。
5. 增加有意义的场景覆盖和居民互动，不能为凑调用数刷空请求。普通思考保持至少 30 分钟现实时间冷却，门卫/护卫私下白日梦保持 6 小时现实时间冷却；真实访客和重大事件按现有事件策略触发。模拟加速不能增加收费频率。保持并发、去重、保存后的冷却和预算账本约束。
6. 延续已有累计 Kimi 100 元授权 / 95 元分配上限，保留旧未决预留；预算是上限而不是消耗目标。不得清账、增额或通过重新建档绕过预算。本规则不自动启动付费运行，不恢复已经结束的定时任务，也不覆盖用户要求的停止时间。

## 目标、分工与验收

- 目标是居民自主形成的中世纪社会：情感与关系、真实生产和买卖、国王收税与逐步筑城、连续地形，以及有居民理由的错落聚落。构件和材质可分层复用、扩展组合，避免固定小地块上重复整栋房屋。
- Astra 负责 Blender 样板、整体轮廓、材质风格和最终实际画面；Luna 负责边界明确的工程、UV、导入导出与整理。使用已有代理，遵守并发上限；UE、Blender及共享构建各有单一操作负责人，避免相互覆盖。
- 材料、钱、施工、位置与关系必须来自真实持久状态。新需求可以增加实例，重试同一请求不能重复扣费或安装。模型陈列不等于玩法完成。
- 美术验收看真实引擎画面与几何/材质/动画；行为验收看真实行动、账本和冷恢复。完成一个子任务不代表终极目标完成。保留已知失败和可执行下一步。
- API 密钥、私有配置、预算数据库、Saved、缓存和构建产物不入 Git。提交完整代码、美术源文件、导出物、必要原生资源、可移植检查点和可核对的交付证据；大文件使用 Git LFS。仅在用户授权范围内提交或上传。

# DOTAFS Workspace Instructions

## Paper and learning-note work

For every task that creates, rewrites, reviews, or publishes a paper note,
learning article, article index card, or article-specific visualization in the
DOTAFS website, read the following file completely before making changes:

`C:\vibeGamingDemo1\.codex-publish-dotafs-pbf-site\docs\article-design-standard.md`

That document is a mandatory acceptance standard, not optional inspiration.
Apply it to new articles and to any existing article being substantially
reworked. Preserve existing interactive tools and local edits unless the user
explicitly asks to remove them.

If the detailed standard is temporarily unavailable, do not invent a generic
landing-page treatment. Use a compact research-note structure with: research
problem, related-work comparison, method overview, method details, evidence,
advantages, limitations, practical mapping, and attributed primary sources.
Avoid oversized hero text, marketing slogans, neon gradients, excessive cards,
and decorative animation.

Do not publish, commit, or push article changes unless the user explicitly asks
for publication in that turn.
