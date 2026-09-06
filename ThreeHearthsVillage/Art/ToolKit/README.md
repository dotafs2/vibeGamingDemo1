# ToolKit

Eight original tools for NPC carry and individual work actions, authored through the existing local Blender MCP server with Blender 5.2.0 LTS. The set uses the VillageKit/HomeLifeKit/GoodsKit warm oak, honey wood, cream grip wrap and forged-iron palette, with lightly rounded edges and brighter metal cutting surfaces.

| Asset | Single `tool_id` | Width × depth × height (m) | Triangles |
| --- | --- | --- | ---: |
| `tool_hammer` | `hammer` | 0.276 × 0.080 × 0.447 | 1,228 |
| `tool_mallet` | `mallet` | 0.264 × 0.127 × 0.476 | 1,124 |
| `tool_axe` | `axe` | 0.298 × 0.068 × 0.755 | 1,176 |
| `tool_saw` | `saw` | 0.697 × 0.057 × 0.196 | 736 |
| `tool_pickaxe` | `pickaxe` | 0.536 × 0.076 × 0.858 | 1,160 |
| `tool_shovel` | `shovel` | 0.274 × 0.068 × 1.064 | 1,284 |
| `tool_hoe` | `hoe` | 0.283 × 0.264 × 0.893 | 1,148 |
| `tool_trowel` | `trowel` | 0.174 × 0.068 × 0.386 | 796 |

The hand saw has a genuine open handle and geometric teeth. The axe has a broad curved cutting edge, the pickaxe has two distinct working ends, the shovel has shoulder steps and a raised rib, and the hoe has a forward-projecting working blade. Each exported module is one tool mesh with one identity and its own anchors.

## Delivery and coordinates

- `ToolKit.blend`: eight original source meshes at their individual local origins, plus linked display copies. No character geometry or external references are included.
- `modules/`: eight self-contained GLB files, each with one mesh and one identity-transform node.
- `module-specs.json`: measured dimensions, primary `grip_anchor`, working tip and working-axis suggestions. Axe, pickaxe, shovel and hoe also have a secondary grip proposal.
- `previews/ToolKit_overview.png`: actual Cycles CPU render of all tools at their authored scale.
- `previews/ToolKit_grips.png`: actual render with primary grip (blue), secondary grip (cream) and working tip (amber) markers. Markers are presentation geometry and are excluded from the GLBs.
- `model-report.json`, `validation.json`, `source-audit.json`, `visual-review.json` and `artifact-manifest.json`: model statistics, independent exported-byte validation, original-source audit, render review and delivery hashes.

All tools are authored in metres, +Z up and -Y front, with an exact bottom-centre origin. Standard glTF export uses +Y up / +Z front. Convert glTF positions back to authoring coordinates as `(x, -z, y)`. JSON and custom GLB metadata always use the documented authoring coordinates.

The long handles point along authored +Z; their working heads are above the grip. The saw blade points primarily along +X. To place a tool at a hand target with a chosen rotation `R`, use `translation = hand_target - R * grip_anchor`. Tool length is intended for the existing approximately 1.557 m Cropout resident; the supplied anchors are starting points for attachment and animation authoring, not a verified grip pose. Preserve the asset scale and choose the per-action rotation explicitly.

No tool inventory, crafting recipe, UE import, socket, skeleton binding, IK, collision behavior or live work animation is created or verified by this kit. Geometry alone must not create tool stock or trigger a work action. The working tip and axis proposals do not define an enabled hit volume.

## Reproduction and independent checks

Execute `create_tool_kit.py` with the local Blender MCP `execute_blender_code` tool. Execute `render_tool_kit.py` with `sys.argv = ['render_tool_kit.py', '--', 'overview']` and again with `['render_tool_kit.py', '--', 'grips']`. Request JSON files are in `mcp/`; render scene files and session logs remain under `Saved/ThreeHearths/ToolKit/`. Cycles uses CPU, three threads and 24 samples.

Run `validate_tool_kit.py` using ordinary Python from this project. It independently decodes the GLB buffers and reuses the existing project asset validator to check indices, nondegenerate triangles, UVs, unit normals, portable PBR parameters, dimensions, origins, single-tool identities, anchors and the 3,000-triangle per-module budget. It also intersects exported geometry through every primary and secondary grip to confirm that the anchor lies within a 15–120 mm handle section. This checks geometric placement and does not verify hand posing. Materials have no external images or procedural-only shader dependency. UVs are deterministic local planar coordinates for solid-color materials; they are not a uniquely packed baking atlas.
