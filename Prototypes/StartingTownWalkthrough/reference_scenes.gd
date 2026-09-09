extends Node3D
## Two editable Blender scenes installed at the existing Level0 region anchors.
const MANIFEST_PATH := "res://assets/reference_scenes/scene_manifest.json"
const KIRITO = preload("res://kirito_actor.gd")
var host: Node3D
var sites: Dictionary = {}
var manifest: Dictionary
var active_site := 4
var inspection_camera := true
var original_environment: Environment
var review_environment: Environment
var world_environment: WorldEnvironment
var original_sun: Dictionary
var people: Dictionary = {4:[],5:[]}
var character_angle := 0
var evidence: Dictionary = {"schema":1,"kind":"blender_reference_scene_geometry",
    "kimi_requests":0,"persistent_world_loaded":false,"npc_behavior_implemented":false,
    "canonical_exact_coordinates":false,"visual_approval":false,"checks":{},"screenshots":[]}

func install(owner_node: Node3D, manifest_path := MANIFEST_PATH) -> bool:
    host = owner_node
    for child in host.get_children():
        if child is WorldEnvironment: world_environment = child
    original_environment = world_environment.environment
    original_sun = {"rotation":host.sun.rotation,"energy":host.sun.light_energy,"color":host.sun.light_color}
    review_environment = original_environment.duplicate(true)
    var review_sky: ProceduralSkyMaterial = review_environment.sky.sky_material
    review_sky.sky_top_color = Color("669ed2")
    review_sky.sky_horizon_color = Color("c3ddeb")
    review_sky.ground_horizon_color = Color("bcc6b2")
    review_environment.ambient_light_color = Color("bbd3ea")
    review_environment.ambient_light_energy = .34
    review_environment.tonemap_mode = Environment.TONE_MAPPER_FILMIC
    review_environment.tonemap_exposure = 1.05
    review_environment.ssao_enabled = true
    review_environment.ssao_radius = 1.05
    review_environment.ssao_intensity = 2.4
    review_environment.ssao_power = 1.5
    review_environment.ssao_detail = .6
    review_environment.ssao_light_affect = .45
    review_environment.fog_enabled = false
    var parsed = JSON.parse_string(FileAccess.get_file_as_string(manifest_path))
    if not parsed is Dictionary:
        push_error("Reference scene manifest missing or invalid")
        return false
    manifest = parsed
    if not KIRITO.prepare():
        push_error("Authored Kirito model missing or invalid")
        return false
    for index in range(manifest.scenes.size()):
        var entry: Dictionary = manifest.scenes[index]
        var gltf := GLTFDocument.new()
        var state := GLTFState.new()
        if gltf.append_from_file("res://assets/reference_scenes/" + entry.glb,state) != OK:
            push_error("Reference GLB could not be imported: " + entry.glb)
            return false
        var root: Node3D = gltf.generate_scene(state)
        if root == null: return false
        add_child(root)
        root.name = entry.id
        root.position = Vector3(entry.godot_position_m[0],entry.godot_position_m[1],entry.godot_position_m[2])
        root.rotation.y = deg_to_rad(float(entry.godot_yaw_degrees))
        var parts: Array[MeshInstance3D] = []
        _collect(root,parts)
        var solid_faces := PackedVector3Array()
        for mesh in parts:
            if str(mesh.name).contains("_visual_person_"):
                var actor := KIRITO.new()
                actor.name = str(mesh.name).replace("visual_person","kirito_visual")
                mesh.get_parent().add_child(actor)
                actor.transform = mesh.transform
                actor.install(float(people[4+index].size())*.23)
                people[4+index].append(actor)
                mesh.visible = false
            # GLB materials preserve the common Blender palette; old F5/F6 does not recolor these meshes.
            if mesh.name in entry.collision_mesh_names:
                for vertex_pos in mesh.mesh.get_faces():
                    solid_faces.append(mesh.global_transform * vertex_pos)
                # Exported simple collision proxies must never cover the crafted surfaces.
                mesh.visible = false
        var body := StaticBody3D.new()
        body.name = entry.id + "_Collision"
        var shape := ConcavePolygonShape3D.new()
        shape.backface_collision = true
        shape.set_faces(solid_faces)
        var collider := CollisionShape3D.new()
        collider.shape = shape
        body.add_child(collider)
        add_child(body)
        sites[4+index] = {"root":root,"entry":entry,"meshes":parts}
    # Reversible replacement of overlapping inferred houses only, not the plaza/arcade landmarks.
    var removed: Array[String] = []
    for mesh in host.meshes.duplicate():
        var p: Vector3 = mesh.global_position
        var generated := str(mesh.name).begins_with("fabric_") or str(mesh.name).begins_with("roof_") or str(mesh.name).begins_with("task_") or str(mesh.name).begins_with("tree_")
        if generated and p.x > -294 and p.x < -112 and p.z > -79 and p.z < 66:
            mesh.visible = false
            host.meshes.erase(mesh)
            removed.append(str(mesh.name))
    evidence.replaced_blockout_object_ids = removed
    evidence.checks.two_distinct_regions = sites[4].entry.region_id == "beginnings_plaza" and sites[5].entry.region_id == "tolbana"
    evidence.checks.geographic_separation = sites[4].root.global_position.distance_to(sites[5].root.global_position) > 7000
    evidence.asset_revision = manifest.style_id
    evidence.checks.crafted_asset_revision = manifest.style_id in ["level0_anime_crafted_v2", "level0_market_craft_v5", "level0_two_towns_craft_v5"]
    evidence.scenes = []
    for key in sites:
        var s: Dictionary = sites[key]
        evidence.scenes.append({"id":s.entry.id,"region_id":s.entry.region_id,"godot_position_m":s.entry.godot_position_m,
            "floor_east_north_m":s.entry.floor_east_north_m,"mesh_instances":s.meshes.size(),"triangles":s.entry.triangles,"sha256":s.entry.sha256})
    return true

