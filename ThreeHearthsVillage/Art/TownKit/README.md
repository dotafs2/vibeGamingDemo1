# TownKit connection pieces

TownKit fills the assembly gap between existing straight VillageKit/PublicWallKit bays and larger SocietyKit castle layouts. It adds six minimum site-installation units on the shared 2m grid:

- `town_corner_stone_2m`: orthogonal stone corner for continuous streets, courtyards and castle runs.
- `town_wall_gate_timber_2m`: real central opening that connects a shelter, room or courtyard without swapping a whole building asset.
- `town_roof_ridge_joint_2m`: roof junction that lets a plan extend across repeated bays.
- `town_stair_timber_2m`: one installable eight-step flight for upper rooms and wall walkways.
- `town_roof_valley_joint_2m`: orthogonal valley node for L/T-shaped roof extensions.
- `town_gable_end_timber_2m`: 2m end closure that lets roof bays terminate without a whole-building mesh.

The same existing families remain the source of the wider scale range: VillageKit supplies straight walls, doors/windows and roof slopes; PublicWallKit supplies the 2m authored anchor and PBR conventions; SocietyKit supplies the blue-grey castle stone precedent and larger wall/tower context. New geometry is limited to the connection pieces above so existing assets are not duplicated or modified.

Each GLB is one joined local mesh, unit scale, +Z-up / -Y-front, with explicit stock inputs and `npc_portable=false`. NPCs transport stock bundles; construction installs the piece at its socket. `attachment-sockets.json` records compatible existing modules and two cross-scale examples.

Runtime support is split deliberately. `town_corner_stone_2m`, `town_wall_gate_timber_2m`, `town_stair_timber_2m` and `town_gable_end_timber_2m` are candidates using the current stone/planks/beams stock. `town_roof_ridge_joint_2m` and `town_roof_valley_joint_2m` retain their real `tiles` inputs and are marked `blocked_on_tiles_runtime_resource`; the current C++ economy does not expose tiles, so these pieces are future material candidates rather than executable recipes.

Validation covers GLB structure, stable IDs, metadata, resource inputs and socket references. It does not claim Blender source/render evidence, UE import, collision, navigation or runtime acceptance; those remain follow-up work because no Blender executable/MCP or UE run is available in this lane.
