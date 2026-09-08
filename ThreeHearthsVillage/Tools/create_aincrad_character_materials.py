"""Create the native Aincrad resident material pair.

This script only owns /Game/ThreeHearths/Materials/AincradCharacters.  It does
not import meshes, set actor CustomDepth, or change a world/post-process volume.
Re-running it rebuilds the two material graphs in place.
"""
import unreal as ue


DEST = "/Game/ThreeHearths/Materials/AincradCharacters"
EDIT = ue.MaterialEditingLibrary
ASSETS = ue.AssetToolsHelpers.get_asset_tools()


def enum_value(enum_type, *names):
    """Resolve the UE Python spelling while failing loudly if it is unavailable."""
    for name in names:
        value = getattr(enum_type, name, None)
        if value is not None:
            return value
    raise RuntimeError("Required Unreal enum value is unavailable: " + ", ".join(names))


def get_material(name):
    path = DEST + "/" + name
    material = ue.load_asset(path)
    if material is None:
        material = ASSETS.create_asset(name, DEST, ue.Material, ue.MaterialFactoryNew())
    if not isinstance(material, ue.Material):
        raise RuntimeError("Asset is not a material: " + path)
    EDIT.delete_all_material_expressions(material)
    return material


def custom_input(name):
    item = ue.CustomInput()
    item.set_editor_property("input_name", name)
    return item


def scalar(material, name, value, x, y):
    node = EDIT.create_material_expression(material, ue.MaterialExpressionScalarParameter, x, y)
    node.set_editor_property("parameter_name", name)
    node.set_editor_property("default_value", value)
    return node


def vector(material, name, value, x, y):
    node = EDIT.create_material_expression(material, ue.MaterialExpressionVectorParameter, x, y)
    node.set_editor_property("parameter_name", name)
    node.set_editor_property("default_value", value)
    return node


def save(material):
    EDIT.layout_material_expressions(material)
    EDIT.recompile_material(material)
    if not ue.EditorAssetLibrary.save_loaded_asset(material, False):
        raise RuntimeError("Could not save " + material.get_path_name())


def make_resident_material():
    material = get_material("M_AnimeResident")
    EDIT.set_material_usage(material, ue.MaterialUsage.MATUSAGE_SKELETAL_MESH)
    material.set_editor_property("two_sided", False)

    custom = EDIT.create_material_expression(material, ue.MaterialExpressionCustom, 160, 0)
    custom.set_editor_property("output_type", ue.CustomMaterialOutputType.CMOT_FLOAT3)
    custom.set_editor_property(
        "code",
        """
float HearthN = saturate(HearthNdotL);
float HearthBand = smoothstep(0.24, 0.64, HearthN);
float3 HearthBandColor = lerp(HearthShadowTint, HearthTint, HearthBand);
return lerp(HearthTint, HearthBandColor, saturate(HearthBandMix));
""",
    )
    custom.set_editor_property(
        "inputs",
        [
            custom_input("HearthNdotL"),
            custom_input("HearthTint"),
            custom_input("HearthShadowTint"),
            custom_input("HearthBandMix"),
        ],
    )

    normal = EDIT.create_material_expression(material, ue.MaterialExpressionVertexNormalWS, -720, 0)
    # LightVector is not available in the opaque surface domain. This shaping
    # direction matches the town's fixed sun; DefaultLit still supplies shadows.
    light = vector(material, "HearthKeyDirection", ue.LinearColor(-.6064,.2828,.7431,1), -720, 140)
    ndotl = EDIT.create_material_expression(material, ue.MaterialExpressionDotProduct, -420, 80)
    EDIT.connect_material_expressions(normal, "", ndotl, "A")
    EDIT.connect_material_expressions(light, "RGB", ndotl, "B")
    EDIT.connect_material_expressions(ndotl, "", custom, "HearthNdotL")

    tint = vector(material, "HearthTint", ue.LinearColor(0.72, 0.56, 0.48, 1.0), -720, 300)
    shadow_tint = vector(material, "HearthShadowTint", ue.LinearColor(0.30, 0.25, 0.30, 1.0), -720, 420)
    band_mix = scalar(material, "HearthBandMix", 0.34, -720, 540)
    EDIT.connect_material_expressions(tint, "RGB", custom, "HearthTint")
    EDIT.connect_material_expressions(shadow_tint, "RGB", custom, "HearthShadowTint")
    EDIT.connect_material_expressions(band_mix, "", custom, "HearthBandMix")
    EDIT.connect_material_property(custom, "", ue.MaterialProperty.MP_BASE_COLOR)

    roughness = scalar(material, "HearthRoughness", 0.93, 420, 220)
    specular = scalar(material, "HearthSpecular", 0.035, 420, 340)
    fill = scalar(material, "HearthFill", 0.025, 420, 520)
    EDIT.connect_material_property(roughness, "", ue.MaterialProperty.MP_ROUGHNESS)
    EDIT.connect_material_property(specular, "", ue.MaterialProperty.MP_SPECULAR)
    fill_mul = EDIT.create_material_expression(material, ue.MaterialExpressionMultiply, 650, 500)
    EDIT.connect_material_expressions(custom, "", fill_mul, "A")
    EDIT.connect_material_expressions(fill, "", fill_mul, "B")
    EDIT.connect_material_property(fill_mul, "", ue.MaterialProperty.MP_EMISSIVE_COLOR)
    save(material)
    return material


