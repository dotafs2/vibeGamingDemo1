"""Read-only UE reflection probe for MedievalLife material-instance overrides.

This intentionally does not mutate or save any asset.  Run it in the same
headless editor environment as the importer when checking a banner import:
the output lists the reflected override names on the first banner material
instance found under the MedievalLife generated directory.
"""
import json

import unreal as ue


DEST = "/Game/ThreeHearths/Generated/MedievalLife"


def main() -> None:
    registry = ue.AssetRegistryHelpers.get_asset_registry()
    registry.scan_paths_synchronous([DEST], True)
    assets = registry.get_assets_by_path(DEST, recursive=True)
    for data in assets:
        asset = data.get_asset()
        if not isinstance(asset, ue.StaticMesh):
            continue
        if "royal_banner" not in asset.get_path_name().lower():
            continue
        for slot in asset.get_editor_property("static_materials"):
            material = slot.get_editor_property("material_interface")
            if not isinstance(material, ue.MaterialInstanceConstant):
                continue
            overrides = material.get_editor_property("base_property_overrides")
            reflected = sorted(name for name in dir(overrides)
                               if any(token in name.lower()
                                      for token in ("override", "blend", "two_sided")))
            candidates = {}
            for name in ("override_blend_mode", "b_override_blend_mode",
                         "override_two_sided", "b_override_two_sided",
                         "blend_mode", "two_sided"):
                try:
                    candidates[name] = repr(overrides.get_editor_property(name))
                except Exception as exc:
                    candidates[name] = type(exc).__name__ + ": " + str(exc)
            ue.log("[MedievalLifeMaterialProbe] " + json.dumps({
                "material": material.get_path_name(),
                "overrides_type": type(overrides).__name__,
                "reflected_names": reflected,
                "candidate_results": candidates,
            }, ensure_ascii=False, sort_keys=True))
            return
    raise RuntimeError("No royal_banner MaterialInstanceConstant found under " + DEST)


if __name__ == "__main__":
    main()
