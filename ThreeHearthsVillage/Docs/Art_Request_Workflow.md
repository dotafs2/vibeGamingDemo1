# Offline art request workflow

`Plugins/ThreeHearths/Tools/hearth_art_jobs.py` turns the host board export into two review artifacts:
`hearth_art_jobs.json` and `hearth_art_jobs.md`. It accepts only schema 1 input:

```json
{"schema_version":1,"world_id":"world-01","requests":[{"id":"asset-01","category":"asset","status":"proposed","summary":"...","resolution":"...","requester_ids":["npc-01"]}]}
```

The runtime accepts exactly these categories: `narrative`, `existing_action`, `asset`,
`mechanic`, and `clarification`; statuses are `proposed`, `resolved_existing`,
`needs_details`, and `deferred`. The board contains at most 64 requests, summaries and
resolutions are at most 512 characters, identifiers are at most 128 characters, and
each request has 1..32 requester IDs. Only `category: asset` with `status: proposed`
creates a job. All other valid requests are accepted and ignored for art export.
Newlines and ordinary whitespace in text are normalized; other control characters,
duplicates, unsafe IDs, oversized documents, traversal paths, and malformed catalog
references fail before writing.

Run from `ThreeHearthsVillage`:

```powershell
python Plugins/ThreeHearths/Tools/hearth_art_jobs.py --input Saved/ThreeHearths/art-requests.json --output-dir Saved/ThreeHearths/art-jobs
```

The optional `asset_context` object binds a physical need to its actual requester
and target. Its fields are `purpose`, `target_id`, `resident_contexts`,
`target_position_cm`, and `observation_id`. Each resident context records
`resident_id`, `name`, `personality`, `inner_story`, and `design_goal`. Position
is a finite three-number array or null; an unknown target or observation is an
empty string. The NPC's story is a snapshot of the persistent story, not a new
story generated for an asset job. Old exports without this object remain valid.

The generic runtime entry is `SubmitResidentAssetRequest(Index, Need)`; visual
review option 5 submits the same physical-need proposal for any resident. Neither
entry selects a construction recipe based on an object name. Requests for barrels,
cellars, furniture, rooms, vegetation, or previously unnamed objects use this
same entry. Option 4 retains the general world-host request path for rules and
other proposals. Submission itself does not make or place a model.

The default catalog is resolved from the tool file location to
`Art/Stage4HouseAudit/component-catalog.json`. It uses that catalog’s
`components[].component_asset_id`, `source_glb`, `units`, `authoring_axes`, and
`glb_to_authoring_coordinates` fields. Reusable candidates are chosen deterministically
from lexical matches in its GLB entries and remain unverified suggestions. No match
leaves an empty candidate list for catalog review; it does not establish that an
asset is missing. Unrelated walls are no longer supplied as a default for every request. Each
brief asks for a low-poly medieval result on the shared tiled UV/material convention,
reuses catalog pieces first, and requires a self-contained GLB with stable naming,
UV0, PBR materials, and the catalog’s recorded coordinate convention. The manifest
marks every job `proposed` and `requires_review: true`; it never claims approval.

The brief separates model delivery from runtime functionality. Unknown dimensions,
placement details, interaction points, capacity, resource costs, and capability
bindings must be resolved during design; the exporter does not invent them.
A cellar mesh alone does not establish underground navigation or food preservation.
Delivery must supply measured bounds and origin, reusable UV/material information,
collision and use-point information, an actual UE placement observation, and a
resident review. These are acceptance requirements, not completed workflow stages.
Job identity includes the world as well as the request to avoid cross-world reuse.

See [the universal NPC needs contract](Universal_NPC_Needs.md) for the intended
construction and use loop. This exporter produces review files; automatic modeling,
delivery registration, and functional acceptance are still separate implementation work.

Every job records the source board and catalog SHA-256, world/requester provenance,
and explicit `external_models_called: false` and `blender_launched: false` fields.
Writes use temporary files plus atomic replacement and always target the two fixed
filenames under `--output-dir`. Invalid input is rejected before writing; replacement
is atomic for each output file. The exporter does not read credentials or configuration.

Validate the bridge with:

```powershell
python -m unittest Plugins/ThreeHearths/Tools/test_hearth_art_jobs.py
```
