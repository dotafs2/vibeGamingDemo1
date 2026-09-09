extends SceneTree
## Actual engine captures, collision walks and geographically separate towns.
var destination: String

func _initialize() -> void:
    call_deferred("capture")

func xyz(values: Array) -> Vector3:
    return Vector3(float(values[0]),float(values[1]),float(values[2]))

func frame_file(filename: String) -> bool:
    for frame in range(48): await process_frame
    await RenderingServer.frame_post_draw
    var picture := root.get_texture().get_image()
    return picture.get_width()==1600 and picture.get_height()==900 and picture.save_png(destination+"/"+filename)==OK

func walk(host: Node3D, start: Vector3, direction: Vector3, steps: int, speed: float) -> Dictionary:
    host.player.position = host.references.local_to_world(5,start)
    host.player.velocity = Vector3.ZERO
    var before: Vector3 = host.player.position
    var heading: Vector3 = (host.references.local_to_world(5,start+direction)-before).normalized()
    for step in range(steps):
        await physics_frame
        host.player.velocity = heading*speed+Vector3(0,host.player.velocity.y-18.0/60.0,0)
        host.player.move_and_slide()
    var offset: Vector3 = host.player.position-before
    return {"distance_m":Vector2(offset.x,offset.z).length(),"on_floor":host.player.is_on_floor(),"end_world":str(host.player.position),"frames":steps,"speed_m_s":speed}

func capture() -> void:
    var revision := "r1"
    for arg in OS.get_cmdline_user_args():
        if arg.begins_with("--revision="): revision=arg.trim_prefix("--revision=").validate_filename()
    destination="res://validation/tolbana_craft_v5/"+revision
    DirAccess.make_dir_recursive_absolute(ProjectSettings.globalize_path(destination))
    var market = JSON.parse_string(FileAccess.get_file_as_string("res://assets/reference_scenes/market_craft_v5_manifest.json"))
    var tolbana = JSON.parse_string(FileAccess.get_file_as_string("res://assets/reference_scenes/tolbana_craft_v5_manifest.json"))
    var manifest = JSON.parse_string(FileAccess.get_file_as_string("res://assets/reference_scenes/scene_manifest.json"))
    if not market is Dictionary or not tolbana is Dictionary or not manifest is Dictionary:
        push_error("Craft manifest missing");quit(1);return
    manifest.scenes=[market.scenes[0],tolbana.scenes[0]]
    manifest.style_id="level0_two_towns_craft_v5"
    var manifest_path := destination+"/engine_manifest.json"
    FileAccess.open(manifest_path,FileAccess.WRITE).store_string(JSON.stringify(manifest,"\t"))
    var host = load("res://main.tscn").instantiate()
    host.reference_manifest_path=manifest_path
    root.add_child(host);current_scene=host
    host.capturing=true;host.caption.get_parent().get_parent().visible=false
    var report: Dictionary={"schema":1,"revision":revision,"engine":Engine.get_version_info().string,"renderer":RenderingServer.get_current_rendering_method(),"checks":{},"screenshots":[],"visual_approval":false,"global_style_approved":false,"kimi_requests":0,"source_glb":tolbana.scenes[0].glb,"source_sha256":tolbana.scenes[0].sha256,"market_sha256":market.scenes[0].sha256,"geography":tolbana.scenes[0].godot_position_m}
    host.references.set_view(4)
    report.checks.market_baseline=await frame_file("market_baseline.png");report.screenshots.append("market_baseline.png")
    host.references.set_view(5)
    report.checks.tolbana_baseline=await frame_file("tolbana_baseline.png");report.screenshots.append("tolbana_baseline.png")
    report.checks.geographic_anchor=host.references.sites[5].root.global_position.is_equal_approx(Vector3(0,0,-7670))
    report.checks.town_separation=host.references.sites[4].root.global_position.distance_to(host.references.sites[5].root.global_position)>7000
    report.character_count=host.references.people[5].size()
    report.checks.characters_replaced=report.character_count==3
    var camera_start: Vector3=host.player.position
    for frame in range(91):
        await process_frame
        host.player.position=camera_start+Vector3(1.8,0,-1.2)*(float(frame)/90.0)
        if frame in [0,45,90]:
            var filename: String="motion_%03d.png"%frame
            report.checks[filename]=await frame_file(filename);report.screenshots.append(filename)
    for detail in [
        {"name":"residences","camera":Vector3(-9,10,4.6),"target":Vector3(-12,27,5.0),"fov":56},
        {"name":"inn_corner","camera":Vector3(-13,15,5.7),"target":Vector3(-23,28,5.5),"fov":55},
        {"name":"inn_joinery","camera":Vector3(-19.5,21.6,2.3),"target":Vector3(-22,26.1,1.7),"fov":60},
        {"name":"inn_interior","camera":Vector3(-22,27.6,1.94),"target":Vector3(-22.1,33.7,1.9),"fov":68},
        {"name":"fountain_detail","camera":Vector3(14.5,-12,3.0),"target":Vector3(8.9,-5,2.0),"fov":57},
        {"name":"garden_seating","camera":Vector3(-10,-12,2.0),"target":Vector3(-16,-7.8,.92),"fov":58},
        {"name":"tree_backlight","camera":Vector3(-10,16,3.3),"target":Vector3(-2.5,4,8.6),"fov":68},
        {"name":"overview","camera":Vector3(28,-25,27),"target":Vector3(-2,17,4),"fov":68}]:
        host.references.set_view(5)
        host.player.position=host.references.local_to_world(5,detail.camera)-Vector3(0,1.68,0)
        host.player.rotation=Vector3.ZERO;host.eye.rotation=Vector3.ZERO
        host.eye.fov=float(detail.fov)
        host.eye.look_at(host.references.local_to_world(5,detail.target))
        host.player.rotation.y=host.eye.rotation.y;host.eye.rotation.y=0
        report.checks[detail.name]=await frame_file(detail.name+".png");report.screenshots.append(detail.name+".png")
    report.inn_entry=await walk(host,Vector3(-22,24.4,.36),Vector3(0,1,0),135,2.0)
    report.checks.inn_enterable=report.inn_entry.distance_m>3.9 and report.inn_entry.on_floor
    report.plaza_walk=await walk(host,Vector3(-10,-15,.36),Vector3(1,0,0),180,3.0)
    report.checks.plaza_walk=report.plaza_walk.distance_m>8.4 and report.plaza_walk.on_floor
    report.fountain_collision=await walk(host,Vector3(8.9,-10,.36),Vector3(0,1,0),150,2.0)
    report.checks.fountain_solid=report.fountain_collision.distance_m>1.2 and report.fountain_collision.distance_m<2.3 and report.fountain_collision.on_floor
    report.technical_passed=true
    for value in report.checks.values(): report.technical_passed=report.technical_passed and value
    FileAccess.open(destination+"/capture.json",FileAccess.WRITE).store_string(JSON.stringify(report,"\t"))
    print("LEVEL0_TOLBANA_V5_CAPTURE "+JSON.stringify({"technical_passed":report.technical_passed,"inn_entry":report.inn_entry,"plaza_walk":report.plaza_walk,"fountain_collision":report.fountain_collision}))
    quit(0 if report.technical_passed else 1)
