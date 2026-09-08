# Aincrad character materials

`Tools/create_aincrad_character_materials.py` creates and saves two native UE materials under `/Game/ThreeHearths/Materials/AincradCharacters`. It is safe to rerun: each graph is rebuilt in place, and no mesh, actor, world, post-process volume, or character color instance is changed.

`M_AnimeResident` remains the default Lit surface material for skeletal meshes. It uses very low specular and high roughness, then applies a restrained NdotL color band between `HearthTint` and `HearthShadowTint`. `HearthBandMix` controls the strength of that band and `HearthFill` supplies a small emissive fill so shadowed faces do not become unreadable. The material still receives the engine's normal direct lighting and shadowing; it is not an Unlit material.

`M_AnimeResidentOutline` is a post-process material. It reads the scene color, CustomStencil, CustomDepth, and SceneDepth. Only stencil value `7` contributes to the resident mask. The depth comparison suppresses outlines when CustomDepth is behind SceneDepth, and screen derivatives produce a thin approximately one-pixel edge. The original scene color is passed through unchanged apart from that outline; there is no full-screen posterize pass.

The parent integration script should assign `M_AnimeResident` to the skeletal mesh material slots named `AC_Skin`, `AC_Hair`, and related `AC_*` slots, then set per-character colors through material instances or dynamic instances. It should enable Custom Depth and Custom Stencil on the character primitive and write stencil `7`; this asset script intentionally does not touch those actor settings. The parent should add `M_AnimeResidentOutline` to the intended post-process volume.

All authored custom node inputs and material parameters use the `Hearth` prefix. The script was statically checked against the available UE 4.26 headers for `LightVector`, `SceneTexture`, `MD_PostProcess`, `BL_AfterTonemapping`, and the relevant scene texture IDs. It was not executed in UE 5.8 in this task, so the parent must perform the actual material compile and visual check in the target editor.
