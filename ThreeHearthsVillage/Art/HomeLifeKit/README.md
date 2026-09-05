# HomeLifeKit 01

12 original living and social components in the VillageKit/SocietyKit style: warm rounded oak, cream linen, sage/blue bedding, glazed food bowls, woven baskets and muted berries. The kit includes editable Blender source, independent GLBs, a 4×4 m living corner and a ten-seat communal meal area. No paid generation service, external textures or copied Cropout character mesh was used.

## Scale and installation

The existing Cropout adult is 1.557142 m tall. Its measured pelvis-weighted region is 0.803452 m wide and 0.673079 m deep. These measurements were read from the previously exported project reference, not estimated from a conventional human model. Seats therefore have a 0.90×0.72 m surface; a narrow 0.45 m real-world chair would intersect this character's hips.

| Module | Intended result | Primary dimensions / support |
| --- | --- | --- |
| bed_single_1_1x2m | installed single bed | 1.1×2.0 m footprint; 1.0×1.82 m sleep region |
| bed_double_2x2m | installed double bed | 2.0×2.0 m footprint; two 0.91×1.82 m sleep regions |
| chair_oak_wide | installed chair | 0.90×0.72 m seat, Z=0.45 m |
| bench_backed_1_8m | installed two-person backed bench | two seats 0.90 m apart, Z=0.45 m |
| table_dining_1_2m | installed dining table | 1.2×0.8 m top, Z=0.78 m |
| table_communal_2_6m | installed trestle table | 2.6×1.1 m top, Z=0.78 m |
| food_tray_bread | existing meal inventory display | tray, bowl/stew surface and two bread rolls |
| basket_berries | existing berry inventory display | filled woven basket with two grips |
| basket_empty | owned carry container | physically hollow basket with two grips |
| grain_chest | installed storage furniture | approximately 1.0×0.58 m; initial stock is zero |
| firewood_stack | existing wood inventory display | low log pile, approximately 0.85×0.52 m |
| fence_low_2m | installed low boundary | 2 m pitch, two inset end posts |

Furniture roots use floor centre at Z=0. Carryable display meshes use the centre of their bottom support plane. All coordinates in `module-specs.json` are Blender metres, +Z up and -Y forward. Yaw=0 faces -Y; positive yaw rotates around +Z. GLB export converts to standard glTF Y-up, with no residual node transform in the independent modules. Real bounds and material names are recorded in `model-report.json`.

The single bed has approximately 1 m of mattress width. The double bed allocates 0.91 m per person; bed support points are on the quilt at Z=0.505 m. The single-bed pillow top reference is Z=0.60 m. Bed head direction is +Y. These describe support regions, not an already working lie-down animation.

Each fence owns both end posts, inset by half their width. Placing sections every 2 m creates paired posts at joins without overlapping post volumes. It is a decorative/collision candidate, without gate-opening behaviour.

## Interaction contract

`module-specs.json` separates the following local points:

- `approach_ground`: a ground destination with a 0.45 m standing-clearance radius and 1.56 m height allowance.
- `sitting_contact`: the furniture's physical seat surface.
- `pelvis_pose_candidate`: the seat point plus 0.22 m, using the measured pelvis-to-hip-bottom offset. This is a future pelvis-bone alignment target, not a character root transform.
- `lying_support` and `head_support`: mattress/quilt and pillow references with explicit usable dimensions.
- `place_surface`: the real tabletop height and usable rectangle.
- `carry_grip`: an attachment/grip reference on an owned item.
- `inventory_transfer`: a visual access point for an existing inventory relationship.

An object's point transforms with the object's placement: rotate its local position by the placement yaw, then add the placement position. Combine local and placement yaw for orientation. Sitting, lying and carrying still need appropriate UE animations, IK, capsule changes and collision policy. No animation, navigation or resource-system integration is claimed by this art kit.

The render's blue rings show the standing regions and the cream seat dots show sitting contacts. The ruler marks the actual resident height. These presentation objects are excluded from the exported example GLBs. Overlapping approach regions are recorded in `validation.json` as pairs requiring mutually exclusive reservations. A clear destination does not authorize two NPCs to occupy the same approach region simultaneously.

## Inventory rules

Every module declares `creates_inventory: false`. Beds, chairs, tables, chests and fences are installed results that require construction or authorized placement of an already owned object. The empty basket also requires an owned container; no carrying capacity implementation is supplied.

Food trays, full berry baskets and woodpiles visualize existing authoritative inventory records. Rendering a berry or log never grants that resource, and the number of visible pieces is not the stock quantity. Spawning these meshes must not add food or wood. The grain chest begins with zero stock. `authoritative_inventory_required` and `visual_quantity_is_not_stock` express these distinctions in machine-readable form.

## Layouts and checks

