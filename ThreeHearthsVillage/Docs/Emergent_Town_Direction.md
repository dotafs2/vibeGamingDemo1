# Continuous town shaped by NPC decisions

User clarification, 2026-09-06: isolated small plots do not match the intended game. Modular art should form one coherent, larger scene, with visible variation arising from NPC choices over time.

Interpretation: a continuous medieval settlement with streets, courtyards, workshops, homes and a castle, all assembled from reusable components. This is not a request for one giant building. Internal grids may still support navigation and construction validation, but visible square display bases and a fixed one-resident/one-slot layout must not determine the town's appearance.

## Diagnosis from the current implementation

- BuildIslandVillage fixes ten home positions and draws 5.4m square plinths with colored borders and simple connecting strips.
- InitializeProduction chooses up to eighteen expansion sites from a 9m-spaced candidate grid. Each site has one dominant use and a fixed-sized soil patch.
- HearthCottage::Populate emits the same forty-five components at fixed offsets for every new modular cottage. Individual installation exists; varied spatial assembly does not yet exist.
- The initial NPC house choice accepts only supplied plot_id and one of three house_style_id values. The new modular cottage recipe does not consume a richer NPC plan for shape, orientation, extension, facade or shared frontage.
- Initial style assignment also follows resident index modulo three. Asset variety and a construction ledger alone cannot produce socially motivated town morphology.

## Required direction

Latest user example: the same family of small building materials should support a small tent-like shelter, a very large enclosed camp/compound, or a castle assembled piece by piece. Interpret this as scale-independent composition and reuse, not a request for three new fixed templates. NPCs decide counts, connections, dimensions, openings, rooms and extensions; function follows the resulting arrangement. Avoid a mandatory fixed component count or a building-type enum that limits valid forms. Reusable room/roof assembly rules may help validation, but must permit changed spans, repeated bays, connected spaces and later growth. Material properties and quantity still matter: fabric shelter surfaces require a real fabric source; wood/stone constructions must use their actual supported components and supplies.

The next acceptance must show reuse of the same base component IDs across substantially different sizes and functions, and enlargement of an existing plan without swapping a whole-building asset. Three different predefined house templates alone do not pass. Persist every chosen instance/connection and its real material source, including partial extensions.

1. Make one continuous settlement readable in an overview. Normal presentation should show terrain, paths, thresholds and real parcel features; show selection/ownership boundaries only when relevant, rather than permanently displaying square bases.
2. Separate land ownership, buildable footprint, building plan and individual components. A building may grow by rooms or extensions; a shared street/courtyard can connect several properties. NPC count must not imply a fixed number or size of home slots.
3. Give NPCs meaningful, constrained building choices: where to live relative to work/friends/market, affordable footprint, room needs, orientation toward access, workshop attachment, and supported wall/roof/door/window/decor options. The executor validates terrain, access, collisions, structural connections, supply and cost before reserving work.
4. Build a persisted component plan from those choices. Use attachment/socket rules or an equivalent bounded assembly grammar. Shape, layout and additions should visibly differ, beyond recoloring or jittering identical whole houses.
5. Record why each choice happened and persist it: needs, occupation, means, relationships, local geography and approved policy. Stable variation/seeds can choose between equally suitable valid options; do not reroll appearance every load. Later prosperity or changed household needs can cause material upgrades or extensions.
6. Keep the existing ten NPCs, real resource/coin flows and incremental construction. Assets made by Codex/Blender populate the catalog; NPC plans determine which eligible pieces get used and where. A completed static art display is not acceptance for autonomous settlement growth.

## Next acceptance slice

Safely integrate the currently active finance/procurement/public-wall work first; retain useful uncommitted work and avoid simultaneous rewrites of shared files. Then prioritize one coherent street or small neighborhood on the same terrain. Within the ten-NPC simulation, observe at least three distinct NPC household/workplace plans using the same component catalog, differing visibly in footprint/orientation or extensions and at least one facade/material choice where real supply permits. Show one later extension or improvement driven by changed needs or resources.

Acceptance must include an overview of the connected settlement, close-up progressive assembly, persisted NPC decision reasons, usable routes and doors, real material/cost changes, and save/load preserving placements and variation. Compare against the current isolated-square presentation. Do not substitute more identical plots, random colors, decorative instant-spawn buildings or a hand-authored town for this behavior.

Sol manages the work and integration; Luna implements bounded pieces under the latest Agent_Work_Plan.md cost/concurrency policy. This direction changes the next visual/layout priority, not the Kimi budget, save safety, model hierarchy or no-push rule.

## Foundation progress, 2026-09-06

- The island no longer renders permanent square plot plinths or colored ownership borders. Existing saved positions remain unchanged while the visible presentation moves toward continuous streets.
- `HearthTownLayout` now produces deterministic street-front candidates from roads, access, nearby work/market/friends and blockers. Home and rear-extension IDs remain stable when unrelated existing buildings are inserted.
- `HearthStructurePlan` now represents reusable catalog components, sockets, rooms, openings, connections, explicit material recipes and decision reasons. Validation covers support, collision, door access, component budget and available materials; failed extensions roll back atomically.
- `Art/TownKit` contains six offline glTF candidates and attachment metadata. Four candidates use current stone/plank/beam stocks. The two roof joint candidates retain a real `tiles` dependency and remain blocked until tiles exist in the runtime economy. Blender and Unreal import/render acceptance is still pending.
- Public-wall transport now persists an explicit depot/site phase, refunds safely on invalid return routing, and restores the complete original site if approval fails. The full `ThreeHearths` suite passes 24/24 after these changes.

These are planning, validation and candidate-art foundations. Runtime NPC plan selection, schema migration, component visualization and save/load of new plans remain the next implementation slice.
