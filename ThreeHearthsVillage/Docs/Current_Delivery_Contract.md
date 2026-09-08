# Current delivery contract and coordinator review

## 2026-09-08 最新检查点：SAO 持续世界与三人委托

当前有效方向为独立的 SAO 第一层世界。旧国王征税、中央高地城堡及中世纪人物仅作历史档案，下文旧阶段中的“当前”不代表现行目标。

最新取舍：先验证三人之间一件可拒绝的真实委托，再验证记忆影响选择、换模型接续原档，最后兑现一个缺失能力。保留既有 13 个永久身份，本轮只激活其中 3 人。两位 Luna 分别完成步行入口和交易持久化内核，主代理负责集成、修复和验收。本次用户已明确授权将项目代码、美术、项目提示词与最新决定上传 GitHub。

当前完成的是修理、交付、结算与工具使用的代码和本地测试；真实 Kimi 尚未完整完成这条新增委托链。详细证据与已知缺口见 [本轮报告](Validation/Life_Checkpoint_2026-09-08/README.md)，提示词见 [项目提示词入口](Prompts/README.md)，决策见 [三方路线评审](Design/Persistent_World_Route_Review_2026-09-08.md)。本地测试不能代替真实居民验收。

以下章节保留历史工作记录。

## 2026-09-08 当前完成：城堡续建与小城用途规划

用户要求定位城堡停工原因，并在场景中设定住宅、市场生活、工坊、生产和城堡的位置。已落实七个不规则用途区、实际模块尺寸推导的城堡轮廓、全城取景和P键规划切换，分区接入选址软偏好。修复当前施工批次的私人木板生产、采购与税款工资衔接；续跑同一13人世界，城堡25→27，完成9张木板供货单并通过冷恢复。真实Kimi本轮13次（含1次协调者图片复核），新增0.2846744元；人物内在故事和原有房屋保持。108项测试通过。

完整实景、证据和限制见[城堡与分区验收](Validation/Castle_Town_Plan_2026-09-08.md)。金色线框不是完工建筑；83块阶段地板先于立墙的原配方没有修改。后续仍需真实税收与原木供给，不以注资或瞬间完工掩盖经济限制。下节林缘木匠、森林群落和高差地形仍是待实现方向，不能把本轮用途规划算作其完成。

## 2026-09-08 当前主线：地形、生态分区与住家生产

用户在实际演示后指出：地形看起来平坦、植物散落、缺少森林和职业生活形成的聚落。代码核对确认，地形王室抬升项仅155厘米且住宅有大范围整平台；装饰仍按10米网格抖动散点并避开住宅11米；当前世界仅有一个无主公共木工台。已有住家/职业/朋友选址偏好主要作用于开垦和建房，未形成住家工坊生产及资源林带的联系。

下一批优先围绕一个完整的林缘木匠生活区制定可执行验收：有层次的林带与空地、可辨识地势、住家附属工作台和堆料位、真实原木来源及通勤/搬运成本。Kimi提出带个人理由的空间需求，地方规则验证地形、产权、通路、邻居影响和真实费用后落地。不要把均匀增加树木、放大地块或新建无功能装饰当作完成。保留现有房屋、货物、工资、长凳和世界进度；该空间改进尚未实现，本节记录诊断与当前优先级。

## 2026-09-08 本机货运恢复验收

拉取 `fb5f3b9` 后继续原13人世界。第二单原有6根房梁已倒车脱困并实际送达，原工资3币只结算一次；冷启动后第一、二单及两张长凳完整保留。全套104项通过（98无警告、6带已有警告）。本轮真实Kimi9次新增0.1821785元。用户明确回复“直接用kimi”，沿用本机现有城市账本，不因旧主账本未同步再次索要同一授权；旧负债仍保守计入总预算。

证据与边界见 [马车恢复验收](Validation/Freight_Recovery_2026-09-08.md)。施工需求与运料匹配仍待处理：当次城堡25/1276，下个地面构件需5木板，工地仅1木板，已有12房梁；不能让车夫持续补梁而让缺板的施工一直等待。此前将该项称作石材是协调者读反数组顺序，现已更正；公共工程材料顺序为石材、木板、房梁。Kimi看图误识别必须经真实实体与账本核对后才能成为请求。本轮没有提交/推送，没有恢复旧定时任务。

## 2026-09-08 当前优先级更新

以仓库根 [AGENTS.md](../../AGENTS.md) 为持续协作规则。用户要求提高 Kimi 实际参与程度：真实模拟暴露需求 → 有边界的 Luna 工程 / Astra 美术 → 回到 Kimi 居民观察与行为验收 → 再决定下一批工作。每批可运行工程后必须回到这一闭环，不能连续堆积离线功能，以资产或测试数量替代社会进展。保留低频值勤、事件触发、费用和未决账本约束，禁止刷调用数。

本轮交付及已知缺口见 [中世纪社会本轮交付](Medieval_Night_Delivery_2026-09-08.md)。用户本次明确授权上传本轮全部项目改动；下文旧阶段的“不上传”限制不再适用于本次上传。早 9 点的运行与定时任务已经结束，本次规则修订和上传不重新启动它们。

