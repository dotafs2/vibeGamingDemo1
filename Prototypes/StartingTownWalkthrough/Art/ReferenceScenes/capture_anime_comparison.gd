extends SceneTree
## Camera-only comparison capture. Does not change project presets or scene assets.
const OUT := "res://validation/anime_comparison"

func _initialize() -> void:
    call_deferred("capture")

func capture() -> void:
    var host = load("res://main.tscn").instantiate()
    root.add_child(host)
    current_scene = host
    host.capturing = true
    host.caption.get_parent().get_parent().visible = false
    DirAccess.make_dir_recursive_absolute(ProjectSettings.globalize_path(OUT))
    var cases := [
        {"name":"market_matched", "site":4, "camera":Vector3(0,-19,2.25), "target":Vector3(-1.8,25,5.4), "hfov":70.0},
        {"name":"tolbana_matched", "site":5, "camera":Vector3(10,-24,2.4), "target":Vector3(-2,16,.4), "hfov":66.0},
        {"name":"tolbana_height_check", "site":5, "camera":Vector3(10,-24,3.0), "target":Vector3(-2,16,1.0), "hfov":66.0}
    ]
    var report: Dictionary = {"engine":Engine.get_version_info().string,
        "renderer":RenderingServer.get_current_rendering_method(), "camera_only":true,
        "asset_revision":host.references.manifest.style_id, "global_style_approved":false,
        "kimi_requests":0, "captures":[], "passed":true}
    for item in cases:
        host.references.set_view(item.site)
        host.player.position = host.references.local_to_world(item.site,item.camera)-Vector3(0,1.68,0)
        host.player.rotation = Vector3.ZERO
        host.eye.rotation = Vector3.ZERO
        host.eye.fov = rad_to_deg(2.0*atan(tan(deg_to_rad(item.hfov)*.5)/(1600.0/900.0)))
        host.eye.look_at(host.references.local_to_world(item.site,item.target))
        host.player.rotation.y = host.eye.rotation.y
        host.eye.rotation.y = 0
        # Allow TAA and shadow history to settle after the camera jump.
        for frame in range(48):
            await process_frame
        await RenderingServer.frame_post_draw
        var picture := root.get_texture().get_image()
        var filename: String = OUT+"/"+item.name+".png"
        var saved: bool = picture.save_png(filename) == OK
        report.passed = report.passed and saved and picture.get_width() == 1600 and picture.get_height() == 900
        report.captures.append({"file":filename,"site":item.site,
            "camera_blender_m":[item.camera.x,item.camera.y,item.camera.z],
            "target_blender_m":[item.target.x,item.target.y,item.target.z],
            "horizontal_fov_degrees":item.hfov,"world_camera":str(host.eye.global_position),
            "scene_world_origin":str(host.references.sites[item.site].root.global_position),
            "saved":saved,"width":picture.get_width(),"height":picture.get_height()})
    FileAccess.open(OUT+"/capture.json",FileAccess.WRITE).store_string(JSON.stringify(report,"\t"))
    print("LEVEL0_ANIME_COMPARISON "+JSON.stringify(report))
    quit(0 if report.passed else 1)
