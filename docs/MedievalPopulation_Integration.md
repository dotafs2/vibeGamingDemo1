# Medieval population integration plan

This document describes the smallest migration needed to add three real medieval
residents to organic village version 4: one gatekeeper, one royal guard, and one
carter. They are simulation residents with actors, duties, wages, history, and
selection support. They share public accommodation and do not consume housing
plots. This is an integration plan only; it does not change the core world files.

## Current contract to preserve

The current save format is schema 11. Organic version 4 still has ten housing
plots (`AHearthVillage::HousingPlotCount()` in
`Plugins/ThreeHearths/Source/ThreeHearths/Public/HearthVillage.h`, around line
400), and the existing world therefore has ten residents, one owner per plot.
`FHearthWorldImage` in `Private/HearthWorldState.h` has `Schema=11` and
`PlotCount=3` as its default legacy value. The serialized people array is
validated against the plot count in `Private/HearthWorldState.cpp` around line
712. That equality is the main population/plot coupling to remove.

The first ten residents keep their stable IDs, actor/house data, plot owners,
tax history, wallet balances, personal inventory, and existing bonds. Version 4
continues to have ten plots. The three new residents have no plot, no house
construction fields, no starter goods, and zero coins at migration:

| Resident | Migration role key (not the resident StableId) | Role | Plot | Coins | Initial residence |
| --- | --- | --- | ---: | ---: | --- |
| Gatekeeper | `medieval-gatekeeper-01` | `门卫` | `-1` | `0` | public gatehouse/dorm |
| Royal guard | `medieval-royal-guard-01` | `护卫` | `-1` | `0` | public guardhouse/dorm |
| Carter | `medieval-carter-01` | `车夫` | `-1` | `0` | public stable/dorm |

The role keys above are migration markers only. Existing world validation
requires every resident `StableId` to be a valid GUID: do not use these plain
strings as resident IDs or weaken GUID validation. Allocate a GUID once inside
the atomic migration and persist the marker-to-GUID mapping with the world.
On reload reuse that mapping. Do not generate new GUIDs merely because actors
are being respawned, and do not share resident GUIDs across different worlds.

## Schema 12 shape

Add a `PopulationCount` field to `FHearthWorldImage` and serialize it as
`population_count`. Keep `PlotCount` as the number of buildable private plots.
Schema 12 for the organic village is therefore:

```text
schema          12
plot_count      10
population_count 13
people          13 records
```

The decoder in `Private/HearthWorldState.cpp` should accept schemas 1 through 12.
For schemas through 11, derive `PopulationCount` from `People.Num()` after the
existing legacy decoding rules; do not reinterpret old `plot_count` values. For
schema 12 require `1 <= PopulationCount <= HearthVillageLimits::MaxPopulation`
and `People.Num() == PopulationCount`. Continue validating `PlotCount` against
the existing allowed values (3, 10, or the Town 3 population), because plot
arrays and private housing remain plot-sized.

The 11-to-12 migration should run only for the organic ten-plot world with ten
people. It appends exactly the three catalog residents, sets
`PopulationCount=13`, and leaves `PlotCount=10`. Reapplying the migration must
find the three migration markers and their persisted GUIDs and make no second
append. A malformed save containing only one or two markers must fail atomically rather than silently
repairing the roster. The existing 3-to-10 migration in
`Private/HearthSociety.cpp` around lines 44-76 must remain unchanged and must not
receive the new residents.

Do not put a population count into the plot arrays. The fixed arrays in
`FHearthVillage` remain bounded by `MaxPopulation`; only their active plot range
is `PlotCount`. Resident references in other arrays are bounded by
`PopulationCount`.

## Runtime creation and load ordering

`AHearthVillage::ResetVillageState()` in `Private/HearthVillage.cpp` around lines
520-585 currently creates one resident per plot and initializes starter values.
Keep that path for the ten plot owners, then append the three explicit shared
residents with `Plot=-1`, `BuildProgress=0`, empty carried/delivered materials,
zero personal goods, zero coins, `LifeChoosing`, and an actor at the real shared
gatehouse/guardhouse/stable anchor. Do not run the per-plot starter resource or
wallet initialization for them.

