extends Node3D
## Authored character appearance only; permanent resident identity belongs to the world layer.
const ASSET_PATH := "res://assets/characters/kirito/Kirito.glb"
static var template: PackedScene
var skeleton: Skeleton3D
var visual_mesh: MeshInstance3D
var animation: AnimationPlayer
var animation_name: StringName
var elapsed := 0.0
var test_pose := false

static func prepare() -> bool:
    if template != null: return true
    var document := GLTFDocument.new()
    var state := GLTFState.new()
    if document.append_from_file(ASSET_PATH,state) != OK: return false
    var root := document.generate_scene(state)
    if root == null: return false
    _prepare_materials(root)
    template = PackedScene.new()
    var err := template.pack(root)
    root.free()
    return err == OK

static func _prepare_materials(node: Node) -> void:
    if node is MeshInstance3D:
        for surface in range(node.mesh.get_surface_count()):
            var material: Material = node.mesh.surface_get_material(surface)
            if material is StandardMaterial3D:
                material.roughness = 1.0
                material.metallic_specular = 0.0
                material.texture_filter = BaseMaterial3D.TEXTURE_FILTER_LINEAR_WITH_MIPMAPS_ANISOTROPIC
                if material.albedo_color.a < .01:
                    material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA_SCISSOR
                    material.alpha_scissor_threshold = .5
                elif str(material.resource_name).contains("髪"):
                    material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA_SCISSOR
                    material.alpha_scissor_threshold = .2
                    material.cull_mode = BaseMaterial3D.CULL_DISABLED
                else:
                    material.transparency = BaseMaterial3D.TRANSPARENCY_DISABLED
    for child in node.get_children(): _prepare_materials(child)

func install(phase: float) -> void:
    var model := template.instantiate()
    add_child(model)
    _find_parts(model)
    assert(skeleton != null and visual_mesh != null and animation != null)
    for candidate in animation.get_animation_list():
        if str(candidate).contains("Idle"):
            animation_name = candidate
            animation.get_animation(candidate).loop_mode = Animation.LOOP_LINEAR
            animation.play(candidate)
            animation.seek(phase,true)
            break
    elapsed = phase
    set_meta("appearance_source","sugaki_kirito")
    set_meta("visual_prop_only",true)

func _find_parts(node: Node) -> void:
    if node is Skeleton3D: skeleton = node
    if node is MeshInstance3D: visual_mesh = node
    if node is AnimationPlayer: animation = node
    for child in node.get_children(): _find_parts(child)

func set_inspection_pose(enabled: bool) -> void:
    test_pose = enabled
    if not enabled:
        skeleton.reset_bone_poses()
        animation.play(animation_name)
        animation.advance(0)
        return
    animation.pause()
    var index := skeleton.find_bone("右ひじ")
    if index >= 0:
        var rest_basis := skeleton.get_bone_global_rest(index).basis
        var bend := Basis(Vector3.RIGHT,-.8)
        skeleton.set_bone_pose_rotation(index,(rest_basis.inverse()*bend*rest_basis).get_rotation_quaternion())
    for name in ["まばたき","あ"]:
        var shape := visual_mesh.find_blend_shape_by_name(name)
        if shape >= 0: visual_mesh.set_blend_shape_value(shape,1.0 if name == "まばたき" else .65)

func inspect_data() -> Dictionary:
    var names: Array[String] = []
    for i in range(visual_mesh.mesh.get_blend_shape_count()): names.append(visual_mesh.mesh.get_blend_shape_name(i))
    var baked := visual_mesh.bake_mesh_from_current_skeleton_pose()
    return {"bones":skeleton.get_bone_count(),"blend_shapes":names,"has_skin":visual_mesh.skin != null,
        "rest_aabb":str(visual_mesh.mesh.get_aabb()),"posed_aabb":str(baked.get_aabb()) if baked else "unavailable",
        "skeleton_path":str(visual_mesh.skeleton),"skeleton_transform":str(skeleton.global_transform),
        "animations":animation.get_animation_list(),"animation_playing":animation.is_playing(),
        "global_position":[global_position.x,global_position.y,global_position.z],"visual_only":true}
