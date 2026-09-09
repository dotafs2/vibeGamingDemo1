extends Node3D

const MODEL_PATH := "res://assets/StartingTown_Outline.glb"
const TOON_SHADER = preload("res://soft_toon.gdshader")
const REFERENCE_SCENES = preload("res://reference_scenes.gd")
const MODES := ["柔和风格光照", "分层明暗参考", "标准 PBR 参考"]
var player: CharacterBody3D
var eye: Camera3D
var bird: Camera3D
var sun: DirectionalLight3D
var caption: Label
var help_text: Label
var town: Node3D
var navigation: NavigationRegion3D
var meshes: Array[MeshInstance3D] = []
var original_materials: Dictionary = {}
var art_mode := 0
var checking := false
var capturing := false
var drive := Vector3.ZERO
var references: Node3D
var reference_manifest_path := "res://assets/reference_scenes/craft_scene_manifest.json"
var report: Dictionary = {"schema": 1, "kind": "spatial_prototype", "kimi_requests": 0,
    "persistent_world_loaded": false, "npc_decisions_implemented": false, "checks": {}}

func _ready() -> void:
    Engine.max_fps = 60
    checking = "--validate" in OS.get_cmdline_user_args()
    capturing = "--capture" in OS.get_cmdline_user_args()
    _input_actions()
    _lighting()
    var gltf := GLTFDocument.new()
    var state := GLTFState.new()
    var result := gltf.append_from_file(MODEL_PATH, state)
    if result != OK:
        push_error("Town GLB import failed: " + str(result))
        get_tree().quit(1)
        return
    town = gltf.generate_scene(state)
    if town == null:
        push_error("Town GLB produced no scene")
        get_tree().quit(1)
        return
    town.name = "TownGeometry"
    add_child(town)
    _collect_meshes(town)
    report.source_blockout_mesh_instances = meshes.size()
    _repair_arcade_gateway_supports()
    references = REFERENCE_SCENES.new()
    references.name = "GeographicReferenceScenes"
    add_child(references)
    if not references.install(self,reference_manifest_path):
        get_tree().quit(1)
        return
    _collision()
    if not checking and ResourceLoader.exists("res://assets/first_street_navmesh.tres"):
        navigation = NavigationRegion3D.new()
        navigation.name = "FirstStreetNavigation"
        navigation.navigation_mesh = load("res://assets/first_street_navmesh.tres")
        add_child(navigation)
    _player()
    _ui()
    _style(0)
    _view(1)
    report.engine = Engine.get_version_info().string
    report.checks.glb_import = report.source_blockout_mesh_instances > 700
    report.mesh_instances = meshes.size()
    if "--character-capture" in OS.get_cmdline_user_args():
        references.call_deferred("capture_characters")
    elif "--reference-capture" in OS.get_cmdline_user_args():
        call_deferred("_reference_capture")
    elif checking:
        call_deferred("_validate")
    elif "--capture" in OS.get_cmdline_user_args():
        call_deferred("_capture")
    else:
        references.set_view(4)
    print("LEVEL0_SPATIAL_READY " + str(meshes.size()))

func _input_actions() -> void:
    for action in {"forward":KEY_W,"back":KEY_S,"left":KEY_A,"right":KEY_D,"sprint":KEY_SHIFT,"jump":KEY_SPACE}:
        if not InputMap.has_action(action): InputMap.add_action(action)
        var key := InputEventKey.new()
        key.physical_keycode = {"forward":KEY_W,"back":KEY_S,"left":KEY_A,"right":KEY_D,"sprint":KEY_SHIFT,"jump":KEY_SPACE}[action]
        InputMap.action_add_event(action,key)