func _collect(node: Node, out: Array[MeshInstance3D]) -> void:
    if node is MeshInstance3D: out.append(node)
    for child in node.get_children(): _collect(child,out)

func local_to_world(site: int, blender_pos: Vector3) -> Vector3:
    return sites[site].root.to_global(Vector3(blender_pos.x,blender_pos.z,-blender_pos.y))

func set_view(site: int, view_kind := "reference") -> void:
    active_site = site
    world_environment.environment = review_environment
    host.sun.rotation_degrees = Vector3(-52, 34 if site == 4 else -32, 0)
    if site == 4: host.sun.rotation.y += sites[site].root.rotation.y
    host.sun.light_energy = 1.0
    host.sun.light_color = Color("fff0d6")
    host.sun.shadow_bias = .035
    host.sun.shadow_normal_bias = .5
    host.sun.shadow_blur = 1.5
    var camera_pos: Vector3
    var target: Vector3
    var fov := 62.0
    if site == 4:
        match view_kind:
            "overview":
                camera_pos = Vector3(38,-35,42); target = Vector3(0,28,5); fov = 68
            "close":
                camera_pos = Vector3(-1,-10,2.4); target = Vector3(-6.7,-4,3.1); fov = 68
            "craft":
                camera_pos = Vector3(-1,-11,5.8); target = Vector3(-6.65,-4,5.7); fov = 62
            "reverse":
                camera_pos = Vector3(0,37,2.0); target = Vector3(0,-15,6.5); fov = 65
            _:
                camera_pos = Vector3(-.5,-18,2.65); target = Vector3(0,28,7.4); fov = 64
    else:
        match view_kind:
            "overview":
                camera_pos = Vector3(70,-68,65); target = Vector3(0,25,5); fov = 67
            "close":
                camera_pos = Vector3(15,-13,2.8); target = Vector3(8.9,-5,2.3); fov = 64
            "craft":
                camera_pos = Vector3(-11,13,5.5); target = Vector3(-21.5,28,7.2); fov = 62
            "reverse":
                camera_pos = Vector3(-6,16,2.0); target = Vector3(8.9,-5,3); fov = 68
            _:
                camera_pos = Vector3(21,-31,6.3); target = Vector3(-1,15,7.3); fov = 61
    if view_kind == "reference":
        var entry: Dictionary = sites[site].entry
        var cp: Array = entry.reference_camera_blender_m
        var ct: Array = entry.reference_target_blender_m
        camera_pos = Vector3(cp[0],cp[1],cp[2])
        target = Vector3(ct[0],ct[1],ct[2])
        fov = entry.reference_horizontal_fov
    host.player.position = local_to_world(site,camera_pos)-Vector3(0,1.68,0)
    host.player.velocity = Vector3.ZERO
    host.player.rotation = Vector3.ZERO
    host.eye.rotation = Vector3.ZERO
    # Blender authored a horizontal angle; Godot's KEEP_HEIGHT uses a vertical angle.
    var viewport_size: Vector2 = get_viewport().get_visible_rect().size
    host.eye.fov = rad_to_deg(2.0*atan(tan(deg_to_rad(fov)*.5)/(viewport_size.x/viewport_size.y)))
    host.eye.look_at(local_to_world(site,target))
    host.player.rotation.y = host.eye.rotation.y
    host.eye.rotation.y = 0
    host.eye.make_current()
    inspection_camera = true
    host.caption.text = "LEVEL0 / " + ("起始之城 · 市场街" if site == 4 else "托尔巴纳 · 喷泉广场")
    host.help_text.text = "WASD 行走 · 鼠标看向 · Shift 快走 · 空格跳跃\n4 市场街  5 托尔巴纳  R 参考机位  O 局部俯瞰\n点击或按 WASD 开始行走 · Esc 释放鼠标\n1 返回起始之城中央广场"
    host.help_text.text += "\nC 人物近景 / 侧面 / 背面 · X 抬手与表情检查"

