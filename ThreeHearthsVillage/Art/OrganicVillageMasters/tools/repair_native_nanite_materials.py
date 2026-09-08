"""Bounded repair for generated OrganicVillageMasters static meshes.

The imported GLBs are small modular runtime pieces.  Interchange enabled
Nanite on these meshes, while their generated material instances were not
compiled with Nanite usage.  Nanite is unnecessary for these low-poly layers,
so this script disables Nanite only on the 114 generated organic meshes.  It
does not touch source materials, engine/plugin assets, catalog data, or any
asset outside the generated OrganicVillageMasters branch.

Use ``-HearthOrganicNaniteVerifyOnly`` for a cold-read check without saves.
"""
from __future__ import annotations

import json
from pathlib import Path

import unreal as ue


DEST = "/Game/ThreeHearths/Generated/OrganicVillageMasters"
ROOT = Path(__file__).resolve().parents[3]
REPORT = ROOT / "Art" / "OrganicVillageMasters" / "RuntimeLayers" / "organic_nanite_repair_report.json"


def generated_meshes() -> list[ue.StaticMesh]:
    paths = ue.EditorAssetLibrary.list_assets(DEST, True, False)
    meshes = []
    for path in paths:
        asset = ue.load_asset(path)
        if isinstance(asset, ue.StaticMesh):
            meshes.append(asset)
    if len(meshes) != 114:
        raise RuntimeError(f"Expected 114 generated StaticMeshes, found {len(meshes)}")
    return sorted(meshes, key=lambda mesh: mesh.get_path_name())


def inspect(meshes: list[ue.StaticMesh], save: bool) -> dict:
    subsystem = ue.get_editor_subsystem(ue.EditorAssetSubsystem)
    changed = 0
    enabled_before = 0
    slots_checked = 0
    missing_materials = []
    for mesh in meshes:
        settings = mesh.get_editor_property("nanite_settings")
        enabled = bool(settings.get_editor_property("enabled"))
        enabled_before += int(enabled)
        if enabled:
            if save:
                settings.set_editor_property("enabled", False)
                mesh.set_editor_property("nanite_settings", settings)
                changed += 1
        slots = mesh.get_editor_property("static_materials")
        for index, slot in enumerate(slots):
            slots_checked += 1
            if not slot.get_editor_property("material_interface"):
                missing_materials.append(f"{mesh.get_path_name()}[{index}]")
        if save and enabled:
            if not subsystem.save_loaded_asset(mesh, False):
                raise RuntimeError("Could not save " + mesh.get_path_name())
    if missing_materials:
        raise RuntimeError("Missing generated material slots: " + ", ".join(missing_materials))
    if not save and enabled_before:
        raise RuntimeError(
            f"Cold-read found {enabled_before} meshes with Nanite still enabled")
    # Re-read the values after mutation.  Verify-only never mutates the loaded
    # assets, so this is also a strict persisted-state check.
    enabled_after = 0
    for mesh in meshes:
        enabled_after += int(bool(mesh.get_editor_property("nanite_settings").get_editor_property("enabled")))
    result = {
        "status": "passed",
        "destination": DEST,
        "mesh_count": len(meshes),
        "nanite_enabled_before": enabled_before,
        "nanite_disabled": changed,
        "nanite_enabled_after": enabled_after,
        "material_slots_checked": slots_checked,
        "missing_material_slots": len(missing_materials),
        "policy": "Nanite disabled for low-poly modular runtime layers; no source or engine/plugin assets changed",
    }
    return result


def main() -> None:
    _, switches, _ = ue.SystemLibrary.parse_command_line(ue.SystemLibrary.get_command_line())
    verify_only = "HearthOrganicNaniteVerifyOnly" in switches
    result = inspect(generated_meshes(), save=not verify_only)
    if not verify_only:
        REPORT.parent.mkdir(parents=True, exist_ok=True)
        REPORT.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print("[OrganicNaniteRepair] " + json.dumps(result, sort_keys=True))


if __name__ == "__main__":
    main()
