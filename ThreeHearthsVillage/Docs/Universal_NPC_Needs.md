# Universal NPC Needs and Delivery Contract

## Scope

Every NPC physical need is in scope for the shared needs system. The target is
not a tavern-only feature. Water, food, shelter, cooling, storage, meeting,
rest, and future physical needs must be representable by the same request,
catalog, construction, review, and functional-use contract. Luna implements
the main change; the parent task integrates it and runs the tests.

The need module and its data may vary by need. The construction mechanism is
generic: an item or home declares what need it satisfies, what capacity or
effect it provides, where it can be built, which resources it consumes, and
which usable point or points it exposes. Callers must not branch on an asset's
display name. Different items and homes must use the same API and contract.

Examples include a barrel that stores water, a cellar that stores food, and a
smallhouse or canopy that supplies a shelter-related capability. These are
examples of the same contract, not special cases. An underground asset does
not automatically enable underground behavior, and a cooling mesh does not
automatically enable cooling. Those effects require explicit catalog/data
declarations, runtime behavior, construction support, and acceptance evidence.

## Shared delivery loop

The delivery path for each need is:

1. **Shared core need definition.** Describe the physical need, its state,
   capacity, satisfaction rules, usable location, and failure or unmet state
   in the common needs model.
2. **Catalog reuse, composition, or custom art.** Search the existing catalog
   first. Reuse a validated component, compose validated pieces, or have us and
   Luna make a custom model when the catalog cannot meet the need. All three
   paths produce the same contract-bearing item or home record.
3. **Unmet-mechanic proposal.** When no existing item or home can satisfy the
   need, record the unmet mechanic and an actionable artist brief. The brief
   identifies the need capability, capacity, interaction points, constraints,
   and required evidence; it does not claim that art, behavior, or approval
   already exists.
4. **Validated delivery.** A delivered asset must pass the applicable import,
   naming, scale, coordinate, collision, material, placement, and catalog
   checks. A delivered model is still pending functional acceptance until it
   works through the shared contract.
5. **Normal resource construction.** The item or home is placed through the
   ordinary construction flow, consumes the declared resources, respects
   placement and occupancy rules, and persists through save and cold reload.
   Need-specific code must not bypass the normal construction path.
6. **NPC real-image review.** Review the actual in-world result with the NPCs,
   usable points, surrounding paths, scale, collisions, and visual context.
   This is a real rendered or captured scene review, not a mesh-only claim.
7. **Functional-use acceptance.** Demonstrate that an NPC can discover the
   capability, route to it, use it, update the relevant need state, and recover
   correctly when the capability is unavailable. Verify the same result after
   save and cold reload where persistence applies.

The acceptance record should identify the need, item or home, catalog/data
entry, construction transaction, resource change, usable point, NPC action,
image-review evidence, and test result. The parent fills in actual results as
the loop is completed.

## Art and implementation boundary for this turn

The generic request/export path has been exercised in a real UE world and with
two real Kimi image reviews. This establishes delivery of proposals to an artist,
not delivery of a newly made model or a working gameplay capability. The model
registration, construction and functional-use loop remains separate work.
Town3's earlier component, migration and castle failures were repaired; all 64
native tests and the actual 30-resident runtime/cold-reload smoke test passed.
This is independent of request-flow acceptance and does not complete the
generic asset-delivery or functional-use loop. See
[Town3 acceptance](Validation/Town_Growth_Acceptance.json).

## Ownership and next acceptance

### Actual verification, 2026-09-07

UE 5.8 compilation, all 10 Python exporter tests, and all four selected native
tests passed. The JSON-test shallow-copy defect was fixed using an independent
negative fixture and checked result arrays.

An isolated copy of the accepted Town2 world received four scripted proposals
(barrel, cellar, small storage house, and flower pots). Repeating a request did
not duplicate it. Actual cold reload preserved requests, building plans, public
works and resident stories. Two real Kimi reviews under explicit barrel/cellar
test goals produced two typed proposals with real owner and observation IDs;
both exported successfully to artist briefs. Persistent stories and resources
were unchanged. Cost was CNY 0.027589, cumulative CNY 0.270312 / 22 settled calls.

The first local scene run returned an exception code after its results were
saved and UE's log closed. Cold reload, Kimi and a separate two-capture recheck
exited normally. This isolated shutdown exception remains recorded without
claiming it was repaired. No newly generated model or functional use is claimed.
See [the evidence and actual images](Validation/Generic_NPC_Needs/Kimi_Proposals.md)
and [the machine record](Validation/Generic_NPC_Needs_Acceptance.json).

The user clarified that code, build and test errors are repaired autonomously.
Only an ambiguous or infeasible gameplay requirement calls for a user decision.

Luna owns the main implementation of the generic request and export path. The
parent integrates the work, resolves conflicts, runs the native suite, and
records the evidence. The next acceptance target is one end-to-end example
for each distinct contract shape needed by the current scope, including a
water-storage item, a food-storage item, and a shelter-related home or
structure, while exercising the same API with no hardcoded item or home name
checks. Each example must reach normal resource construction, NPC real-image
review, and functional-use acceptance before the need is marked delivered.
