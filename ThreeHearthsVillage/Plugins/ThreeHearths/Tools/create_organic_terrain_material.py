"""Create the native vertex-color terrain material used by OrganicTerrain.

This script is intentionally idempotent.  It creates or rebuilds exactly one
material at ``/Game/ThreeHearths/Generated/OrganicVillageMasters/M_OrganicTerrain``.
The Base Color pin is driven by Vertex Color RGB and Roughness is a constant
0.95.  Run headless with UnrealEditor-Cmd and the Hearth flags used by the
project; no mesh, catalog, or runtime asset is touched.
"""
from __future__ import annotations

import unreal as ue


MATERIAL_PATH = "/Game/ThreeHearths/Generated/OrganicVillageMasters/M_OrganicTerrain"
PACKAGE_PATH = "/Game/ThreeHearths/Generated/OrganicVillageMasters"
ASSET_NAME = "M_OrganicTerrain"


def make_material(save: bool = True) -> ue.Material:
    material = ue.load_asset(MATERIAL_PATH)
    if material is not None and not isinstance(material, ue.Material):
        raise RuntimeError("Existing asset is not a Material: " + MATERIAL_PATH)
    if material is None:
        tools = ue.AssetToolsHelpers.get_asset_tools()
        material = tools.create_asset(
            ASSET_NAME, PACKAGE_PATH, ue.Material, ue.MaterialFactoryNew())
        if material is None:
            raise RuntimeError("Could not create " + MATERIAL_PATH)

    editing = ue.MaterialEditingLibrary
    editing.delete_all_material_expressions(material)
    vertex_color = editing.create_material_expression(
        material, ue.MaterialExpressionVertexColor, -420, 0)
    roughness = editing.create_material_expression(
        material, ue.MaterialExpressionConstant, -420, 180)
    roughness.set_editor_property("r", 0.95)
    material.set_editor_property("two_sided", False)
    # UE Python returns None for successful connect calls in some 5.8 builds,
    # so verification below inspects the resulting expression graph instead
    # of treating the return value as a boolean.
    editing.connect_material_property(
        vertex_color, "", ue.MaterialProperty.MP_BASE_COLOR)
    editing.connect_material_property(
        roughness, "", ue.MaterialProperty.MP_ROUGHNESS)
    editing.layout_material_expressions(material)
    editing.recompile_material(material)
    if save and not ue.EditorAssetLibrary.save_loaded_asset(material, False):
        raise RuntimeError("Could not save " + MATERIAL_PATH)
    return material


def verify(material: ue.Material) -> dict:
    expressions = ue.MaterialEditingLibrary.get_material_expressions(material)
    vertex = [e for e in expressions
              if isinstance(e, ue.MaterialExpressionVertexColor)]
    constants = [e for e in expressions
                 if isinstance(e, ue.MaterialExpressionConstant)]
    if len(vertex) != 1 or len(constants) != 1:
        raise RuntimeError("Unexpected terrain material expression graph")
    if abs(constants[0].get_editor_property("r") - 0.95) > 1e-6:
        raise RuntimeError("Terrain roughness is not 0.95")
    base_node = ue.MaterialEditingLibrary.get_material_property_input_node(
        material, ue.MaterialProperty.MP_BASE_COLOR)
    rough_node = ue.MaterialEditingLibrary.get_material_property_input_node(
        material, ue.MaterialProperty.MP_ROUGHNESS)
    if not isinstance(base_node, ue.MaterialExpressionVertexColor):
        raise RuntimeError("Base Color is not connected to Vertex Color")
    if not isinstance(rough_node, ue.MaterialExpressionConstant):
        raise RuntimeError("Roughness is not connected to a constant")
    if ue.MaterialEditingLibrary.get_material_property_input_node(
            material, ue.MaterialProperty.MP_WORLD_POSITION_OFFSET) is not None:
        raise RuntimeError("Terrain material unexpectedly uses WPO")
    return {
        "material": material.get_path_name(),
        "vertex_color_expressions": len(vertex),
        "roughness_constants": [constants[0].get_editor_property("r")],
        "base_color_source": "VertexColor.Color (RGB)",
        "roughness": 0.95,
        "two_sided": material.get_editor_property("two_sided"),
        "wpo": "unconnected",
    }


def main() -> None:
    _, switches, _ = ue.SystemLibrary.parse_command_line(
        ue.SystemLibrary.get_command_line())
    if "HearthOrganicTerrainColdRead" in switches:
        material = ue.load_asset(MATERIAL_PATH)
        if material is None:
            raise RuntimeError("Cold read could not load " + MATERIAL_PATH)
        print("[OrganicTerrainMaterial] cold-read verified " + str(verify(material)))
        return
    save = "HearthOrganicTerrainVerifyOnly" not in switches
    material = make_material(save=save)
    details = verify(material)
    if save:
        print("[OrganicTerrainMaterial] saved and verified " + str(details))
    else:
        print("[OrganicTerrainMaterial] verify-only (unsaved) " + str(details))


if __name__ == "__main__":
    main()
