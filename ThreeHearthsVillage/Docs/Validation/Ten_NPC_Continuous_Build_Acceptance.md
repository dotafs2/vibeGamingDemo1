# Ten-NPC continuous neighborhood acceptance

Validated on 2026-09-06 against `Docs/Current_Delivery_Contract.md` on branch `codex/pie-import-preview`.

## Result

The current delivery slice passes. A real UE 5.8.1 ten-resident world autonomously produced six persistent, material-backed component plans on the continuous street grid. The plans contain 156 independently transported and installed components. They include one-, two-, and three-room layouts; timber and stone wall execution; and resident-specific need, occupation, relationship, budget, preferred-material, executable-material, and road-access reasons.

The acceptance run was deliberately local-rule fallback (`-HearthDisableApi`): it made zero API requests and does not claim that Kimi authored these six plans. The existing Kimi cumulative CNY100 authorization, CNY95 allocation, CNY5 reserve, and ledger remain unchanged. The runtime's normal Kimi-default configuration was not changed.

## Live plans

| Owner | Occupation | Site | Rooms | Installed | Distinguishing live reason |
|---:|---|---:|---:|---:|---|
| 3 | King | 22 | 3 | 46/46 | Shelter plan followed by two later extensions as nearby friends and household count increased |
| 5 | Potter | 23 | 2 | 31/31 | Shelter plan followed by a household-driven extension |
| 1 | Farmer | 24 | 2 | 31/31 | Timber/slate-blue preference; timber roof chosen because no tile inventory existed |
| 9 | Apprentice | 25 | 1 | 16/16 | Plaster/terracotta preference; honest timber fallback because neither production inventory existed |
| 4 | Merchant | 27 | 1 | 16/16 | Timber/slate-blue preference; affordable one-room executable plan |
| 2 | Mason | 28 | 1 | 16/16 | Stone-wall/terracotta preference; real stone walls and door with timber roof fallback |

The runtime snapshot records each plan's conserved per-room inputs. For example, the mason's executable room uses five stone, three planks, and eight beams. Every plan uses the same native foundation, floor, frame, wall/door, and roof component catalog; material variants change catalog members rather than replacing the house with a monolithic mesh.

## Expansion and reload invariants

Owner 3's site-22 home records these two distinct later extensions:

- `resident_172085AF-4603-B12D-879C-4B87757EBE28_house:extension:1`
- `resident_172085AF-4603-B12D-879C-4B87757EBE28_house:extension:2`

It grew from one room/16 components to three rooms/46 components. The pre-restart and post-restart snapshots have the same world ID `F8F4D755-4F2E-ABC0-3C1B-1A95B43F5938`, revision 14, six plan IDs, owners, sites, room counts, and component counts. A field-by-field comparison of all 156 component IDs, catalog IDs, extension IDs, XYZ offsets, yaw, and authoritative bounds passes. Thus reload neither moves nor replaces old component instances.

The runtime regression also saves during the first extension, reloads that partial extension, starts a second later extension from a changed need, and compares every old component field. It rejects repeated/invalid additions atomically. Schema-6 migration keeps legacy resident positions unchanged. Schema-8 public-haul fixtures cover loaded cargo, empty cargo at the depot, and an empty route while physically away from the depot without minting cargo.

## Geometry, support, and entrance

`Art/ResidentialVariants/native-validation.json` is a read-only audit of the real imported UE assets. It records native mesh bounds and origins, source/native triangle parity, UV presence, PBR values, and disabled Nanite status. Runtime plan components store these catalog bounds.

Plan validation transforms the real bounds by component yaw, verifies catalog-bound equality, checks physical support contact plus allowed parent catalog, rejects unintended component overlap and occupied-volume collision, and checks the actual door opening direction and clearance. The completed-cottage runtime regression finds a route from an NPC to the entrance approach and verifies that the approach remains outside the building footprint.

## Transport, payment, and conservation

The executor reserves finite current materials and an authorized private wage before work starts. NPC jobs carry each component's material from its recorded source and install that component independently. The close-up history records the mason's final `roof_slope_timber_2m` delivery and confirms all 16 components transported, installed, and paid.

The restarted state reconciles the initial ten-NPC endowment exactly:

| Account | Initial | Accounted after run |
|---|---:|---:|
| Food | 100 | 100 |
| Wood | 99 | 99 |
| Stone | 0 | 0 |
| Planks | 0 | 0 |
| Beams | 0 | 0 |

It has 1 treasury coin, 0 protected project coins, 0 active escrow, 327 released project-tax coins, 1,605 transaction records, 731 wage-payable records, and no pending API request. The regression suite additionally covers private-house wage escrow, protected project funds, atomic taxable income, procurement, cancellation/failure, reload, and repeat-callback behavior. Failed or repeated actions do not debit materials or settle wages twice.

## Visual and machine evidence

- `Saved/ThreeHearths/Acceptance/ten-npc-overview-two-homes.png` — real UE overview at 300x with multiple houses simultaneously at different component assembly stages on one continuous road grid.
- [Three-plan neighborhood overview](Delivery_2026-09-07/ten-npc-three-plans.png) — committed real UE overview showing the persistent one-, two-, and three-room neighborhood result.
- `Saved/ThreeHearths/Acceptance/six-plan-stone-house-closeup.png` — real UE close-up with the mason's stone-wall house, roof highlights, NPC, and component completion/payment history.
- `Saved/ThreeHearths/Acceptance/ten-npc-neighborhood.json` — persisted acceptance world.
- `Saved/ThreeHearths/Acceptance/scheduler-speed-tax-housing-state.json` and `scheduler-speed-tax-housing-restarted-state.json` — revision-14 pre/post restart snapshots.
- [Six-plan reload comparison](Delivery_2026-09-07/six-plan-reload-comparison.json) — committed field-by-field plan/component reload comparison.
- [Six-plan conservation summary](Delivery_2026-09-07/six-plan-conservation.json) — committed resource and settlement summary.
- [Six-plan artifact manifest](Delivery_2026-09-07/six-plan-artifact-manifest.json) — committed SHA-256 and byte length for the original evidence artifacts.
- `Saved/Logs/DeliveryContractFinal.log` — full `ThreeHearths` automation run: 31 successes, 0 failures, exit code 0.

Key implementation and regression sources are `HearthStructureCatalog.cpp`, `HearthStructurePlan.cpp`, `HearthResidentBuildingPlanner.cpp`, `HearthProduction.cpp`, `HearthPlannedConstructionRuntimeTests.cpp`, `HearthPersistenceTests.cpp`, `HearthModularCottageTests.cpp`, and `HearthMovementTests.cpp` under `Plugins/ThreeHearths/Source/ThreeHearths/Private`.

The artifact manifest includes the following central hashes:

| Artifact | SHA-256 |
|---|---|
| Persisted acceptance world | `f9573c1ded3e04f296ecbafa8e5b9e7ac9dc3b656166d300bd880854edebd2eb` |
| Progressive overview | `ca1366d38d5ce6cef55f66fb1e6b7ef51271902a7f1bc21c4ec91be2314d9028` |
| Three-plan overview | `2fa65d224c14e0af587acc5b611985edc36c4a28b2835cd014aed64f31f4a412` |
| Stone-house close-up | `cbe5dedddc9f5c4a98906e7326afe68c3631ec4a90d145d9edb70ed4c36fe33b` |
| Final 31-test log | `51e5893a088bdebf2648b5c3773cce08d0700e675bcb4711e788b9c5cf21fdbd` |
