# ResidentKit 01

16 original static accessories and 10 adult appearance proposals for the existing Cropout resident. These follow the VillageKit/SocietyKit palette: warm leather and wood, cream linen, muted sage/plum/ochre, blue cloth and small brass fasteners. Head forms are thick, softly rounded and deliberately simple at village camera distance.

`modules/` contains one independent GLB mesh per stable ID. `ResidentKit.blend` contains editable original accessory meshes and a display arrangement. It does not contain the Cropout body, skeleton or animation. `module-specs.json`, `model-report.json` and `artifact-manifest.json` are the installation contract, measured geometry report and frozen hashes.

## Actual reference and attachment basis

The fitting reference was exported from `/Game/Characters/Meshes/SKM_Villager` in this project with UE 5.8, then measured in Blender 5.2.0 LTS. Its height is 1.557142 m and its head width is 0.449723 m. This is the existing large-headed stylized character, so conventional real-human hat dimensions would be wrong. The source FBX, surface triangles and reference-containing fitting scenes remain under `Saved/ThreeHearths/ResidentKitReference`; they are not distributed as new original assets.

Each new GLB root is `(0,0,0)` at its intended reference-pose bone origin. The mesh is in metres, +Z up and -Y forward in Blender. glTF export performs the standard Y-up conversion. Geometry axes remain parallel to the body reference, rather than the bone's rotated local axes. `model-report.json` records bounds and the inverse reference bone rotation for every item. `attachment-reference.json` records the original measured bone matrices and positions.

| Bone | Measured reference origin in Blender metres | New items |
| --- | --- | --- |
| head | (0.000004, -0.016597, 1.272770) | 5 hairstyles, beard, headwrap, straw hat, soft merchant cap |
| neck | (0.000004, 0.004122, 1.153571) | red scarf |
| spine_02 | (0.000004, -0.002040, 0.807131) | satchel, backpack, linen apron, cape, shawl |
| pelvis | (0.000004, 0.002338, 0.492759) | double belt pouch |

For a Blender attachment, the reference world position is `T(bone origin) * accessory vertex`; parenting to the measured bone requires the inverse reference bone rotation supplied in the report. UE must convert both coordinates and that rotation to its imported mesh/skeleton basis. Do not paste the Blender matrix as a UE rotator or assume its axis order is identical. Existing `AHearthVillager.Body` also has relative yaw -90 degrees.

The current `AHearthVillager.Hat` is a **SkeletalMeshComponent using LeaderPose**. These GLBs are static meshes and must use new bone/socket attachments; hide the existing hat when applying one of these head looks. They cannot be passed to the existing skeletal-hat setter. Bone names have been verified against the actual source skeleton, but UE socket axis conversion, runtime attachment, walking/chopping/carrying intersections and animation binding have not been validated here. Torso garments are rigid accessory candidates, without cloth simulation or skin weights.

## Ten adult proposals

All initial people are adults. Ages and presentation describe appearance proposals; they do not modify the character skeleton, gameplay age or runtime identity. Clothing and skin colours in fitting renders are preview material assignments only. The body's original project materials were not changed.

| Proposal | Adult age / presentation | Visible distinction |
| --- | --- | --- |
| King | 56 / masculine | silver hair, short beard, blue cape, existing crown |
| Carpenter | 32 / feminine | cropped dark hair, utility pouches |
| Farmer | 27 / feminine | auburn braid, wide straw hat, linen apron |
| Mason | 48 / masculine | chestnut waves, red scarf, utility pouches |
| Smith | 41 / feminine | dark bun, red scarf, existing leather apron |
| Merchant | 35 / masculine | plum cap, leather crossbody satchel |
| Potter | 29 / feminine | sage headwrap, linen apron |
| Herbalist | 62 / feminine | silver hair, ochre shawl, satchel |
| Courier | 23 / masculine | chestnut waves, backpack and linen bedroll |
| Baker | 45 / feminine | dark bun, linen apron, utility pouches |

`looks.json` is the single source for these combinations. It reuses `SocietyKit/regalia_king_crown` and `profession_smith_apron` by path and fitting transforms; no mesh files were copied. The hammer and chisel are references only (`display_only: true`) until their grips/hand animation are tested. Do not stack arbitrary hats or back items: the explicit combinations choose at most one head cover and one back item.

