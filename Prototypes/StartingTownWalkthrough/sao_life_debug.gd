extends Node3D
## A read-only projection of the existing SAO world, independent of missing art packs.
const EXPECTED_WORLD := "F4390752-4A07-7DE7-FACD-32BAC6F72C54"
const WORLD_ORIGIN_CM := Vector3(0, 465000, 0)
var snapshot: Dictionary = {}
var snapshot_path := ""
var status: Label
var details: Label
var actors: Node3D
var hud_panel: ColorRect
var details_scroll: ScrollContainer
var refresh_elapsed := 0.0
var last_file_time := -1
var load_error := ""
var active_ids: Array[String] = []
func _arg_value(flag: String, fallback: String = "") -> String:
    var args := OS.get_cmdline_user_args(); var i := args.find(flag)
    return str(args[i + 1]) if i >= 0 and i + 1 < args.size() else fallback
func _ready() -> void:
    Engine.max_fps = 20
    var layer := CanvasLayer.new(); add_child(layer)
    hud_panel = ColorRect.new(); hud_panel.color = Color(0.035, 0.05, 0.08, 0.94); hud_panel.position = Vector2(18,18); layer.add_child(hud_panel)
    status = Label.new(); status.position = Vector2(16,12); status.size = Vector2(548,180); status.add_theme_font_size_override("font_size",18); status.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART; hud_panel.add_child(status)
    details_scroll = ScrollContainer.new(); details_scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED; details_scroll.position = Vector2(16,205); details_scroll.size_flags_horizontal = Control.SIZE_EXPAND_FILL; details_scroll.size_flags_vertical = Control.SIZE_EXPAND_FILL; hud_panel.add_child(details_scroll)
    details = Label.new(); details.size_flags_horizontal = Control.SIZE_EXPAND_FILL; details.custom_minimum_size = Vector2(528,0); details.add_theme_font_size_override("font_size",17); details.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART; details_scroll.add_child(details)
    get_viewport().size_changed.connect(_layout_hud); _layout_hud()
    actors = Node3D.new(); actors.name = "SnapshotGeometry"; add_child(actors)
    var floor_mesh := PlaneMesh.new(); floor_mesh.size = Vector2(100,110); var floor_node := MeshInstance3D.new(); floor_node.mesh = floor_mesh; var floor_mat := StandardMaterial3D.new(); floor_mat.albedo_color = Color("263840"); floor_node.material_override = floor_mat; add_child(floor_node)
    var camera := get_node("Camera3D") as Camera3D; camera.projection = Camera3D.PROJECTION_ORTHOGONAL; camera.size = 105; camera.position = Vector3(-18,75,90); camera.look_at(Vector3(-18,0,-4))
    snapshot_path = _arg_value("--snapshot", "res://validation/sao_life_snapshot.json"); load_snapshot()
    if "--validate" in OS.get_cmdline_user_args(): call_deferred("_validate")
func _layout_hud() -> void:
    if hud_panel == null: return
    var h := maxf(300.0, get_viewport().get_visible_rect().size.y - 36.0)
    hud_panel.size = Vector2(580, h)
    details_scroll.size = Vector2(548, maxf(80.0, h - 221.0))
func _vector_ok(value: Variant) -> bool:
    if not value is Array or value.size() != 3:
        return false
    for component in value:
        if not (component is int or component is float) or not is_finite(float(component)):
            return false
    return true

