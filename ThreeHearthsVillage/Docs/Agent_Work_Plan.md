# Development roles and cost policy

User direction, 2026-09-06: keep the coordinator on GPT-6 for sparse milestone review; use the main project task as a GPT-5.6 Sol manager, and delegate implementation to five GPT-5.6 Luna specialists.

## Responsibilities

- Coordinator: meaningful milestone review, intervention on concrete failures, and a quiet check every 30 minutes. Do not repeatedly read full transcripts or large documents.
- Project manager: GPT-5.6 Sol / medium. Decompose tasks, define shared interfaces, inspect evidence, integrate changes, and keep the work moving. Small integration edits are allowed; substantial implementation belongs to Luna.
- Implementers: GPT-5.6 Luna / medium. Spawn with fresh context and only the relevant task, files, interfaces, and acceptance criteria. Do not inherit the full game conversation. Report changed files, verification, and blockers concisely.

## Five implementation lanes

1. Finance: atomic income/tax settlement, protected tax funds, escrow and refunds. Start from the existing uncommitted HearthFinance.cpp work; preserve it. Own the dedicated finance implementation and focused tests.
2. Persistence: schema, historical tax policies, procurement/project state, validation and migration. Own persistence/world-state implementation and tests after agreeing the public data contract.
3. Procurement: real supplier ownership, orders, reservations, physical delivery, acceptance/payment and cancellation. Own a dedicated procurement implementation and tests.
4. Public construction: king approval, public ownership, stable wall components, sourced recipes, hauling/installation, routes and cancellation. Own dedicated public-works implementation and tests.
5. Art and runtime validation: native PublicWallKit visuals, material/source checks, representative acceptance scenarios, scene evidence and documentation. This lane owns any required UI/asset operations.

Sol must assign concrete non-overlapping files before spawning. It owns shared public declarations and integration points; freeze or version the interface contract so agents do not concurrently rewrite the same header or production controller. Dependency-bound work waits for its interface, while independent pieces proceed.

Request five workers through the supported subagent tools and inspect actual model/concurrency results. Project configuration requests five spawned workers excluding the manager; if the running session exposes a lower cap, obey that cap and queue the remaining lanes. Do not claim five simultaneous workers until verified, and do not create extra user-owned tasks or independent CLI sessions to evade the cap.

Only one owner may operate UE/Blender or run the native build at a time. Use the display currently containing Codex. Preserve uncommitted work at the handover; do not reset, duplicate, or discard it. Verify meaningful unit boundaries, then one integrated build/test run. Avoid five copies of the full test suite, duplicated paid simulations, and endless status polling.

## Current delivery target

The current coordinator review and concrete completion contract are in `Current_Delivery_Contract.md`. They take precedence over completed public-wall lane descriptions below when choosing the next work. Keep the broader user direction in `Emergent_Town_Direction.md`. The five specialist lanes can be reassigned to current geometry, persistence, live NPC construction, layout and asset/runtime validation work; they are not permanently tied to completed finance work.

The manager owns continuing from completed subunits to the next executable dependency. Finishing a commit or a worker task is not completion of the current delivery. Use the native thread goal for the user's explicitly requested continuing outcome, inspect its actual state, and distinguish a checkpoint from a completed goal. Keep pause/budget lifecycle controls with the user/system. Do not ask the coordinator to send another generic wakeup after every commit.

The user's latest visual/layout clarification is in `Emergent_Town_Direction.md`. After safely integrating the current finance/procurement/wall unit, prioritize a continuous NPC-shaped street/neighborhood with genuinely varied component plans. Repeating fixed plot displays and one fixed cottage recipe does not satisfy that goal.

The finance/procurement/public-wall slice is integrated and runtime accepted: a local-policy ten-NPC run completed 15/15 parts from 36 stone, 21 purchased resident-owned planks and 12 beams, then survived save/reload without duplicate transactions. The final suite passed 22/22. The next implementation target is the continuous settlement described below.

Tax funds must reconcile with cash and escrow, ordinary spending cannot consume protected project funds, and failed taxable settlements leave all state unchanged. Preserve applied historical tax policy or keep the initial approved policy immutable. Include transaction-capacity checks. Materials must have real ownership/source; public allocations must not be labelled purchases. Preserve source transaction/order/component IDs, money and material conservation, cancellation and mid-operation save/load. Demonstrate one completed wall segment with ten NPCs and clearly identify local-rule versus real Kimi behavior.

The original cumulative Kimi CNY 100 authorization / CNY 95 allocation and persistent unresolved-request gate remain unchanged. No new budget, bypass, recharge, extra Codex reset, or push. Paid failures do not prevent independent code/art work. Escalate a specific difficult diagnosis to the coordinator after targeted Luna attempts fail; do not routinely switch the manager back to GPT-6. Continue subsequent useful work after a subunit finishes.
