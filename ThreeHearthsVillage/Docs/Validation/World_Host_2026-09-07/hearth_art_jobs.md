# Hearth art jobs: `F7E68E06-453F-3D13-4821-20824BAE682F`

Offline review manifest; no model provider or Blender execution is implied.

## art-request_1 — proposed / requires review

**Objective:** 需要一个纯视觉的铁匠铺招牌模型
**Resolution:** 记录为视觉资产提议；不会据此声称家族系统或其他玩法已实现。
**Style:** low\-poly medieval village; shared tiled UV; reuse the existing catalog first
**Coordinates:** `\{"authoring\_axes": "\+Z up / \-Y front", "glb\_to\_authoring\_coordinates": "\(x,\-z,y\)", "units": "metres"\}`

Reusable catalog candidates:
- floor\_timber\_2m — \.\./VillageKit/modules/floor\_timber\_2m\.glb
- foundation\_stone\_2m — \.\./VillageKit/modules/foundation\_stone\_2m\.glb

Acceptance:
- GLB opens without missing external textures or dependencies.
- Grid scale and authoring coordinate convention match the catalog metadata above.
- UVs are shared/reusable and the result stays within the request scope.

Failure isolation: Keep this asset in its own source/export folder; a failed job must not alter existing catalog files.
Provenance: board `1e580e11ac8e`; catalog `e6536d920159`.