`example-layouts.json` is the single placement source used by rendering and validation. The cabin uses one bed, one dining seat, a compact table, storage chest and empty basket. Its central nominal furniture gap is 1.05 m. The left and back cutaway walls, columns and floor are referenced from VillageKit, without copying standalone module files into this kit. The full reference shell is shifted down 0.16 m so VillageKit's finished floor and the new furniture floor datum both equal Z=0.

The public meal corner has ten seats: four two-person benches and two wide chairs. It uses two tables. The two table-end approach points blocked by the end chairs are explicitly disabled in the layout; the inner service points remain available. The woodpile sits in an outer corner so it does not cut off the route to the rear seating.

`validate_home_life_kit.py` independently reads the GLB byte streams and checks:

- One mesh per module, UV0, normalized normals, non-degenerate triangles and a maximum of 8,000 triangles per item.
- Actual exported bounds, floor origins and seat/bed/table support heights sampled from triangles.
- Five vertical probes through the empty basket's mouth reach only its bottom, proving it has no false cap across the opening.
- Furniture footprints remain within the example areas and do not overlap; tabletop displays remain supported.
- All enabled standing circles avoid the furniture, with a conservative 5 cm 2D clearance-grid route from the example entry to each approach.
- Seat count, exported example instance positions/rotations and JSON placement agreement.

The 2D check uses exported furniture bounds and a standing radius; it is not UE NavMesh testing and does not simulate sitting manoeuvres or body animation. The cutaway leaves the front/right walls open for review. A complete house, door opening, live reservations and animated motion still need integration checks. The assembly is an art/layout candidate, not a claim that NPC life actions are implemented.

## Reproduction and files

1. Run `python Art/HomeLifeKit/write_contracts.py` to write the stable specifications and layouts.
2. Execute `create_home_life_kit.py` through Blender MCP using `runpy.run_path(..., run_name='__main__')`. It generates all GLBs and the original-only `HomeLifeKit.blend`, without rendering.
3. Execute `preview_home_life_kit.py` separately with `sys.argv=['preview_home_life_kit.py','--',VIEW]`. Views are `overview`, `cabin_living_4x4m` and `common_meal_10_seats`. Each render is a separate MCP call. Request JSON is retained in `mcp/`; local logs and Blender backup files are ignored.
4. Inspect the three PNGs, update the visual-review record, then run `python Art/HomeLifeKit/validate_home_life_kit.py`. This writes `validation.json` and the SHA-256 artifact manifest.

`HomeLifeKit.blend` contains only original furniture/item source meshes. `examples/` contains editable presentation scenes and assembled geometry GLBs; the cabin includes our previously made VillageKit reference shell. The scratch scene is not a deliverable. The independent `modules/` GLBs require no old kit to load. Regeneration is procedural and repeatable, but Blender binary byte identity across versions is not guaranteed.

## Native UE art acceptance — 2026-09-06

The 12 modules and two assembled examples are saved as native StaticMesh assets under `/Game/ThreeHearths/Generated/HomeLifeKit`. `UE_Import_Report.json` records all 14 asset paths, native bounds, UV count and material factors. `import_home_life_kit.py` uses the existing VillageKit import pipeline and the ResidentKit native audit. Blender `(x,y,z)` metres converts to UE `(100*x,-100*y,100*z)` centimetres; measured bounds origins and extents agree with transformed source vertices within 0.15 cm, without recentering. Base colour, roughness and metallic values match source PBR factors, and UV0 is present throughout.

Native Nanite meshes preserve all 103,040 source triangles across modules and the two examples. The modules alone remain 34,588 triangles; example totals include their repeated furniture and the cabin's VillageKit shell. UE's separate LOD0 fallback totals 23,784 triangles and is not the full Nanite geometry. Source normals, degeneracy, support heights and approach routes remain covered by the independent GLB validator.

`/Game/ThreeHearths/Maps/L_HomeLifeKit_Showcase` is an independent review map using copied village island lighting, with its simulation actor removed. The three actual UE captures are `previews/HomeLifeKit_InUE.png`, `HomeLifeKit_cabin_InUE.png` and `HomeLifeKit_meal_InUE.png`, all 1920 × 1080. Their camera FOVs are 58, 52 and 55 degrees respectively, with exposure bias 0. The open cabin interior and the ten-seat meal arrangement were inspected for scale, visible openings, table support and furniture intersections. Assets are displayed at scale 1. `UE_Visual_Review.json` records the images and native artifact hashes.

The review actors have collision disabled; seating/lying animation, actual UE navigation and live interaction reservations are still untested. These static assets grant no inventory and activate no life actions. Reproduce captures with `capture_life_kits_showcase.py HomeLifeKit [overview|cabin|meal]` from one dedicated offline editor launched with `-HearthApiConfig=".../Saved/ThreeHearths/import-preview/offline.json" -HearthNoWorldPersistence`; only its local remote-Python endpoint is needed. The main map, saved world and original project assets were not modified.