For loading, `Private/HearthPersistence.cpp` currently decodes, validates, and
then applies into the already-sized `Residents` array. Around lines 96-142 it
also sizes pending decisions from the existing array and copies resident data by
index. The migration must append/construct the three runtime residents only
after the decoded image has passed structural and conservation validation, and
before actor validity checks and the final copy. The operation needs a rollback
path: a failed actor spawn or later validation must leave the pre-load village
unchanged. A fresh v4 runtime may create the three shared actors before
`ApplyWorldState`; an existing runtime load needs an explicit
`EnsurePopulationCapacity(13)` step between decode/migration and apply.

The first ten records must be copied by stable ID or by their unchanged legacy
order, then the three new records by their catalog IDs. Never use a changed
array index to reassign a private plot. Resize `PendingDecisions` and any
parallel resident arrays only after the decoded population count is known.

## Shared residence rules

`Plot=-1` is valid only for an explicitly shared resident. Use a narrow helper
such as `IsSharedMedievalResident(Index)` backed by the catalog role/ID. Do not
make every resident with `Plot=-1` valid: that would hide corrupt private-house
saves.

Shared residents must satisfy all of these invariants:

* no entry in `PlotOwners`, no plot cost, and no private house blueprint/style;
* `BuildProgress`, `CarriedWood`, and `DeliveredWood` are zero;
* no private construction or private-house task can be selected;
* they may be `LifeChoosing`, `LifeTravel`, or `LifeActivity` only when the
  corresponding public anchor exists;
* they may have an active duty task only with a non-empty `ActiveTaskId` and a
  real duty/site reference; an empty task ID remains valid only while choosing;
* rest, food, social, and work routes resolve to the shared dorm, gatehouse, or
  stable anchor rather than calling `HomeApproach(-1)`.

The validation in `Private/HearthWorldState.cpp` around lines 770-776 currently
requires every non-idle resident to have a plot and build progress. Split this
into the legacy private-house rule and the explicit shared-duty rule. Keep the
existing rejection of carried materials or build progress on a `Plot=-1`
resident. Keep `BuildProgress >= 1` for private residents before house/build
tasks. This is a scoped exception for three catalog residents, not a global
relaxation.

`CompletedHomes()==Residents.Num()` in `Private/HearthVillage.cpp` around line
981 must count private plot owners or compare against `HousingPlotCount()`; the
three shared residents are not unfinished homes. `ReservePlot`, `CostFor`,
`PlotNameFor`, and all private construction operations should reject or skip a
shared resident. A missing plot must never become plot zero through a clamp.

## Exact population/plot audit

The following locations need a population-aware bound when the field contains a
resident reference. A field that describes a plot must continue using
`PlotCount`.

### `Private/HearthWorldState.cpp`

* Around lines 438-475: decode `population_count` for schema 12, preserve the
  old plot-count decoder, and bound selected/last-life resident indices by
  `PopulationCount`.
* Around lines 451-462: `TaxRemainders` is resident-account state; require and
  serialize `PopulationCount` entries for schema 12. Preserve the old shape for
  legacy schemas, then migrate the first ten values without changing them.
* Around lines 492-680: bounds for `People.Plot`, site owner/reserver, history
  resident, conversation participants/speakers, transactions, tax residents,
  wages, trades, tile orders, and public-project workers/king must use
  `PopulationCount`. Site and plot indices continue to use their own site or
  `PlotCount` bounds.
* Around line 712: replace the unconditional `People.Num() == PlotCount` rule
  with the schema-aware population equality while retaining the legacy rule for
  old images before migration.
* Around lines 740-778: split shared-resident task validation from private-house
  validation as described above; validate `LifeAction` against the population
  count and preserve the existing task/action ID ranges.
* Around lines 807-847: resident owner/reserver checks use population bounds;
  plot ownership remains plot-bounded and one-owner-per-plot.
* Around lines 848-894: validate conversations and bonds against all 13 stable
  IDs. Sparse bonds are acceptable; any stored target must resolve to a resident.
* Around lines 895-980: wallet, tax, and wage conservation loops use population
  count. Treasury and starter material baselines remain plot based for v4.
