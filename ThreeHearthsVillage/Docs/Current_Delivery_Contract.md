# Current delivery contract and coordinator review

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