func character_view(angle := 0) -> void:
    set_view(active_site)
    var actor: Node3D = people[active_site][1 if active_site == 4 else 0]
    var offset := Vector3(.35,1.42,2.6)
    if angle == 1: offset = Vector3(2.25,1.42,.8)
    if angle == 2: offset = Vector3(1.8,1.42,-1.6)
    var camera_pos: Vector3 = actor.to_global(offset)
    host.player.position = camera_pos-Vector3(0,1.68,0)
    host.player.rotation = Vector3.ZERO
    host.eye.rotation = Vector3.ZERO
    host.eye.fov = 37
    host.eye.look_at(actor.to_global(Vector3(0,1.25,0)))
    host.player.rotation.y = host.eye.rotation.y
    host.eye.rotation.y = 0
    inspection_camera = true

func handle_input(event: InputEvent) -> void:
    if event is InputEventKey and event.pressed and not event.echo:
        match event.keycode:
            KEY_4: set_view(4)
            KEY_5: set_view(5)
            KEY_R: set_view(active_site)
            KEY_O: set_view(active_site,"overview")
            KEY_C:
                character_view(character_angle)
                character_angle = (character_angle+1)%3
            KEY_X:
                var actor: Node3D = people[active_site][1 if active_site == 4 else 0]
                actor.set_inspection_pose(not actor.test_pose)
            KEY_W,KEY_A,KEY_S,KEY_D,KEY_SPACE: inspection_camera = false
            KEY_1,KEY_2,KEY_3,KEY_T: inspection_camera = false
    if event is InputEventMouseButton and event.pressed:
        inspection_camera = false

func restore_blockout_lighting() -> void:
    world_environment.environment = original_environment
    host.sun.rotation = original_sun.rotation
    host.sun.light_energy = original_sun.energy
    host.sun.light_color = original_sun.color