func _snapshot_ok(value: Variant) -> bool:
    if not value is Dictionary or value.get("schema") != 1 or value.get("world_id") != EXPECTED_WORLD:
        return false
    var life: Variant = value.get("life")
    if not life is Dictionary or not life.get("residents") is Array or life.residents.size() != 13:
        return false
    var ids := {}
    for r in life.residents:
        if not r is Dictionary or not r.get("stable_id") is String or r.stable_id.is_empty() or ids.has(r.stable_id):
            return false
        ids[r.stable_id] = true
        if not r.get("active") is bool or not _vector_ok(r.get("position_cm")) or not r.get("account") is Dictionary:
            return false
    if not life.get("buildings", []) is Array:
        return false
    for b in life.get("buildings", []):
        if not b is Dictionary or not b.get("building_id") is String or not _vector_ok(b.get("position_cm")):
            return false
    var food_resource: Variant = life.get("public_food_resource")
    if food_resource != null:
        if not food_resource is Dictionary or food_resource.get("source_id") != "starter_commons_berry_patch":
            return false
        if not _vector_ok(food_resource.get("visual_position_cm")):
            return false
        for key in ["stock", "produced_total", "harvested_total"]:
            var amount: Variant = food_resource.get(key)
            if not (amount is int or amount is float) or not is_finite(float(amount)) or amount < 0:
                return false
    return true

func load_snapshot() -> void:
    if not FileAccess.file_exists(snapshot_path):
        load_error = "Snapshot missing; previous display retained."
        _status()
        return
    var parsed: Variant = JSON.parse_string(FileAccess.get_file_as_string(snapshot_path))
    if not _snapshot_ok(parsed):
        load_error = "Invalid snapshot; previous display retained."
        _status()
        return
    snapshot = parsed
    load_error = ""
    last_file_time = FileAccess.get_modified_time(snapshot_path)
    for child in actors.get_children():
        actors.remove_child(child)
        child.queue_free()
    active_ids.clear()
    var text := ""
    var resource: Variant = snapshot.life.get("public_food_resource")
    if resource is Dictionary:
        text += "公共浆果地 · 库存 " + str(resource.stock) + "/3 · 实际生长 " + str(resource.produced_total) + " · 已采集 " + str(resource.harvested_total) + "\n"
        text += "初始3份为开发者配置；未满时每30分钟现实运行生长1份。\n\n"
        _food_resource(resource)
    for b in snapshot.life.get("buildings", []):
        _building(b)
    for r in snapshot.life.residents:
        if not r.active:
            continue
        active_ids.append(r.stable_id)
        _resident(r)
        text += str(r.get("name", "")) + "  |  Col " + str(r.account.get("coins_col", 0)) + "  |  " + str(r.get("phase", "")) + "\n"
        text += "目标：" + str(r.get("self_goal", "")).left(120) + "\n"
        text += "需要：" + str(r.get("self_need", "")).left(110) + "\n"
        text += "实际结果：" + str(r.get("result", "")).left(150) + "\n"
        var body_state: Dictionary = r.get("survival", {})
        if body_state.get("installed", false):
            text += "饱腹 " + str(body_state.get("hunger_satisfaction")) + " / 精力 " + str(body_state.get("energy")) + " / 口粮 " + str(body_state.get("food")) + "\n"
        text += "木料 " + str(r.account.get("wood", 0)) + "  铁料 " + str(r.account.get("iron", 0)) + "  柴火 " + str(r.account.get("kindling", 0)) + "\n\n"
    details.text = text
    _status()

func _position(p: Array) -> Vector3:
    var v := Vector3(float(p[0]), float(p[1]), float(p[2])) - WORLD_ORIGIN_CM
    return Vector3(v.x, v.z, v.y) / 100.0

func _building(b: Dictionary) -> void:
    var body := MeshInstance3D.new()
    body.name = "DebugBuilding_" + b.building_id
    var mesh := BoxMesh.new()
    mesh.size = Vector3(9, 0.15, 9)
    body.mesh = mesh
    body.position = _position(b.position_cm) + Vector3(0, 0.1, 0)
    var mat := StandardMaterial3D.new()
    mat.albedo_color = Color("486779")
    body.material_override = mat
    actors.add_child(body)

func _food_resource(resource: Dictionary) -> void:
    var body := MeshInstance3D.new()
    var mesh := SphereMesh.new()
    mesh.radius = 0.7
    mesh.height = 1.4
    body.mesh = mesh
    body.position = _position(resource.visual_position_cm) + Vector3(0, 0.7, 0)
    var mat := StandardMaterial3D.new()
    mat.albedo_color = Color("75934d")
    body.material_override = mat
    actors.add_child(body)
    var label := Label3D.new()
    label.text = "公共浆果地 · " + str(resource.stock) + "/3"
    label.position = body.position + Vector3(0, 3, 0)
    label.billboard = BaseMaterial3D.BILLBOARD_ENABLED
    label.no_depth_test = true
    label.font_size = 42
    label.pixel_size = 0.025
    actors.add_child(label)

