# 夜间 Kimi 人民币预算门禁

本次授权截止北京时间 2026-09-06 09:30，Kimi 总额度 100 元；账本只分配 95 元，另留 5 元余量。当前只允许中国区 `kimi-k2.6`。官方 2026-09-06 定价为每百万 tokens：未缓存输入 6.50 元、缓存输入 1.10 元、输出 27 元，见 [中国区官方模型卡片](https://platform.kimi.com/)。

## 实际请求路径

UE `HearthDecisions.cpp` 把该 Kimi 配置强制转换为 `http://127.0.0.1:18766/v1/chat/completions`。原生派发处再次拒绝直连 HTTPS 和未受保护的 Kimi 请求；其他未配置预算的 HTTPS 模型也被拒绝。原有本机离线假接口仍可测试。

`kimi_gateway.py` 是唯一真实上游入口：固定官方 HTTPS 域名、禁止重定向和环境代理、密钥只从本地配置读取。UE 只向本机代理发送随机本机访问令牌、操作 UUID 和居民 ID。项目 `HttpNoProxy=127.0.0.1,localhost` 避免本机通信被系统代理截获。当前居民 ID 使用数组下标；世界持久 ID 落地时同步替换。

代理限定纯文本、最多 8 条消息、上下文 UTF-8 总长 24,576 字节、请求 32 KiB、输出最多 512 tokens（当前 UE 默认256），关闭思考、流式、工具和多模态。模型和价格固定，拒绝其他生成选项。

## 金额与持久化

`kimi_budget.py` 使用整数纳元（1元=10亿纳元），未缓存/缓存/输出每 token 分别计 6500/1100/27000 纳元。每个短文本请求仍按模型完整 262,144 输入 token 上界预留，不依赖可能低估的字符换算或 token 估算；输出512时最高预留1.71776元，输出256时1.710848元。有效 usage 到达后，按输入、输出及缓存分别结算并释放多余预留。缺少缓存计数则按较贵的未缓存价格计；计数异常、超限或模型不符时保留预留并停止后续付费请求。

固定账本为 `Saved/ThreeHearths/Budget/kimi-overnight-2026-09-06.sqlite3`，配套独立 `.guard.json`。SQLite `BEGIN IMMEDIATE` 原子预留，`synchronous=FULL` 持久写入；guard、策略指纹、账本身份、记录数量和负债合计每次核验。新世界、重启和新测试都不创建新预算。账本丢失/损坏/策略改变则停付费，不自动恢复备份额度。初始化是一次性显式操作，已有 guard 或数据库就拒绝重新初始化。

全项目最多10个在途或未解决请求，每位居民最多1个。相同操作和相同输入若已结算，返回已保存结果而不再次调用；相同ID不同输入拒绝。超时、HTTP错误、上游已收到但代理被终止，均继续占用原预留；没有自动退款、立即重试或超时后释放居民槽位。重复结算不重复扣款。更换世界不改变这些记录。

此前协调任务的一次119 tokens连通性测试保守计入0.01元占用。官方余额通过只读接口额外记录，余额增加不会提高本项目授权，余额没有立即变化也不会撤销usage计费。账本是本项目保守核算，不冒充平台发票；平台最终扣费可有延迟。余额接口见 [官方余额文档](https://platform.kimi.com/docs/api/balance)，usage缓存字段见 [官方对话接口](https://platform.kimi.com/docs/api/chat)。

## 验收与操作

```powershell
python Plugins/ThreeHearths/Tools/test_kimi_budget.py
python Plugins/ThreeHearths/Tools/kimi_gateway.py status
# init 仅首次执行；当前本机已经初始化，不要重复或删除账本。
# python Plugins/ThreeHearths/Tools/kimi_gateway.py init
# 本地配置enabled=true后启动固定本机门禁，再进入村庄PIE：
python Plugins/ThreeHearths/Tools/kimi_gateway.py serve
```

14项离线验收通过：含10人独立并行、10个独立进程抢最后额度、上游收到请求后终止代理进程再重启、同居民排他、重复回调、超时、异常usage、坏档/丢档/改变策略、HTTP鉴权和请求限制。全部使用临时账本与假接口，不读真实密钥。

UE 5.8.1编译通过，6项原生测试通过，包含 `ThreeHearths.Decisions.PersistentBudgetRouting` 和扩展到10人容量的测试。原有 `ImportPreview.WorldIsolation` 仍有一条临时世界清理警告，本批没有新增该警告。

真实UE只允许3次初始选址，3次全部被采纳；账本对应3条已结算记录，共643输入+86输出=729 tokens，缓存0，按usage计0.0065015元，在途0。加此前保守占用后负债0.0165015元，剩余可分配94.9834985元。对账时平台余额尚未显示下降，账本保留上述计费。首次UE尝试被系统代理阻拦，未到达预算服务；修正本机代理绕过配置后通过。报告见 [Kimi预算验收](Validation/Kimi_Budget_Acceptance.json)。

UE 日常决策间隔默认为6秒模拟时间，随倍速缩放并在暂停时冻结；仍保留每人独立请求与最多10个并发。请求配额和预算报错使当轮停止继续调用，村民转为本地执行；权威费用来自持久账本，UE快照中的费用字段仅是最近收到的预算回执。世界存档、10人场景、生产与双向社会承诺均已接入，后续按社会执行路线继续交易、税收和家庭制度。