func _lighting() -> void:
    var world := WorldEnvironment.new()
    var env := Environment.new()
    env.background_mode = Environment.BG_SKY
    var sky := Sky.new()
    var sky_mat := ProceduralSkyMaterial.new()
    sky_mat.sky_top_color = Color("79a7ce")
    sky_mat.sky_horizon_color = Color("d2e1da")
    sky_mat.ground_bottom_color = Color("7b836c")
    sky_mat.ground_horizon_color = Color("d2ddd1")
    sky.sky_material = sky_mat
    env.sky = sky
    env.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
    env.ambient_light_color = Color("c9dddf")
    env.ambient_light_energy = 0.25
    env.tonemap_mode = Environment.TONE_MAPPER_LINEAR
    env.fog_enabled = true
    env.fog_light_color = Color("b4ced1")
    env.fog_density = 0.0007
    world.environment = env
    add_child(world)
    sun = DirectionalLight3D.new()
    sun.rotation_degrees = Vector3(-48,-32,0)
    sun.light_color = Color("fff5e6")
    sun.light_energy = 0.45
    sun.shadow_enabled = true
    sun.directional_shadow_max_distance = 210
    add_child(sun)

func _reference_capture() -> void:
    await references.capture_and_validate()

func _collect_meshes(node: Node) -> void:
    if node is MeshInstance3D:
        meshes.append(node)
        if node.mesh.get_surface_count() > 0:
            original_materials[node.get_instance_id()] = node.mesh.surface_get_material(0)
    for child in node.get_children(): _collect_meshes(child)

func _repair_arcade_gateway_supports() -> void:
    # The archived blockout generated the four enlarged arch lintels without piers.
    # Keep the source GLB intact and provide the missing structural geometry here.
    var support_count := 0
    for arch in town.find_children("arcade_gateway_*","MeshInstance3D",true,false):
        var material: Material = arch.mesh.surface_get_material(0)
        for side in [-1,1]:
            var support_root := Node3D.new()
            support_root.name = "GatewaySupport_%s_%d" % [arch.name,side]
            arch.add_child(support_root)
            # This asset retains its original Z-up mesh basis under a rotated GLB root.
            for tier in range(3):
                var shape := BoxMesh.new()
                shape.size = [Vector3(1.3,2.4,5.4),Vector3(1.65,2.65,.22),Vector3(1.6,2.65,.3)][tier]
                shape.material = material
                var part := MeshInstance3D.new()
                part.name = "GatewayPier_%s_%d_%d" % [arch.name,side,tier]
                part.mesh = shape
                support_root.add_child(part)
                part.position = Vector3(side*3.65,0,[2.7,.11,5.1][tier])
            _collect_meshes(support_root)
            support_count += 1
    report.repaired_gateway_piers = support_count

func _collision() -> void:
    var faces := PackedVector3Array()
    for instance in meshes:
        for point in instance.mesh.get_faces():
            faces.append(instance.global_transform * point)
    var body := StaticBody3D.new()
    body.name = "TownCollision"
    var collision := CollisionShape3D.new()
    var shape := ConcavePolygonShape3D.new()
    shape.backface_collision = true
    shape.set_faces(faces)
    collision.shape = shape
    body.add_child(collision)
    add_child(body)
    report.triangle_count = faces.size() / 3

func _player() -> void:
    player = CharacterBody3D.new()
    player.name = "Visitor"
    player.floor_snap_length = 0.4
    var collider := CollisionShape3D.new()
    var capsule := CapsuleShape3D.new()
    capsule.radius = 0.32
    capsule.height = 1.8
    collider.shape = capsule
    collider.position.y = 0.9
    player.add_child(collider)
    add_child(player)
    eye = Camera3D.new()
    eye.position.y = 1.68
    eye.fov = 72
    eye.far = 1800
    player.add_child(eye)
    bird = Camera3D.new()
    bird.far = 2400
    bird.position = Vector3(420,440,590)
    add_child(bird)
    bird.look_at(Vector3(0,0,-30))

