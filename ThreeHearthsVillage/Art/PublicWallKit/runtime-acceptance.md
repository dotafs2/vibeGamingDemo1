# PublicWallKit runtime acceptance evidence

This slice binds the four imported PublicWallKit modules to the public works ledger. `AHearthVillage::RefreshPublicVisuals` displays only completed parts, at the approved production site plus each authored centimetre offset, with unit scale and the imported material slots unchanged.

Runtime acceptance completed on 2026-09-06 in an isolated ten-NPC world with the local policy and API disabled. The run completed all 15 parts, bought 21 resident-owned planks, paid 15 tax-funded construction wages, saved schema 8, then reloaded the same project without duplicating orders or transactions. Evidence is in `Saved/ThreeHearths/continuous-development/PublicWall_Runtime_Acceptance.json`, `PublicWall_SaveReload_Acceptance.json`, and `tool-action.png`; the final automation log is `Saved/Logs/PublicWorksFinalTests4.log` (22/22 passed).

## Isolated ten-NPC run

Use a fresh world path and keep the original `current-world.json` untouched. Run with `-HearthNoWorldPersistence -HearthDisableApi -HearthSimulationSpeed=100` for deterministic local validation, or use a unique persistence path when save/load evidence is required. The local run uses the fixed 25% policy and must be labelled **local policy**. It does not prove the real Kimi backend path. A Kimi run may be captured separately only when the configured backend is available and the evidence records `backend`, `api_status`, and `model` from `demo-state.json`.

The accepted run covers these checkpoints:

1. Approval: `public_project.status=building`, immutable `policy=local_king_fixed_income_tax_25`, king/site and `approval_history_id` populated.
2. Partial assembly: only completed parts appear in the world; each `public_project.parts` record retains required, reserved, delivered, worker, task and status fields.
3. Procurement: `orders` records include project ID, resident-owned origin, reserved quantity, escrow and status; `total_active_escrow` matches active order escrow.
4. Conservation and cancellation: cancel an active worker or supplier, then verify public stock, resident/private funds, protected funds and escrow return exactly once.
5. Persistence: save, reload the isolated world, and verify completed placements and ledger IDs remain stable.
6. Completion: all 15 parts are completed exactly once, `status=completed`, and the wall uses four reusable module IDs.

The screenshot is an overview from the actual UE runtime. It also makes the remaining fixed square-plot presentation visible; it is evidence for the wall slice, not acceptance of the continuous-town direction.

## Direction boundary

The public wall is a reusable boundary component for the continuous-street settlement direction. The 15-piece wall display is a construction/visual acceptance slice, not final town acceptance. The current square bases and fixed house styles remain baseline content awaiting the broader continuous settlement work; this evidence must not present them as the finished town layout.
