extends SceneTree
## Both crafted scenes at their existing region anchors; global style awaits user review.

func _initialize() -> void:
    call_deferred("launch_demo")

func launch_demo() -> void:
    var candidate = JSON.parse_string(FileAccess.get_file_as_string("res://assets/reference_scenes/market_craft_v5_manifest.json"))
    var tolbana = JSON.parse_string(FileAccess.get_file_as_string("res://assets/reference_scenes/tolbana_craft_v5_manifest.json"))
    var manifest = JSON.parse_string(FileAccess.get_file_as_string("res://assets/reference_scenes/scene_manifest.json"))
    if not candidate is Dictionary or not tolbana is Dictionary or not manifest is Dictionary:
        push_error("Craft demo manifest missing")
        quit(1)
        return
    manifest.scenes[0] = candidate.scenes[0]
    manifest.scenes[1] = tolbana.scenes[0]
    manifest.style_id = "level0_two_towns_craft_v5"
    var folder := "res://validation/market_craft_v5"
    DirAccess.make_dir_recursive_absolute(ProjectSettings.globalize_path(folder))
    var path := folder+"/active_demo_manifest.json"
    FileAccess.open(path,FileAccess.WRITE).store_string(JSON.stringify(manifest,"\t"))
    var host = load("res://main.tscn").instantiate()
    host.reference_manifest_path = path
    root.add_child(host)
    current_scene = host
    if "--tolbana" in OS.get_cmdline_user_args():
        host.references.set_view(5)