* Around lines 982-1121: all resident references use population count, while
  the starter food/wood formulas stay based on ten plots. Existing v4 values
  remain food 100, wood 99, treasury 500, and first-ten starter wallets 12.

### `Private/HearthSociety.cpp`

Keep the current 3-to-10 migration. Add a separate, idempotent 10-to-13 organic
migration with no material or coin grant. `InitializeResidentIdentity()` around
lines 4-23 currently supplies the legacy identity catalog; add explicit medieval
identities without changing the first ten stable IDs or their role/personality
data.

### `Private/HearthPersistence.cpp`

Around lines 32-64, export all `Residents.Num()` records and set both counts;
copy only `PlotCount` active plot slots. Around lines 96-142, resize runtime
residents and pending decisions after validated migration and before applying
actors/data. The actor check must include the shared anchors. Any failure must
discard the temporary image and temporary appended actors together.

### `Private/HearthFinance.cpp`

`PrepareIncomeTax()` around line 6 rejects `Resident >= 10`; replace that literal
with a resident-array/population validity check. The other transfer, wage reserve,
and wage settle paths already use `Residents.IsValidIndex` in the current tree.
New residents begin at zero. A wage is created only by the normal
`ReserveWage()` to `SettleWage()` path, producing the normal wallet, treasury,
transaction, and tax effects. Never seed coins to make them appear employed.

### `Private/HearthLife.cpp`

`LoadHistory()` around line 64 has a hard-coded `Index > 9`; use the decoded/current
population range. Social action IDs are currently `3 + resident index` in
`AvailableLifeActions()` and `StartLifeAction()` around lines 215-273. Preserve
that namespace, but bound it by `PopulationCount` and route shared residents to
public anchors. Resize `PendingDecisions` with the roster, and clamp selected
resident/last-life state by population rather than plots.

### `Private/HearthSocial.cpp`

`IsSociallyAvailable()` around lines 48-52 currently requires `BuildProgress >= 1`.
Keep that requirement for private residents and allow the three shared residents
only when they are choosing, not already in a conversation, and have a valid
public social anchor. `BeginConversation()` and bond validation should use the
dynamic resident range and stable IDs. Do not generate reciprocal friendship
records automatically unless both sides are valid; empty bonds are a valid
initial state.

### `Private/HearthVillage.cpp` and `Private/HearthInterface.cpp`

Keep plot generation at ten. Skip shared residents in plot reservation, house
cost, build progress, and private-home completion logic. The interface already
iterates `Residents.Num()` in `HearthInterface.cpp` around lines 58 and 76 and
validates selection dynamically, so the main changes are role/residence labels
and safe handling of `Plot=-1`. `FocusResident()` around lines 336-339 should
focus the resident actor or public anchor and must not derive a camera target
from a missing plot. The selection command-line path around line 276 should use
the resized population range. Any UI text that calls `PlotNameFor()` or
`CostFor()` must show a shared-duty/dorm description for the three new roles.

## Active task and action ID boundaries

`ActiveTaskId` remains a GUID for an actual active job. A shared resident in
`LifeChoosing` has no active task; a shared resident travelling, working, or
delivering has a task ID and a validated duty/site reference. Do not reuse an
empty ID as a “shared” marker.

Keep the existing life action namespace (`3 + resident index` for social actions,
50 for eating, and the current production/tavern ranges). With 13 residents the
social index range is 3 through 15. Every decode and validation site must use
`PopulationCount` for this range. Guard, gate, and carter duty identifiers should
live in the MedievalSociety duty catalog or a separately reserved operation
range; do not overload private-house actions or invent an unbounded action ID.

If a duty pays wages, reserve and settle it through the existing finance ledger.
The operation must have a real source/funder and a completion event. A failed
route or cancelled task releases its reservation according to the existing
ledger rules and does not create a wallet balance.

## Finance and material conservation

The migration preserves the current organic v4 baseline:

```text
private plots                 10
starting treasury             500
legacy wallets                 10 * 12 coins
new shared wallets             3 * 0 coins
starter food                  100 (plot baseline)
starter wood                   99 (plot baseline)
new resident personal goods    0
```