func _ui() -> void:
    var canvas := CanvasLayer.new()
    add_child(canvas)
    var panel := PanelContainer.new()
    panel.position = Vector2(24,22)
    var bg := StyleBoxFlat.new()
    bg.bg_color = Color(0.07,0.13,0.16,0.84)
    bg.content_margin_left = 18
    bg.content_margin_right = 18
    bg.content_margin_top = 12
    bg.content_margin_bottom = 12
    panel.add_theme_stylebox_override("panel",bg)
    canvas.add_child(panel)
    var column := VBoxContainer.new()
    panel.add_child(column)
    var system_font := SystemFont.new()
    system_font.font_names = PackedStringArray(["Microsoft YaHei UI","Noto Sans CJK SC","sans-serif"])
    caption = Label.new()
    caption.add_theme_font_override("font",system_font)
    caption.add_theme_font_size_override("font_size",23)
    column.add_child(caption)
    help_text = Label.new()
    help_text.add_theme_font_override("font",system_font)
    help_text.add_theme_font_size_override("font_size",16)
    help_text.text = "WASD 行走 · 鼠标看向 · Shift 快走 · 空格跳跃\n1 广场  2 市场街  3 城门  T 俯瞰 · Esc 释放鼠标\nF5 柔和光照 / F6 分层明暗 / F7 标准 PBR\n空间原型：尚未接入 NPC；地标尺度与街区为研究补全"
    column.add_child(help_text)

func _style(mode: int) -> void:
    art_mode = mode
    var palette := {"ground":"98ad7d", "paving":"d8cdb3", "stone":"c7bca4",
        "observed":"c4bba4", "infill":"cfc1a7", "roof":"707f86", "palace":"485c69",
        "dark":"635949", "tree":"6b9466", "route":"aa7657", "clock":"efe5c7"}
    var shared: Dictionary = {}
    for instance in meshes:
        var original: Material = original_materials.get(instance.get_instance_id())
        if original == null: continue
        var material_name := original.resource_name
        if not shared.has(material_name):
            var color := Color(str(palette.get(material_name,"c6bba7")))
            if mode == 2:
                var pbr := StandardMaterial3D.new()
                pbr.albedo_color = color
                pbr.roughness = 0.92
                pbr.metallic_specular = 0.0
                pbr.cull_mode = BaseMaterial3D.CULL_DISABLED
                shared[material_name] = pbr
                instance.material_override = pbr
                continue
            var material := ShaderMaterial.new()
            material.shader = TOON_SHADER
            material.set_shader_parameter("surface_color",color)
            material.set_shader_parameter("step_strength",0.2 if mode == 0 else 1.0)
            material.set_shader_parameter("pattern_strength",1.0 if material_name == "paving" else 0.0)
            shared[material_name] = material
        instance.material_override = shared[material_name]
    caption.text = "LEVEL0 / 起始之城    ·    " + MODES[mode]

func _view(index: int) -> void:
    if references != null: references.restore_blockout_lighting()
    var positions := {1:Vector3(-30,0.6,43),2:Vector3(-115,0.6,-4),3:Vector3(0,0.6,-275)}
    var targets := {1:Vector3(0,14,0),2:Vector3(-195,5,-22),3:Vector3(0,15,-305)}
    player.position = positions[index]
    player.velocity = Vector3.ZERO
    player.rotation = Vector3.ZERO
    eye.rotation = Vector3.ZERO
    eye.fov = 75.0
    eye.look_at(targets[index])
    player.rotation.y = eye.rotation.y
    eye.rotation.y = 0
    eye.current = true
    if references != null: references.inspection_camera = false
    caption.text = "LEVEL0 / " + {1:"起始之城 · 中央广场",2:"起始之城 · 市场街入口",3:"起始之城 · 北城门"}[index]
    help_text.text = "WASD 行走 · 鼠标看向 · Shift 快走 · 空格跳跃\n1 广场  2 市场入口  3 北门  4 市场场景  5 托尔巴纳\nT 全城俯瞰 · F5/F6/F7 旧灰模明暗参考\n空间原型 · NPC 与持久世界尚未接入"

