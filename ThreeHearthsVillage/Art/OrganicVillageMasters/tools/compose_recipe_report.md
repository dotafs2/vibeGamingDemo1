# Compose recipe verification

The composer was corrected so every piece first receives its recipe palette on every layer, including structure and attachments. An explicit `--finish-palette` then performs a second pass on finish/weathering only. Blender 5.2.1 regenerated the validation outputs under `Art/OrganicVillageMasters/Composed/`; `Modules/` and `Assemblies/` were not written.

The three no-override compositions match their authored assembly GLBs for measured vertex bounds and per-layer semantic material BaseColor sets:

| Recipe | Composed output | Bounds (X/Y/Z m) | Layer colors |
|---|---|---|---|
| `carpenter_court` | `carpenter_court_composed.glb` | `8.16391 / 5.525 / 7.7675` | match |
| `family_cluster` | `family_cluster_default.glb` | `9.34362 / 6.27 / 7.5625` | match |
| `merchant_steps` | `merchant_steps_default.glb` | `7.5475 / 7.375 / 8.035` | match |

The override case uses a genuinely different palette: `family_cluster_finish_ochre.glb` uses `--finish-palette ochre_slate` over the recipe's `sage_clay` baseline. Comparing it with `family_cluster_default.glb` shows `structure` and `attachments` material BaseColor sets unchanged, while `finish` and `weathering` sets change. Geometry and measured bounds remain unchanged by the material-only pass.

The disabled-layer case `merchant_steps_no_extras.glb` removes 21 mesh objects from weathering/attachments and exports only `finish` and `structure` layers. It still preserves the piece/module metadata and anchor nodes for the remaining hierarchy.

Current output hashes:

| File | Bytes | SHA-256 |
|---|---:|---|
| `carpenter_court_composed.glb` | 13,112,420 | `56bf56f2752b8219ae75f3d804d9ff4e1e33103a5324f04bde57e131660cea5f` |
| `family_cluster_default.glb` | 15,017,912 | `d4f443e77119c84800764f4104f5d25afa748a41e787dfe549586bf122c531a7` |
| `family_cluster_finish_ochre.glb` | 15,018,272 | `6411af2c405d19c60f0c14b192504f1dffa2fb884841cf7fa5ab95786d641db8` |
| `merchant_steps_default.glb` | 13,296,820 | `f7d71359fb61fea7e07aeff50fd6a40a0cc47fb6b3f1c1ce9c2fbc6676ce77d1` |
| `merchant_steps_no_extras.glb` | 10,821,396 | `147fcf01f2726e21c10ea9a36a2af17525f855abec2e03a9c0e56872c026e232` |
| `family_side_wing_composed.glb` | 9,461,228 | `f636da775e762672658d61ab26b825f246070f752ce918e373989496e56422ff` |

The composer passes `py_compile`, the protected-output error test, and the invalid-layer error test. Source sentinels remain unchanged: `Modules/wall_plain_2m.glb` = `09065c900b91f6caa03c0be71b767ec9530f3ad28a0fb7bd32cc5b8637282f10`; `Assemblies/family_cluster.glb` = `1f0f3da129e46f41c669dc18b978d2ebd683502dc9e5ba4243ca3d1b2f904211`.

The merchant pair was regenerated after the latest awning source fix. `merchant_steps_default.glb` matches the latest `Assemblies/merchant_steps.glb` at bounds size `[7.5475, 7.375, 8.035]`, 189,296 triangles, and all four per-layer semantic material color sets. The optional-layer output remains at 21 removed mesh objects with only `finish` and `structure` layers.

Growth recipe checks:

- `family_starter -> family_side_wing`: 35 old keys = 32 retain + 3 dismantle; 50 new keys = 32 retain + 18 add. All sets are disjoint where required.
- `family_side_wing -> family_cluster`: 50 old keys = 44 retain + 6 dismantle; 86 new keys = 44 retain + 42 add. All sets are disjoint where required.

`family_side_wing_composed.glb` was composed from the new recipe. It contains exactly 50/50 expected `instance_key` values, one recipe root, and all four layers. Its measured glTF vertex bounds are min `[-2.308622, 0, -4.497500]`, max `[4.497500, 5.075000, 2.215000]`, size `[6.806122, 5.075000, 6.712500]`, matching the recipe bounds after the expected glTF Y-up axis conversion. All ten palette-mapped semantic slots present in the output (`plaster`, `plaster_patch`, `accent`, `roof_0..3`, `wood`, `wood_light`, `wood_dark`) match the `sage_clay` BaseColor values exactly. Output SHA-256: `f636da775e762672658d61ab26b825f246070f752ce918e373989496e56422ff` (9,461,228 bytes).
