# Recipe composer

`compose_recipe.py` turns an existing `Recipes/*.json` assembly recipe into a new composed GLB and Blender source file. It imports each module named by `pieces`, applies only the recipe `translation_m` and `yaw_degrees`, and keeps the module hierarchy, mesh `layer` properties, and anchor empties from the source GLB. No source module or existing assembly is edited.

Run from Blender 5.2.1 or newer:

```text
blender --background --python tools/compose_recipe.py -- \
  --recipe Recipes/family_cluster.json \
  --output Composed/family_cluster_preview.glb \
  --finish-palette ochre_slate \
  --disable-layer weathering \
  --disable-layer attachments
```

`--output` may end in `.glb` or `.blend`; the composer writes both formats beside the requested path. The target must remain outside `Modules/` and `Assemblies/`. Each piece palette is first applied to every layer, so a sage or slate recipe also receives its authored structural wood and attachment tones. `--finish-palette` is an optional second pass for material slots on `finish` and `weathering` meshes only; it leaves the already-resolved `structure` and `attachments` colors unchanged. Valid palette names are read from `catalog.json`.

The only disableable layers are `weathering` and `attachments`. Removing a layer removes its mesh objects while preserving the remaining source hierarchy and anchors. Invalid module IDs, missing files, unknown layers/palettes, non-finite translations/yaws, malformed recipes, and protected output paths fail before Blender imports any module.

Every recipe piece must carry a unique 20-character lowercase hexadecimal `instance_key`. The key is used in the composed piece root name and is preserved as root custom property metadata; array position is never used as the persistent identity. Outputs carry root and piece custom properties for the recipe, module IDs, instance keys, transforms, palette selections, requested layers, and disabled layers. The GLB is intended for native Unreal import by the existing project workflow; the companion `.blend` is the reviewable Blender source for that composed result.
