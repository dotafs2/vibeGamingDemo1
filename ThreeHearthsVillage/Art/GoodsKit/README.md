# GoodsKit

Eight original, separate commodity carry props for the Three Hearths village art set. Built through the existing local Blender MCP server on 6 September 2026, using Blender 5.2.0 LTS. The warm oak, honey end grain, cream ceramics, golden wicker, terracotta and slate blue palette follows VillageKit, SocietyKit and HomeLifeKit.

| Asset | `commodity_id` | Width × depth × height (m) | Triangles | Visual capacity suggestion |
| --- | --- | --- | ---: | --- |
| `goods_raw_clay_basket` | `raw_clay` | 0.528 × 0.450 × 0.363 | 1,324 | 4 modeled clay lumps |
| `goods_bricks_crate` | `bricks` | 0.634 × 0.411 × 0.301 | 1,936 | 12 fired bricks |
| `goods_tiles_terracotta_crate` | `tiles_terracotta` | 0.654 × 0.414 × 0.329 | 1,704 | 10 terracotta tiles |
| `goods_tiles_slateblue_crate` | `tiles_slateblue` | 0.654 × 0.414 × 0.329 | 1,704 | 10 slate blue tiles |
| `goods_lime_pail` | `lime` | 0.444 × 0.390 × 0.452 | 1,004 | 1 pail; no mass implied |
| `goods_pigment_pots` | `pigment` | 0.644 × 0.356 × 0.301 | 2,684 | 3 pots; colors are visual variants |
| `goods_iron_ingots_bundle` | `iron_ingots` | 0.640 × 0.434 × 0.319 | 456 | 12 ingots |
| `goods_nails_box` | `nails` | 0.484 × 0.380 × 0.301 | 1,764 | 13 individual nails plus a visual stock underlayer |

The quantities describe artwork. They are suggestions for later gameplay capacity design, not inventory values, recipes, masses or reservations. Each asset carries exactly one `commodity_id`; brick, terracotta tile and slate blue tile stock remain separate. Pigment pot colors do not encode additional stock types. Creating or displaying these meshes must not create stock.

## Files and coordinates

- `GoodsKit.blend` contains only original goods meshes, with hidden zero-origin source meshes and visible overview duplicates.
- `modules/` contains eight independent, self-contained GLB files, each with one mesh and one identity-transform node.
- `module-specs.json` defines measured envelopes, `carry_anchor`, left/right grip suggestions and portable metadata.
- `model-report.json` records Blender mesh counts; `validation.json` independently reads the exported GLB byte streams.
- `previews/GoodsKit_overview.png` and `previews/GoodsKit_scale.png` are actual Cycles CPU renders at three threads.
- `artifact-manifest.json` records delivered file sizes and SHA-256 hashes.

Authoring units are metres, +Z up and -Y front, with each origin at the exact bottom centre of its mesh envelope. GLBs use standard glTF +Y up / +Z front; convert positions back to authoring coordinates as `(x, -z, y)`. Custom metadata and JSON anchor/grip positions remain in the documented authoring coordinates; they are not silently converted to glTF coordinates.

`carry_anchor.position_m` is the local point proposed for alignment to a resident carry target. A starting target for the measured 1.557142 m resident is `(0, -0.60, 0.86)` m relative to a ground-centred, -Y-facing resident. This places the pack ahead of the reference torso envelope. At zero relative rotation, mesh translation is `target - carry_anchor`. Left/right grip points are local visual placement suggestions, not exported armature sockets. Use a simple convex collision hull for a carried pack and account for it during navigation.

No UE import, inventory logic, crafting recipe, animation, IK, socket binding or live carry action has been implemented or verified by this kit. The scale image shows the existing Cropout body in its reference pose, next to the goods at actual scale; it does not show a verified holding pose. The licensed Cropout source and reference-bearing render `.blend` files remain only under `Saved/ThreeHearths/`; no Cropout FBX or character geometry is included in the kit source or GLBs.

## Reproduction and checks

Execute `create_goods_kit.py` with the existing Blender MCP `execute_blender_code` tool. Then execute `render_goods_kit.py` twice, with `sys.argv = ['render_goods_kit.py', '--', 'overview']` and `['render_goods_kit.py', '--', 'scale']`. The MCP request JSON files are in `mcp/`. The scale view reads the already available `Saved/ThreeHearths/ResidentKitReference/SKM_Villager.fbx`.

Run `validate_goods_kit.py` with ordinary Python from this project. It reuses the project GLB binary inspection routine and additionally checks the eight unique commodity IDs, carry metadata, exact bottom-centre origins, carry envelopes, material factors, nonconstant UV coordinates and a 3,000-triangle per-asset limit. Every material uses exportable Principled base-color, roughness and metallic factors, with no procedural-only shader or external texture dependency. UVs are deterministic local planar coordinates intended for these solid-color materials; they are not a uniquely packed texture-baking atlas.
