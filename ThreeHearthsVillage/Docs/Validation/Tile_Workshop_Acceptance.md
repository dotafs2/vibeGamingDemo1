# Cooperative tile workshop acceptance

Validated on 2026-09-07 on branch `codex/pie-import-preview` with UE 5.8.1.

## Result

The ten-resident acceptance world now completes the tile-workshop causal chain. The run migrated the existing six-plan schema-9 neighborhood to schema 10 without replacing any old house, then continued at 300x using local rules with the API explicitly disabled.

- Nine accepted orders completed. Each order escrowed four customer coins, consumed four recorded clay and two logs, produced and physically delivered six tiles, settled one `tile_order` transaction, and assessed income tax once.
- The potter completed six clay trips and nine kiln firings. Potter-owned input labor can be self-funded when an exhausted public treasury would otherwise deadlock the supply chain; public firing without an order still requires public funds.
- Oscar and Thomas each used twelve personally owned ordered tiles for a new 16-component home. Four native `roof_slope_terracotta_2m` components are completed with `source=resident_owned` and `supply_policy=private_tile_order`.
- The kiln workshop is assembled from the native kiln, clay basket, tile crate, and timber bench around an open court beside the bent street layout.

The run expanded from six to nine persistent structure plans. After restart, the world ID remained `F8F4D755-4F2E-ABC0-3C1B-1A95B43F5938`. None of the 219 pre-restart component IDs disappeared, geometry changed for zero old components, and all 206 components already complete at the checkpoint remained byte-for-byte unchanged. Thirteen in-progress components legitimately advanced to completed status while the restarted society continued.

High-speed scheduling now batches the separate decision-history archive by real time and does not convert a startup/render hitch into thousands of catch-up steps. Normal 300x frames still advance the full 300x simulation clock. NPC gameplay decision deadlines, conversations, orders, travel, and production use simulation time; HTTP timeout and budget receipts use real time. A late model response is accounted for but cannot overwrite the NPC's newer local action.

## Evidence

- `Saved/ThreeHearths/Acceptance/tile-workshop-live4-20260907.json` — final schema-10 continued world.
- `Saved/ThreeHearths/Acceptance/tile-workshop-pre-reload.json` — pre-restart checkpoint.
- `Saved/ThreeHearths/Acceptance/tile-workshop-reload-comparison.json` — component, order, inventory, and reload comparison.
- `Saved/ThreeHearths/Acceptance/private-tile-home-closeup.png` — focused real-UE view of the native ceramic roof components and their specular response.
- `Saved/ThreeHearths/Acceptance/tile-workshop-two-private-tile-homes.png` — real-UE neighborhood view after two private-tile homes completed.
- `Saved/Logs/TileWorkshopFinal.log` — 34 successful `ThreeHearths` automation tests, zero failures, exit code 0.

The automation suite separately covers accepted and refused orders, pre-work cancellation and refund, route interruption, idempotent settlement, material conservation, private-tile construction cancellation, schema-10 validation, and late-response dual-clock behavior.

This evidence came from deterministic local fallback and does not claim Kimi authored the decisions. No Kimi request was sent, and the existing cumulative CNY100 authorization, CNY95 allocation, CNY5 reserve, and original ledger were not changed.
