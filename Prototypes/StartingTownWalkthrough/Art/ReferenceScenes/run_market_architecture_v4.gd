extends SceneTree
## Run the architecture candidate awaiting visual acceptance at its existing world anchor, retaining Tolbana.

func _initialize() -> void:
    call_deferred("launch_demo")

func launch_demo() -> void:
    var candidate = JSON.parse_string(FileAccess.get_file_as_string("res://assets/reference_scenes/market_architecture_v4_manifest.json"))
    var manifest = JSON.parse_string(FileAccess.get_file_as_string("res://assets/reference_scenes/scene_manifest.json"))
    if not candidate is Dictionary or not manifest is Dictionary:
        push_error("Market demo manifest missing")
        quit(1)
        return
    manifest.scenes[0] = candidate.scenes[0]
    manifest.style_id = "level0_market_architecture_v4"
    var folder := "res://validation/market_architecture_v4"
    DirAccess.make_dir_recursive_absolute(ProjectSettings.globalize_path(folder))
    var path := folder+"/active_demo_manifest.json"
    FileAccess.open(path,FileAccess.WRITE).store_string(JSON.stringify(manifest,"\t"))
    var host = load("res://main.tscn").instantiate()
    host.reference_manifest_path = path
    root.add_child(host)
    current_scene = host