以下为历史阶段与验收记录，不能视为当前尚未完成事项的唯一清单。

Updated 2026-09-06 after the user's request to diagnose repeated stopping and audit direction. Reviewed baseline: `24aebd4`. This contract narrows the next verifiable delivery inside the user's larger ten-NPC medieval society goal; it does not replace that larger goal or authorize new spending.

## Why the recent work stopped

The actual manager transcript ends normally with `final_answer` followed by `task_complete` at 09:47:27 UTC and 10:24:27 UTC. The second stop follows a successful local commit and explicitly lists runtime integration and persistence as future work. These are subunit completion stops; the inspected events do not show an API limit, compiler crash, or approval block causing these stops. The compact app status API returns older turn details and is unsuitable as sole evidence.

Repeated coordinator messages saying to continue have not supplied a durable, enforced completion condition. The manager must inspect its native goal state and carry the user's already stated outcome across turns. A plan document alone is not native goal state. Do not mark the overall outcome complete when a worker, test group, commit, or schema change completes. Report meaningful intermediate results as progress and continue the dependency chain. Keep a runnable next action recorded before a checkpoint.

## Direction review: useful foundations, unresolved integration and geometry

1. **The new functions are not gameplay yet.** Outside their own definitions and tests, `HearthResidentBuildingPlanner::Build` and `HearthTownLayout::Build` have no runtime callers at this baseline. `HearthWorldImage` is still schema 8 without StructurePlans. Fixed home positions and the 900-unit production candidate grid still control the running settlement. Removing display plinths alone did not produce NPC-shaped streets.
2. **The planner geometry does not yet match the real catalog.** In `HearthResidentBuildingPlanner.cpp`, every component has a synthetic 180-by-20 size and radius 18; component orientations remain default. The measured assets in `Art/Stage4HouseAudit/component-catalog.json` have different dimensions and origins: the foundation extends DOWN from its top datum, the floor is 2m square, the walls are infill between posts/beams, and the roof slope spans about 2.29 by 2m with about 1.41m height. The current planner puts its floor 1m sideways and 24cm above a foundation whose actual top is Z=0, describes the slope with Height=20cm, omits a real frame, and repeats room origins every 3.6m. Merely supplying Z values or naming a support wall does not prove a closed, connected, walkable building. Use catalog dimensions, origins, sockets and actual transformed geometry.
3. **Validation is weaker than the claims.** `HearthStructurePlan::Validate` checks that a support ID exists; it does not check physical contact or a grounded support path. Collision uses small center circles rather than actual rotated bounds. Road access accepts a caller boolean; it does not establish a route from the actual opening. Tests that assert assigned Z values or an ID containing `_support` cannot establish physical assembly. Add focused failures for disconnected support, overlap, and blocked access, plus actual engine/asset evidence.
4. **Extension is currently a proposal.** `AppendExpansion` validates with one billion coins/materials and road access forced true. That can represent an unfunded future design only. Before accepting executable work, validate against the real household/treasury source, marginal unbuilt material demand, existing occupancy and routes, reserve once, and persist work state. Replaying one extension key must not add another room or charge again. Existing ID/transform preservation tests must compare ALL old instances; `ContainsByPredicate` only establishes at least one surviving ID.
5. **NPC variation is still a small local rule.** The current helper recognizes a few English words, selects one/two initial rooms and grows them on one axis. It is not evidence that live NPC personality, relationships, Kimi decisions or actual world needs drive a settlement. Connect authoritative live inputs and keep the provenance of each decision explicit. Local rule fallback is valid but must be labelled.

These findings do not require discarding working finance, procurement, tools, cottage component jobs or public wall code. Reuse their proven ownership, transport, wage and persistence flows.

## Evidence required to complete the current delivery

- A real UE ten-NPC world on continuous terrain calls the new planning and layout logic; at least three visibly distinct household/workshop plans reuse the same base component catalog, with documented live NPC reasons.
- One persistent building is expanded at least twice by later needs/resources, preserving every old component ID and transform. Larger plans are composed by repeated components/connections, not swapping whole-building assets or a permanent two-room cap.
- Plans use actual mesh dimensions, correct origins/orientations, physically valid supports and usable entrances/routes. Show an overview and close-up progressive assembly on the display containing Codex.
- NPCs reserve and carry real materials, install independent components and settle authorized wages/purchases once. An unfunded proposal does not become installed geometry. Cancellation and repeated actions conserve money/materials.
- Persistent component instances, decision reasons and partially built extensions survive save/reload. Migrate existing schema6/8 worlds without moving old houses; include real old public-haul states, loaded and empty, including an empty route away from the depot.
- Necessary build and meaningful regression checks pass on the delivered code. A pure-function test, offline candidate GLB or static preview is supporting evidence, not completion of live NPC construction.

## Execution and checkpoint policy

Sol/medium manages; Luna/medium implements bounded, nonoverlapping files. Root Astra reviews concrete milestones and failures. Reuse the five existing worker lanes as needed, but respect the actual concurrency cap and a single UE/Blender/build owner. Freeze shared 3D/catalog/persistence contracts before dependent edits.