func capture_and_validate() -> void:
    host.capturing = true
    host.caption.get_parent().get_parent().visible = false
    var output := "res://validation/reference_scenes_hq"
    DirAccess.make_dir_recursive_absolute(ProjectSettings.globalize_path(output))
    await get_tree().physics_frame
    await get_tree().physics_frame
    var space := host.get_world_3d().direct_space_state
    for site in [4,5]:
        var key: String = "market" if site == 4 else "tolbana"
        for kind in ["reference","close","craft","reverse","overview"]:
            set_view(site,kind)
            await get_tree().create_timer(.35).timeout
            await RenderingServer.frame_post_draw
            var filename: String = output + "/" + key + "_" + kind + ".png"
            var err: Error = get_viewport().get_texture().get_image().save_png(filename)
            evidence.checks[key+"_"+kind+"_capture"] = err == OK
            evidence.screenshots.append(filename)
        set_view(site)
        await get_tree().create_timer(1.0).timeout
        evidence[key+"_reference_fps_sample"] = Engine.get_frames_per_second()
        evidence[key+"_draw_calls_sample"] = Performance.get_monitor(Performance.RENDER_TOTAL_DRAW_CALLS_IN_FRAME)
        # An actual continuous camera travel, keeping the reference target in view.
        # Save three frames from the same run so parallax and occlusion can be checked.
        var camera_start: Vector3 = host.player.position
        var travel: Vector3 = sites[site].root.global_basis * Vector3(1.5,0,-3.0)
        for motion_frame in range(121):
            await get_tree().process_frame
            host.player.position = camera_start + travel * (float(motion_frame)/120.0)
            if motion_frame in [0,60,120]:
                await RenderingServer.frame_post_draw
                var motion_file: String = output + "/" + key + "_motion_%03d.png" % motion_frame
                var motion_result: Error = get_viewport().get_texture().get_image().save_png(motion_file)
                evidence.checks[key+"_motion_%03d" % motion_frame] = motion_result == OK
                evidence.screenshots.append(motion_file)
        var start_local := Vector3(0,-20,2) if site == 4 else Vector3(4,-14,2)
        var ground_query := PhysicsRayQueryParameters3D.create(local_to_world(site,start_local),local_to_world(site,start_local-Vector3(0,0,5)),1,[host.player.get_rid()])
        evidence.checks[key+"_ground"] = not space.intersect_ray(ground_query).is_empty()
        # Real CharacterBody movement through each installed scene, no per-frame teleport.
        host.player.position = local_to_world(site,start_local)
        host.player.velocity = Vector3.ZERO
        var start_pos: Vector3 = host.player.position
        var heading: Vector3 = sites[site].root.global_basis * Vector3(0,0,-1)
        for frame in range(150):
            await get_tree().physics_frame
            host.player.velocity = Vector3(heading.x*3.0,host.player.velocity.y-18.0/60.0,heading.z*3.0)
            host.player.move_and_slide()
        var delta: Vector3 = host.player.position-start_pos
        var horizontal_distance := Vector2(delta.x,delta.z).length()
        evidence.checks[key+"_walk"] = horizontal_distance > 6 and host.player.is_on_floor()
        evidence[key+"_walk_distance_m"] = horizontal_distance
        evidence[key+"_walk_finish"] = [host.player.position.x,host.player.position.y,host.player.position.z]
    evidence.engine = Engine.get_version_info().string
    evidence.renderer = RenderingServer.get_current_rendering_method()
    evidence.passed = true
    for value in evidence.checks.values(): evidence.passed = evidence.passed and value
    var file := FileAccess.open(output+"/validation.json",FileAccess.WRITE)
    file.store_string(JSON.stringify(evidence,"\t"))
    print("LEVEL0_REFERENCE_VALIDATION " + JSON.stringify(evidence))
    get_tree().quit(0 if evidence.passed else 1)