func _unhandled_input(event: InputEvent) -> void:
    if checking or capturing or references == null: return
    references.handle_input(event)
    if event is InputEventMouseButton and event.pressed:
        Input.mouse_mode = Input.MOUSE_MODE_CAPTURED
    if event is InputEventMouseMotion and Input.mouse_mode == Input.MOUSE_MODE_CAPTURED:
        player.rotate_y(-event.relative.x * 0.0022)
        eye.rotation.x = clamp(eye.rotation.x-event.relative.y*0.0022,-1.35,1.35)
    if event is InputEventKey and event.pressed and not event.echo:
        match event.keycode:
            KEY_ESCAPE: Input.mouse_mode = Input.MOUSE_MODE_VISIBLE
            KEY_1: _view(1)
            KEY_2: _view(2)
            KEY_3: _view(3)
            KEY_T:
                if bird.current: eye.make_current()
                else: bird.make_current()
            KEY_F5: _style(0)
            KEY_F6: _style(1)
            KEY_F7: _style(2)

func _physics_process(delta: float) -> void:
    if player == null or capturing: return
    if not checking and references.inspection_camera: return
    var direction: Vector3
    if checking:
        direction = drive
    else:
        var axis := Input.get_vector("left","right","forward","back")
        direction = player.basis * Vector3(axis.x,0,axis.y)
    var speed := 10.0 if Input.is_action_pressed("sprint") else 5.0
    player.velocity.x = direction.x * speed
    player.velocity.z = direction.z * speed
    if not player.is_on_floor(): player.velocity.y -= 18.0 * delta
    elif Input.is_action_just_pressed("jump") and not checking: player.velocity.y = 5.0
    else: player.velocity.y = 0
    player.move_and_slide()
    if player.position.y < -20: _view(1)