func _resident(r: Dictionary) -> void:
    var root := Node3D.new()
    root.name = "ResidentProjection_" + r.stable_id
    root.position = _position(r.position_cm)
    actors.add_child(root)
    var body := MeshInstance3D.new()
    var mesh := CapsuleMesh.new()
    mesh.height = 1.8
    mesh.radius = 0.4
    body.mesh = mesh
    var mat := StandardMaterial3D.new()
    mat.albedo_color = Color("f3b86d")
    body.material_override = mat
    root.add_child(body)
    var label := Label3D.new()
    label.text = str(r.get("name", "")) + " · " + str(r.get("phase", ""))
    label.position.y = 3
    label.billboard = BaseMaterial3D.BILLBOARD_ENABLED
    label.no_depth_test = true
    label.font_size = 48
    label.pixel_size = 0.025
    root.add_child(label)

func _status() -> void:
    if status == null:
        return
    if snapshot.is_empty():
        status.text = "SAO 生命循环 · 调试观察窗\n" + load_error
        return
    var source_stamp: Variant = snapshot.get("source_world_mtime")
    var age := -1.0
    if source_stamp is int or source_stamp is float:
        age = maxf(0, Time.get_unix_time_from_system() - float(source_stamp))
    var freshness := "SOURCE AGE UNKNOWN" if age < 0 else ("STALE" if age > 120 else "FRESH")
    status.text = "SAO 生命循环 · 同一世界\n13 个持久身份 / " + str(active_ids.size()) + " 人活跃 / 事件 " + str(snapshot.life.seq)
    status.text += "\n" + str(snapshot.world_id) + "\n" + freshness + " · 源存档年龄 " + str(roundi(age)) + " 秒"
    status.text += "\n只读 UE 社会投影；简化几何不代表美术成品。\n意图不等于完成；自动读取快照，R 手动刷新。"
    if not load_error.is_empty():
        status.text += "\n" + load_error

func _process(delta: float) -> void:
    refresh_elapsed += delta
    if refresh_elapsed >= 2:
        refresh_elapsed = 0
        if FileAccess.file_exists(snapshot_path) and FileAccess.get_modified_time(snapshot_path) != last_file_time:
            load_snapshot()
        _status()

func _unhandled_input(event: InputEvent) -> void:
    if event is InputEventKey and event.pressed and not event.echo and event.keycode == KEY_R:
        load_snapshot()

func _validate() -> void:
    await get_tree().process_frame
    var initial_count := actors.get_child_count()
    load_snapshot()
    await get_tree().process_frame
    var ok := not snapshot.is_empty() and active_ids.size() == 3 and actors.get_child_count() == initial_count
    var report := {"world_id": snapshot.get("world_id"), "active_ids": active_ids, "resident_count": 13 if not snapshot.is_empty() else 0,
        "life_seq": snapshot.get("life", {}).get("seq"), "refresh_does_not_duplicate": actors.get_child_count() == initial_count,
        "read_only": true, "godot_controls_npc": false, "source_world_mtime": snapshot.get("source_world_mtime"), "passed": ok}
    var capture_path := _arg_value("--capture")
    if not capture_path.is_empty() and DisplayServer.get_name() != "headless":
        await RenderingServer.frame_post_draw
        report["capture_error"] = get_viewport().get_texture().get_image().save_png(capture_path)
    var report_path := _arg_value("--report")
    if not report_path.is_empty():
        var file := FileAccess.open(report_path, FileAccess.WRITE)
        if file != null:
            file.store_string(JSON.stringify(report, "  "))
    print("SAO_LIFE_VIEWER_VALIDATION " + JSON.stringify(report))
    get_tree().quit(0 if ok else 1)