func capture_characters() -> void:
    host.capturing = true
    host.caption.get_parent().get_parent().visible = false
    var output := "res://validation/kirito_in_engine"
    DirAccess.make_dir_recursive_absolute(ProjectSettings.globalize_path(output))
    var result: Dictionary = {"engine":Engine.get_version_info().string,"renderer":RenderingServer.get_current_rendering_method(),
        "source_sha256":FileAccess.get_sha256(KIRITO.ASSET_PATH),"visual_approval":false,"npc_behavior_implemented":false,
        "kimi_requests":0,"physics_transferred":false,"checks":{},"actors":{},"screenshots":[]}
    for site in [4,5]:
        var prefix: String = "market" if site == 4 else "tolbana"
        set_view(site)
        var actor: Node3D = people[site][1 if site == 4 else 0]
        result.actors[prefix] = actor.inspect_data()
        result.checks[prefix+"_replacement_count"] = people[site].size() == (15 if site == 4 else 5)
        result.checks[prefix+"_skin"] = actor.visual_mesh.skin != null and actor.skeleton.get_bone_count() == 200
        result.checks[prefix+"_morphs"] = actor.visual_mesh.mesh.get_blend_shape_count() == 32
        var bounds: AABB = actor.visual_mesh.mesh.get_aabb()
        result.checks[prefix+"_mesh_bounds"] = bounds.size.y > 1.5 and bounds.size.y < 2.5 and bounds.size.length() < 4.0
        var hidden := true
        for old_mesh in sites[site].meshes:
            if str(old_mesh.name).contains("_visual_person_"): hidden = hidden and not old_mesh.visible
        result.checks[prefix+"_old_people_hidden"] = hidden
        for view in ["context","front","side","back","pose"]:
            if view == "context": set_view(site)
            else: character_view({"front":0,"side":1,"back":2,"pose":0}[view])
            actor.set_inspection_pose(view == "pose")
            await get_tree().create_timer(.45).timeout
            await RenderingServer.frame_post_draw
            var filename: String = output+"/"+prefix+"_"+view+".png"
            result.checks[prefix+"_"+view+"_capture"] = get_viewport().get_texture().get_image().save_png(filename) == OK
            result.screenshots.append(filename)
        var elbow: int = actor.skeleton.find_bone("右ひじ")
        var blink: int = actor.visual_mesh.find_blend_shape_by_name("まばたき")
        result.checks[prefix+"_elbow_pose"] = actor.skeleton.get_bone_pose_rotation(elbow).angle_to(Quaternion.IDENTITY) > .4
        result.checks[prefix+"_blink_pose"] = actor.visual_mesh.get_blend_shape_value(blink) > .9
        actor.set_inspection_pose(false)
        var head: int = actor.skeleton.find_bone("頭")
        actor.animation.seek(0,true)
        actor.animation.advance(0)
        var before: Quaternion = actor.skeleton.get_bone_pose_rotation(head)
        actor.animation.seek(1.0,true)
        actor.animation.advance(0)
        result.checks[prefix+"_live_bone_animation"] = before.angle_to(actor.skeleton.get_bone_pose_rotation(head)) > .001
        result.checks[prefix+"_floor_anchor"] = abs(actor.position.y-.25) < .01
        # Frames from one continuous camera move around the imported character.
        character_view(0)
        var start: Vector3 = host.player.position
        for frame in range(61):
            host.player.position = start+actor.global_basis*Vector3(.55*float(frame)/60,0,0)
            await get_tree().process_frame
            if frame in [0,30,60]:
                await RenderingServer.frame_post_draw
                var filename: String = output+"/"+prefix+"_motion_%02d.png"%frame
                result.checks[prefix+"_motion_%02d"%frame] = get_viewport().get_texture().get_image().save_png(filename) == OK
                result.screenshots.append(filename)
    result.passed = true
    for value in result.checks.values(): result.passed = result.passed and value
    FileAccess.open(output+"/validation.json",FileAccess.WRITE).store_string(JSON.stringify(result,"\t"))
    print("KIRITO_ENGINE_VALIDATION "+JSON.stringify(result))
    get_tree().quit(0 if result.passed else 1)