func _validate() -> void:
    for i in range(8): await get_tree().physics_frame
    var space := get_world_3d().direct_space_state
    var hits: Array = []
    for p in [Vector3(-30,4,43),Vector3(-120,4,-4),Vector3(-230,4,-29),Vector3(0,4,-275)]:
        var query := PhysicsRayQueryParameters3D.create(p,p-Vector3(0,9,0),1,[player.get_rid()])
        hits.append(not space.intersect_ray(query).is_empty())
    report.checks.ground_at_four_anchors = not hits.has(false)
    var wall_query := PhysicsRayQueryParameters3D.create(Vector3(-80,3,0),Vector3(-80,3,65),1,[player.get_rid()])
    report.checks.arcade_blocks_sideways_ray = not space.intersect_ray(wall_query).is_empty()
    # Bake only the first playable corridor, using the collision mesh.
    navigation = NavigationRegion3D.new()
    navigation.name = "FirstStreetNavigation"
    var nav := NavigationMesh.new()
    nav.geometry_parsed_geometry_type = NavigationMesh.PARSED_GEOMETRY_STATIC_COLLIDERS
    nav.geometry_source_geometry_mode = NavigationMesh.SOURCE_GEOMETRY_ROOT_NODE_CHILDREN
    nav.cell_size = 0.5
    nav.cell_height = 0.25
    nav.agent_radius = 0.5
    nav.agent_height = 2.0
    nav.agent_max_climb = 0.5
    nav.filter_baking_aabb = AABB(Vector3(-275,-2,-105),Vector3(320,48,220))
    # Parse from this root so the sibling collision body participates.
    add_child(navigation)
    var source := NavigationMeshSourceGeometryData3D.new()
    NavigationServer3D.parse_source_geometry_data(nav,source,self)
    NavigationServer3D.bake_from_source_geometry_data(nav,source)
    report.polygons_at_assignment = nav.get_polygon_count()
    NavigationServer3D.map_set_use_async_iterations(navigation.get_navigation_map(),false)
    navigation.navigation_mesh = nav
    # Registration and asynchronous map synchronization are separate from baking.
    # Wait for actual map readiness; a fixed two-frame delay is not sufficient.
    var map_rid := navigation.get_navigation_map()
    var iteration_before := NavigationServer3D.map_get_iteration_id(map_rid)
    for i in range(180):
        await get_tree().physics_frame
        if NavigationServer3D.map_get_iteration_id(map_rid) > iteration_before and NavigationServer3D.region_get_bounds(navigation.get_rid()).has_volume(): break
    report.checks.navigation_synchronized = NavigationServer3D.map_get_iteration_id(map_rid) > iteration_before and NavigationServer3D.region_get_bounds(navigation.get_rid()).has_volume()
    report.nav_region_bounds = str(NavigationServer3D.region_get_bounds(navigation.get_rid()))
    var path := NavigationServer3D.map_get_path(navigation.get_navigation_map(),Vector3(-30,.4,35),Vector3(-229,.4,-27),true)
    report.navigation_map_active = NavigationServer3D.map_is_active(navigation.get_navigation_map())
    report.navigation_start_snap = str(NavigationServer3D.map_get_closest_point(navigation.get_navigation_map(),Vector3(-30,.4,35)))
    report.navigation_finish_snap = str(NavigationServer3D.map_get_closest_point(navigation.get_navigation_map(),Vector3(-229,.4,-27)))
    report.navigation_polygons = nav.get_polygon_count()
    report.navigation_path_points = path.size()
    report.checks.navigation_reaches_market = path.size() >= 2 and path[-1].distance_to(Vector3(-229,.4,-27)) < 3.0
    # Actual CharacterBody motion through the western opening; no teleport during the walk.
    player.position = Vector3(-74,0.6,0)
    var start := player.position
    for i in range(8): await get_tree().physics_frame
    drive = Vector3(-1,0,0)
    for i in range(420): await get_tree().physics_frame
    drive = Vector3.ZERO
    report.walk_start = [start.x,start.y,start.z]
    report.walk_finish = [player.position.x,player.position.y,player.position.z]
    report.checks.walk_through_arcade = player.position.x < -104 and player.position.y > -1 and player.position.y < 2
    report.checks.unique_mesh_names = _unique_names()
    var passed := true
    for value in report.checks.values(): passed = passed and value
    if passed:
        report.checks.navigation_saved = ResourceSaver.save(nav,"res://assets/first_street_navmesh.tres") == OK
        passed = report.checks.navigation_saved
    report.passed = passed
    _write_report("spatial_validation.json")
    print("LEVEL0_SPATIAL_VALIDATION " + JSON.stringify(report))
    get_tree().quit(0 if passed else 1)

func _unique_names() -> bool:
    var names: Dictionary = {}
    for m in meshes:
        if names.has(m.name): return false
        names[m.name] = true
    return true

func _write_report(filename: String) -> void:
    DirAccess.make_dir_recursive_absolute(ProjectSettings.globalize_path("res://validation"))
    var file := FileAccess.open("res://validation/"+filename,FileAccess.WRITE)
    file.store_string(JSON.stringify(report,"\t"))

func _capture() -> void:
    DirAccess.make_dir_recursive_absolute(ProjectSettings.globalize_path("res://validation"))
    for view_id in [1,2]:
        for mode in [0,1,2]:
            _view(view_id)
            _style(mode)
            await get_tree().create_timer(0.45).timeout
            await RenderingServer.frame_post_draw
            get_viewport().get_texture().get_image().save_png("res://validation/view_%d_style_%d.png" % [view_id,mode])
    bird.current = true
    _style(0)
    await get_tree().create_timer(0.45).timeout
    await RenderingServer.frame_post_draw
    get_viewport().get_texture().get_image().save_png("res://validation/overview.png")
    report.render_capture = "completed"
    _write_report("render_validation.json")
    get_tree().quit()