## Geometry and fit checks

The hairstyles and headwrap use dense radial fitting to the actual saved head surface, with 26 mm outer-vertex clearance. The cloth shawl similarly follows the actual reference torso with 60 mm outer-vertex clearance around the raised sleeves. These allowances accommodate the simplified outer shells; they are not a guarantee for every triangle or animated pose. Hair caps, hats and cloth have physical shell thickness, valid UV0 and PBR materials. Cloth/hair roughness is 0.51–0.61; leather is 0.48–0.52; brass alone is metallic. Materials require no paid texture service or external texture downloads.

The exported GLBs are checked independently through `validate_resident_kit.py`: one mesh, UV0, unit normals, non-degenerate triangles and no more than 5,000 triangles per asset. Source bones, adult ages, IDs, mutually exclusive slots and old-asset references are also checked. `visual-review.json` records the actual images reviewed and their hashes. The fitting images show the existing body in a reference pose, with static attachment placement only.

## Reproduce

1. In this project, run `export_resident_reference.py` through UE Python commandlet with `-AllowCommandletRendering`, offline Hearth API config and remote Python disabled. Material baking is disabled. Do not use `-nullrhi` or `-NoShaderCompile`: UE's FBX exporter constructs material-baking mesh data even when baking is disabled.
2. Through Blender MCP `execute_blender_code`, run `inspect_reference.py`, then `create_resident_kit.py` with `runpy.run_path(..., run_name='__main__')`. Generation exports the 16 modules and original-only `.blend`; it does not render.
3. Run `preview_resident_kit.py` separately with `sys.argv=['preview_resident_kit.py','--','front']`, then `back`; run `preview_accessories.py` for the original-only contact sheet. Single renders are separate MCP calls.
4. Review the resulting pictures, update `visual-review.json` only after inspection, and run `python Art/ResidentKit/validate_resident_kit.py`. The validator writes the artifact manifest. A fresh Blender build can produce different binary hashes even with equivalent geometry, so byte-identical regeneration is not promised.

The original body is required only to regenerate fitting measurements and previews. Existing exported GLBs and the editable accessory-only `.blend` remain usable without the Saved reference files. Preview body geometry remains subject to the original Cropout project asset terms.

## Native UE art acceptance — 2026-09-06

The 16 frozen GLBs are now saved as native StaticMesh assets under `/Game/ThreeHearths/Generated/ResidentKit`, with their imported material instances. `UE_Import_Report.json` records each asset path and its measured native bounds, UV count and PBR values. Import uses `import_resident_kit.py` and the existing VillageKit importer with `bake_meshes=True`; no bounds-centre recentering is applied. The verified conversion is Blender `(x,y,z)` metres to UE `(100*x,-100*y,100*z)` centimetres. Both the nonzero bounds origin and extents match transformed source vertices within 0.15 cm. Material base colour, roughness and metallic values match the source factors, and UV0 remains present.

Native Nanite geometry preserves all 27,500 source triangles. The 10,858-triangle LOD0 value is the automatically generated fallback representation, recorded separately in the report. The independent source validation still checks UVs, normals and degenerate faces.

`/Game/ThreeHearths/Maps/L_ResidentKit_Showcase` is an isolated art map derived from the village island lighting, with its simulation actor removed. `previews/ResidentKit_InUE.png` is an actual 1920 × 1080 UE viewport capture using a 58-degree camera and exposure bias 0. The 16 accessories and the existing Cropout body reference appear at scale 1. Display actors are raised to the presentation floor; the imported attachment roots remain unchanged. The source island map and original character assets were not edited. `UE_Visual_Review.json` records the reviewed screenshot and native artifact hashes.

This acceptance covers native art import, dimensions, roots and material appearance. It does not validate skeletal attachment, animation, cloth behaviour or runtime look assignment. Reproduce captures with `capture_life_kits_showcase.py ResidentKit` from one dedicated offline editor launched with `-HearthApiConfig=".../Saved/ThreeHearths/import-preview/offline.json" -HearthNoWorldPersistence`; remote Python is enabled only for that editor. No live API or saved-world simulation is required.
