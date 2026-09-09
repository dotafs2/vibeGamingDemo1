extends SceneTree
## Isolated candidate import, actual engine projection, and real multiview captures.
const BASE := "res://validation/market_demo_v3"
const CANDIDATE := "res://assets/reference_scenes/market_v3_manifest.json"
var destination: String

func _initialize() -> void:
    call_deferred("capture")

func xyz(values: Array) -> Vector3:
    return Vector3(float(values[0]),float(values[1]),float(values[2]))

func frame_file(host: Node3D, filename: String) -> bool:
    for frame in range(48): await process_frame
    await RenderingServer.frame_post_draw
    var picture := root.get_texture().get_image()
    return picture.get_width() == 1600 and picture.get_height() == 900 and picture.save_png(destination+"/"+filename) == OK

func capture() -> void:
    var revision := "r1"
    for arg in OS.get_cmdline_user_args():
        if arg.begins_with("--revision="):
            revision = arg.trim_prefix("--revision=").validate_filename()
    destination = BASE+"/"+revision
    DirAccess.make_dir_recursive_absolute(ProjectSettings.globalize_path(destination))
    var draft = JSON.parse_string(FileAccess.get_file_as_string(CANDIDATE))
    if not draft is Dictionary:
        push_error("Candidate manifest missing"); quit(1); return
    var entry: Dictionary
    if draft.has("scenes"): entry = draft.scenes[0]
    elif draft.has("scene"): entry = draft.scene
    else: entry = draft
    var manifest: Dictionary = JSON.parse_string(FileAccess.get_file_as_string("res://assets/reference_scenes/scene_manifest.json"))
    manifest.scenes[0] = entry
    manifest.style_id = "level0_market_v3_candidate"
    var temporary_manifest := destination+"/engine_manifest.json"
    FileAccess.open(temporary_manifest,FileAccess.WRITE).store_string(JSON.stringify(manifest,"\t"))
    var host = load("res://main.tscn").instantiate()
    host.reference_manifest_path = temporary_manifest
    root.add_child(host)
    current_scene = host
    host.capturing = true
    host.caption.get_parent().get_parent().visible = false
    host.references.set_view(4)
    var report: Dictionary = {"schema":1,"revision":revision,"engine":Engine.get_version_info().string,
        "renderer":RenderingServer.get_current_rendering_method(),"checks":{},
        "visual_approval":false,"independent_review_pending":true,"global_style_approved":false,
        "source_glb":entry.glb,"source_sha256":entry.sha256,"kimi_requests":0,
        "reference_camera_blender_m":entry.reference_camera_blender_m,
        "reference_target_blender_m":entry.reference_target_blender_m,
        "horizontal_fov":entry.reference_horizontal_fov,"scene_origin":str(host.references.sites[4].root.global_position),
        "screenshots":[],"projected_landmarks":[]}
    report.checks.reference_saved = await frame_file(host,"reference.png")
    report.screenshots.append("reference.png")
    # Each marker is transformed by the imported scene root and actual engine camera.
    var markers = entry.get("reference_landmarks_3d",draft.get("reference_landmarks_3d",{}))
    var targets: Dictionary = {}
    var reference_path := BASE+"/reference_landmarks.json"
    if FileAccess.file_exists(reference_path):
        var source = JSON.parse_string(FileAccess.get_file_as_string(reference_path))
        if source is Dictionary: targets = source
    var marker_values: Dictionary = {}
    if markers is Dictionary: marker_values = markers
    elif markers is Array:
        for value in markers:
            if value is Dictionary: marker_values[value.get("name",value.get("id",""))] = value
    for marker in marker_values:
        var value = marker_values[marker]
        var position_values: Array = value if value is Array else value.get("point_blender_m",value.get("position",value.get("xyz",[])))
        if position_values.size() != 3: continue
        var world: Vector3 = host.references.local_to_world(4,xyz(position_values))
        var marker_source := "authored_geometry_marker"
        if marker == "foreground_character_head_top" and host.references.people[4].size()>0:
            var actor = host.references.people[4][0]
            var deformed: ArrayMesh = actor.visual_mesh.bake_mesh_from_current_skeleton_pose()
            var highest := -INF
            for surface in range(deformed.get_surface_count()):
                var arrays: Array = deformed.surface_get_arrays(surface)
                var vertices: PackedVector3Array = arrays[Mesh.ARRAY_VERTEX]
                for local_vertex in vertices:
                    var candidate_world: Vector3 = actor.visual_mesh.global_transform*local_vertex
                    if candidate_world.y>highest:
                        highest=candidate_world.y
                        world=candidate_world
            marker_source = "actual_skinned_mesh_highest_vertex"
        var projected: Vector2 = host.eye.unproject_position(world)
        report.projected_landmarks.append({"name":marker,"local_xyz":position_values,
            "pixel":[projected.x,projected.y],"uv":[projected.x/1600.0,projected.y/900.0],
            "marker_source":marker_source,"behind_camera":host.eye.is_position_behind(world)})
    report.checks.geographic_anchor = host.references.sites[4].root.global_position.is_equal_approx(Vector3(-163,0,-18))
    report.checks.distinct_towns = host.references.sites[4].root.global_position.distance_to(host.references.sites[5].root.global_position)>7000
    report.checks.replacement_characters = host.references.people[4].size()>0
    report.character_count = host.references.people[4].size()
    report.reference_view_draw_calls = Performance.get_monitor(Performance.RENDER_TOTAL_DRAW_CALLS_IN_FRAME)
    # Oblique motion keeps the same 3D scene, providing parallax evidence.
    var start: Vector3 = host.player.position
    var direction: Vector3 = host.references.sites[4].root.global_basis*Vector3(1.0,0,-1.0)
    for frame in range(91):
        await process_frame
        host.player.position = start+direction*(float(frame)/90.0)
        if frame in [0,45,90]:
            await RenderingServer.frame_post_draw
            var filename := "motion_%03d.png" % frame
            report.checks[filename] = root.get_texture().get_image().save_png(destination+"/"+filename)==OK
            report.screenshots.append(filename)
    # Separate geometry inspection camera; no changed light or materials.
    host.references.set_view(4,"overview")
    report.checks.overview_saved = await frame_file(host,"overview.png")
    report.screenshots.append("overview.png")
    host.references.set_view(4,"close")
    report.checks.close_saved = await frame_file(host,"close.png")
    report.screenshots.append("close.png")
    for detail in [{"name":"gate_detail", "camera":Vector3(-1,13,5.5), "target":Vector3(0,23,6.5)},
        {"name":"tower_detail", "camera":Vector3(-2,15,11.5), "target":Vector3(5.0,25.8,13.5)}]:
        host.references.set_view(4)
        host.player.position = host.references.local_to_world(4,detail.camera)-Vector3(0,1.68,0)
        host.player.rotation = Vector3.ZERO
        host.eye.rotation = Vector3.ZERO
        host.eye.fov = 42.0
        host.eye.look_at(host.references.local_to_world(4,detail.target))
        host.player.rotation.y = host.eye.rotation.y
        host.eye.rotation.y = 0
        report.checks[detail.name+"_saved"] = await frame_file(host,detail.name+".png")
        report.screenshots.append(detail.name+".png")
    # The new gate must remain physically traversable with the real CharacterBody.
    var crown: Array = marker_values.get("gate_opening_crown",[0,21.8,6])
    if crown is Array and crown.size() == 3:
        host.player.position = host.references.local_to_world(4,Vector3(crown[0],crown[1]-3,.35))
        host.player.velocity = Vector3.ZERO
        var walk_start: Vector3 = host.player.position
        var heading: Vector3 = host.references.sites[4].root.global_basis*Vector3(0,0,-1)
        for step in range(150):
            await physics_frame
            host.player.velocity = Vector3(heading.x*3,host.player.velocity.y-18.0/60.0,heading.z*3)
            host.player.move_and_slide()
        var travel: Vector3 = host.player.position-walk_start
        report.gate_walk_distance_m = Vector2(travel.x,travel.z).length()
        report.gate_walk_finished_on_floor = host.player.is_on_floor()
        report.checks.gate_walk = report.gate_walk_distance_m>6.5 and host.player.is_on_floor()
    report.technical_passed = true
    for value in report.checks.values(): report.technical_passed = report.technical_passed and value
    FileAccess.open(destination+"/capture.json",FileAccess.WRITE).store_string(JSON.stringify(report,"\t"))
    print("LEVEL0_MARKET_V3_CAPTURE "+JSON.stringify(report))
    quit(0 if report.technical_passed else 1)