The first ten residents retain their existing wallet and inventory values from
the save. The three appended records add no coins, food, wood, planks, beams, or
tiles. Their first positive balance must be traceable to a settled royal wage
or another existing transfer with a source account. Tax and transaction history
must contain that transfer after it occurs; migration itself must not fabricate
a transaction.

Tax remainder storage and all wallet/tax/wage references must be sized for 13
residents, but starter formulas stay plot-based so old v4 worlds do not gain
resources merely by loading schema 12. Reject negative balances, invalid source
indices, duplicate transaction IDs, or a wage payable whose worker is not in the
population.

## Selection, social state, and UI

Selection is resident-index based and can address indices 0 through 12 after
migration. Existing residents retain their current selected index semantics.
The new actors must be selectable, show their role and shared residence, and
remain valid after save/load. Their display must not call private plot functions
with `-1`.

Conversation participants, speakers, history owners, tax residents, wage
workers, and bond keys all refer to the population, not the plot array. On load,
conversation IDs and bond targets must either resolve to one of the 13 records
or be rejected atomically. The three new residents may begin with no bonds and
no conversation; later social changes must be persisted by stable ID.

## Migration sequence

1. Decode into a temporary `FHearthWorldImage` and determine schema, plot count,
   and population count without mutating the live village.
2. Apply the existing legacy migrations, including 3-to-10, exactly as today.
3. For an organic ten-plot, ten-person image, append the three catalog records
   once and set `PopulationCount=13`. Verify the original ten IDs and plot-owner
   mapping are unchanged.
4. Resize temporary resident-indexed arrays, tax remainders, decisions, and
   actor slots to 13. Keep plot arrays at 10.
5. Run all structural, task, bond, transaction, wage, tax, and conservation
   validation against the temporary image.
6. Construct or append the three real shared-resident actors and resolve their
   public anchors. If any construction or anchor check fails, discard the
   temporary additions and leave the live world untouched.
7. Apply the validated image, resize pending decisions, clamp selected resident
   to the population range, and update the UI/selection state.

## Acceptance checklist

* A schema 11 organic v4 save loads with ten residents exactly as before.
* A schema 11 organic v4 save migrates to schema 12 with ten unchanged legacy
  records plus exactly gatekeeper, guard, and carter; `PlotCount=10` and
  `PopulationCount=13`.
* Re-saving and reloading the migrated world is byte/logically stable for IDs,
  plot owners, wallets, stocks, tax history, and shared records.
* Reapplying the migration does not duplicate residents, actors, bonds, wages,
  inventory, or transactions.
* No new resident receives starter coins, food, wood, planks, beams, or tiles.
  Their wallet remains zero until a real wage transfer settles.
* A real settled wage increases the worker wallet, decreases the valid source,
  records the transaction/payable transition, and participates in normal tax
  accounting. A failed or cancelled wage leaves balances conserved.
* The ten private plots still have one owner each, no shared resident owns a
  plot, and private construction/completion still targets ten homes.
* Shared residents can be selected, saved, loaded, rested, socialized, and
  assigned a catalog duty through public anchors without `HomeApproach(-1)` or
  plot-index crashes.
* Non-idle shared residents require a valid `ActiveTaskId` and duty/site; an
  invalid shared record is rejected without mutating the live state.
* Social action IDs 3 through 15, conversation participants, history, bonds,
  tax records, wages, trades, and public projects accept all 13 resident IDs
  while site/plot indices retain their old bounds.
* Invalid population counts, duplicate new IDs, missing one-of-three migration
  records, negative resources/coins, orphaned bonds, bad task/action IDs, and
  out-of-range resident references fail atomically.
* Legacy Town 2 and Town 3 save/load, population, plot, treasury, and starter
  resource tests remain unchanged.
* UI resident count, selection, camera focus, role labels, and shared residence
  text remain valid before and after save/load.

The implementation order should be schema/image validation first, atomic runtime
resize and actor creation second, wage/duty integration third, and UI/selection
handling last. This keeps the existing ten-person economy and private-house
rules intact while making the three new residents real simulation participants.