Next dependency chain: (A) authoritative asset geometry and validation contract; (B) persistent plan/instance/work state and migration; (C) live NPC planning plus existing material-backed component executor and rendering; (D) real simulation, extension/reload evidence and corrections. Independent work may overlap, dependent interfaces must not race. Do not spend another entire delivery only adding unused planning helpers. Reuse already imported timber/stone components where possible.

Keep progress concise: current step, actual artifact/evidence, next executable action, and any concrete blocker. If a lane fails, diagnose or reassign that lane while independent work continues. A missing Blender MCP connection does not by itself block work with already imported assets. Avoid frequent transcript polling and redundant full-suite runs.

Preserve the user's existing cumulative Kimi CNY100 authorization/CNY95 allocation and unresolved-request ledger. Do not add budget, clear uncertainty, buy credits, redeem another Codex reset, push, or launch extra user-owned tasks. Keep the current models. Native goal activation does not authorize bypassing a pause, budget stop or genuine user-action blocker.

## 2026-09-06 runtime acceptance checkpoint

The current branch now has live runtime evidence for the integration slice above:

- The restored ten-resident world contains three authoritative component plans with one, two and three rooms. The three-room home records two later extensions and retains all earlier component IDs. The owners and reasons differ (king/relationships, potter household, merchant shelter and budget).
- Site 14 remains occupied by the completed 15-part public wall. A new residential site is created only after live terrain, footprint, clearance and route checks; the accepted run placed site 24 at `(-3450,-1800)` and completed all 16 transported components there.
- The third completed plan was saved as world revision 10 and reloaded with the same world ID, plan IDs, owners, site IDs, room counts and installed-component counts.
- The overview is `Saved/ThreeHearths/Acceptance/ten-npc-three-plans.png`; pre- and post-restart snapshots are `ten-npc-three-plans-state.json` and `ten-npc-three-plans-restarted-state.json`. The persisted acceptance world is `ten-npc-neighborhood.json`.
- `Saved/Logs/FinalFullAcceptance.log` records 31 successful `ThreeHearths` automation tests and zero failures. Focused public-site finance and movement recovery evidence is in `PublicSiteOwnership.log` and `MovementSamePoint.log`.

The runtime roof audit remains documented separately under `Art/RoofMaterialAudit`. The current generated NPC homes deliberately use the timber roof material family; broader resident-selectable wall and roof material variants remain part of the larger village goal rather than evidence claimed by this checkpoint.

## 2026-09-06 final continuous-neighborhood audit

The dependency chain in this contract is now implemented and verified. The authoritative revision-14 acceptance world contains six completed live plans and 156/156 independently transported and installed components. It adds a real stone-wall mason home, preserves the three-room home's two extension IDs, and reloads every plan identity and every stored component transform/bound unchanged. Resource ledgers reconcile to the ten-resident initial endowment, no construction escrow or model request remains pending, and the final native automation run records 31 successes, zero failures, and exit code 0.

The requirement-by-requirement evidence, local-fallback provenance, screenshots, generated reload/conservation summaries, hashes, and exact artifact paths are recorded in [Ten-NPC continuous neighborhood acceptance](Validation/Ten_NPC_Continuous_Build_Acceptance.md).

## Next checkpoint: cooperative tile workshop

Continue the larger society goal with one narrow causal chain: residents negotiate a tile order from persistent relationship, need, price, and craft ability; real clay and wood fuel become a finite tile inventory at a reachable kiln; tiles are carried and consumed by an independently installed native ceramic roof component. Acceptance and refusal must have different persistent relationship consequences. Cancellation, route interruption, repeated callbacks, and save/reload must preserve ownership, escrow, money, and material conservation.

Reuse the existing native kiln, handheld tile bundle, and curved terracotta roof assets. The current timber roof is truthfully a wooden material variant and does not count as ceramic. Compose the workshop and its approach from the shared site/component vocabulary on a visible bent or courtyard layout, preserving all existing houses and component instances. Keep Kimi and local fallback provenance distinct, preserve the current cumulative budget ledger, and do not push.

## 2026-09-07 cooperative tile workshop checkpoint

This checkpoint is implemented and verified. A continued real UE ten-resident run completed nine persistent tile orders and installed four native terracotta slopes on two residents' new component houses from their personally owned delivered tiles. The same run recovered from an exhausted public treasury through potter- and artisan-funded input labor, grew from six to nine structure plans, retained every old component identity and transform across restart, and kept all API activity disabled. High-speed simulation no longer repeatedly serializes the full history for every decision or catches up a startup hitch in one blocking frame. Full evidence and limitations are recorded in [Cooperative tile workshop acceptance](Validation/Tile_Workshop_Acceptance.md); the current suite records 35 successes and zero failures. The next correctness work is the documented busy-customer delivery restoration and turning workshop presentation pieces into separately persistent NPC construction jobs; the 300x soak also identified the single-precision absolute-clock horizon at 1,048,576 simulated seconds.
