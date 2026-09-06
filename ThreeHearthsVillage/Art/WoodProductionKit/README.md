# WoodProductionKit

This kit fills the missing **in-process** visuals for the logs → planks / timber-beams production chain. Existing log packs, plank packs, finished beam packs, structural beams, carpenter bench, saw and axe already cover the other states; they are referenced directly and are not copied or modified.

Only two new originals were authored through the existing local Blender MCP server:

| New module | Width × depth × height (m) | Triangles | In-process state |
| --- | --- | ---: | --- |
| `wip_log_to_planks` | 0.740 × 0.226 × 0.226 | 484 | Two real longitudinal saw kerfs and one removed bark slab; the remaining base is connected. |
| `wip_log_to_beam` | 0.740 × 0.226 × 0.226 | 354 | Three flattened faces with an unfinished rough end and rounded underside. |

The new meshes use metres, +Z up and -Y front, with their long axis along +X and an exact bottom-centre origin. GLBs use standard +Y up / +Z front; convert to authoring coordinates as `(x, -z, y)`. JSON and custom GLB anchor metadata remain in authoring coordinates. `work_anchor_m` identifies a proposed working point; `support_anchor_m` is the resting origin.

Materials use exportable Principled base-color, roughness and metallic factors: warm chestnut bark, darker bark facets, light fresh oak and honey heartwood. They have UVs and no external images or procedural-only shader dependency. UVs are deterministic local planar coordinates for the solid-color palette, not a uniquely packed baking atlas. Both pieces together total **838 triangles**.

## Existing assets to reuse

The following dimensions are measured from the actual GLB bytes, including protrusions. Preserve each existing origin. `reuse-manifest.json` records the original path, material names, triangle count, origin floor offset and SHA-256 hash.

| Role | Existing asset | Measured size (m) | Triangles |
| --- | --- | --- | ---: |
| Raw log stock | `VillageKit/modules/carry_logs.glb` | 0.774 × 0.404 × 0.374 | 2,820 |
| Plank output stock | `SocietyKit/modules/goods_planks_bundle.glb` | 0.900 × 0.415 × 0.235 | 1,296 |
| Beam output stock | `SocietyKit/modules/goods_beams_bundle.glb` | 1.200 × 0.455 × 0.275 | 1,080 |
| Carpenter bench | `VillageKit/modules/workbench_carpenter.glb` | 1.600 × 0.969 × 0.980 | 1,552 |
| Saw | `ToolKit/modules/tool_saw.glb` | 0.697 × 0.057 × 0.196 | 736 |
| Axe | `ToolKit/modules/tool_axe.glb` | 0.298 × 0.068 × 0.755 | 1,176 |
| Smaller portable plank alternative | `VillageKit/modules/carry_planks.glb` | 0.780 × 0.371 × 0.246 | 1,296 |
| Installed structural beam counterpart | `VillageKit/modules/beam_timber_2m.glb` | 1.820 × 0.180 × 0.200 | 108 |

The existing carpenter bench includes a vise, board and mallet, so its complete envelope is larger than its nominal tabletop dimensions. The unobstructed front tabletop is at **Z = 0.86 m**. `production-layout.json` places each new workpiece at bench-local `(-0.27, -0.23, 0.86)` m; exported geometry checks verify support at three points along the piece. This keeps it away from the existing board and mallet. The nominal 2 m structural module has a 1.82 m visible beam for its original assembly spacing; do not equate its grid size with visible length.

Existing pack lengths differ. These visuals represent resource lots; they do not determine recipe amounts, yields or material conservation. The production simulation owns those rules. Neither new intermediate has an inventory identity, and showing one must not create stock. A production job should display its intermediate only while it owns/reserves the input, then remove or swap it when the job completes or cancels. These are integration suggestions, not implemented runtime behavior.

## Delivery and verification

- `WoodProductionKit.blend` contains only the two new source meshes plus linked display copies.
- `modules/` contains the two independent GLB files, each with one mesh and one identity-transform node.
- `module-specs.json`, `reuse-manifest.json` and `production-layout.json` define the new geometry, unchanged reuse references and static preview placements.
- `previews/WoodProduction_originals.png` is the actual close-up render of the two new pieces.
- `previews/WoodProduction_production.png` is a static illustration combining the new pieces with existing stock, benches and tools at authored scale. It is not a screenshot of running production gameplay.
- `model-report.json`, `source-audit.json`, `visual-review.json`, `validation.json` and `artifact-manifest.json` provide statistics, source audit, render review, independent checks and file hashes.

Run `validate_wood_production_kit.py` with ordinary Python in this project. It reuses the existing GLB byte-stream decoder and checks indices, nondegenerate triangles, UVs, normals, material factors, origins, work metadata, real kerf depth, flattened beam faces, original reference hashes and bench support. Existing asset files are read only. `write_reuse_manifest.py` inventories the source references and does not modify them.

To reproduce new geometry, execute `create_wood_production_kit.py` through Blender MCP. Execute `render_wood_production_kit.py` with `sys.argv = ['render_wood_production_kit.py', '--', 'originals']` and again with `['render_wood_production_kit.py', '--', 'production']`. MCP requests are in `mcp/`; temporary scenes and logs are in `Saved/ThreeHearths/WoodProductionKit/`. Cycles renders use CPU, three threads and 24 samples.

This kit does not modify UE, C++, inventory, recipes, animation, collision, existing art sources or Git state. Runtime attachment and work-animation behavior are not verified here.