def scene_texture(material, texture_id, x, y):
    node = EDIT.create_material_expression(material, ue.MaterialExpressionSceneTexture, x, y)
    node.set_editor_property("scene_texture_id", texture_id)
    node.set_editor_property("filtered", False)
    return node


def make_outline_material():
    material = get_material("M_AnimeResidentOutline")
    material.set_editor_property(
        "material_domain",
        enum_value(ue.MaterialDomain, "MD_POST_PROCESS", "MD_PostProcess"),
    )
    material.set_editor_property(
        "blendable_location",
        enum_value(ue.BlendableLocation, "BL_AFTER_TONEMAPPING", "BL_AfterTonemapping"),
    )

    custom = EDIT.create_material_expression(material, ue.MaterialExpressionCustom, 360, 0)
    custom.set_editor_property("output_type", ue.CustomMaterialOutputType.CMOT_FLOAT3)
    custom.set_editor_property(
        "code",
        """
float HearthResident = step(6.5, HearthStencil) * step(HearthStencil, 7.5);
float HearthVisible = step(HearthCustomDepth, HearthSceneDepth+0.5);
float HearthMask = HearthResident * HearthVisible;
float HearthEdge = saturate(length(float2(ddx(HearthMask), ddy(HearthMask))) * 0.75);
return lerp(HearthSceneColor, HearthOutlineTint, saturate(HearthEdge * HearthOutlineStrength));
""",
    )
    custom.set_editor_property(
        "inputs",
        [
            custom_input("HearthSceneColor"),
            custom_input("HearthStencil"),
            custom_input("HearthCustomDepth"),
            custom_input("HearthSceneDepth"),
            custom_input("HearthOutlineTint"),
            custom_input("HearthOutlineStrength"),
        ],
    )

    scene_color = scene_texture(
        material,
        enum_value(ue.SceneTextureId, "PPI_POST_PROCESS_INPUT_0", "PPI_PostProcessInput0"),
        -720,
        0,
    )
    stencil = scene_texture(
        material,
        enum_value(ue.SceneTextureId, "PPI_CUSTOM_STENCIL", "PPI_CustomStencil"),
        -720,
        140,
    )
    custom_depth = scene_texture(
        material,
        enum_value(ue.SceneTextureId, "PPI_CUSTOM_DEPTH", "PPI_CustomDepth"),
        -720,
        280,
    )
    scene_depth = scene_texture(
        material,
        enum_value(ue.SceneTextureId, "PPI_SCENE_DEPTH", "PPI_SceneDepth"),
        -720,
        420,
    )
    EDIT.connect_material_expressions(scene_color, "", custom, "HearthSceneColor")
    EDIT.connect_material_expressions(stencil, "", custom, "HearthStencil")
    EDIT.connect_material_expressions(custom_depth, "", custom, "HearthCustomDepth")
    EDIT.connect_material_expressions(scene_depth, "", custom, "HearthSceneDepth")

    outline_tint = vector(material, "HearthOutlineTint", ue.LinearColor(0.025, 0.035, 0.075, 1.0), -720, 560)
    outline_strength = scalar(material, "HearthOutlineStrength", 0.72, -720, 680)
    EDIT.connect_material_expressions(outline_tint, "RGB", custom, "HearthOutlineTint")
    EDIT.connect_material_expressions(outline_strength, "", custom, "HearthOutlineStrength")
    EDIT.connect_material_property(custom, "", ue.MaterialProperty.MP_EMISSIVE_COLOR)
    save(material)
    return material


if not ue.EditorAssetLibrary.does_directory_exist(DEST):
    ue.EditorAssetLibrary.make_directory(DEST)

resident = make_resident_material()
outline = make_outline_material()
ue.log("AINCRAD_CHARACTER_MATERIALS_SAVED " + resident.get_path_name() + " " + outline.get_path_name())
