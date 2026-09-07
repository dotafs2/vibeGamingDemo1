"""Render the daily art catalog with Unreal's native editor renderer.

The parent process starts an isolated hidden UnrealEditor and executes this file
with ExecutePythonScript.  The script deliberately uses the native mesh assets
in the catalog; it does not import GLB/FBX files, save a map, or touch source
assets.  Run examples from the parent wrapper:

    -ExecutePythonScript=.../render_daily_art_catalog.py
    -DailyArtOnly=oak

The catalog contract is Saved/ThreeHearths/ArtCatalog/catalog.json with
schema=1 and entries containing native ``mesh_path`` values and centimetre
transforms.
"""

from __future__ import annotations

import json
import math
import re
import time
from pathlib import Path

import unreal as ue


PROJECT = Path(__file__).resolve().parents[1]
REPO = PROJECT.parent
CATALOG_DEFAULT = PROJECT / "Saved/ThreeHearths/ArtCatalog/catalog.json"
WORK = REPO / ".codex-ue58-diagnostics/daily-art-20260907"
THUMBS = WORK / "thumbs"
MANIFEST_PATH = WORK / "render-manifest.json"
REPORT_PATH = WORK / "render-report.json"
WIDTH = 960
HEIGHT = 720
ITEM_DELAY_SECONDS = 3.0
WALL_CAP_SECONDS = 180.0
REQUIRED_IDS = {
    "rowhouse",
    "shop_house",
    "courtyard_workshop",
    "warehouse",
    "inn",
    "royal_keep_garden_v2",
    "oak",
    "birch",
    "orchard",
    "cypress",
    "flowering_shrub",
    "wildflowers",
}
HOUSING_IDS = {"rowhouse", "shop_house", "courtyard_workshop", "warehouse", "inn"}
PLANT_IDS = {"oak", "birch", "orchard", "cypress", "flowering_shrub", "wildflowers"}
TINT_MATERIAL_PATH = "/Game/ThreeHearths/Materials/M_VillageTint"
TINT_PARAMETER = "VillageTint"
OWN_PREFIX = "DailyArtCatalog_"


def _command_line_options():
    """Read wrapper options without consuming Unreal's command-line flags."""
    command_line = ue.SystemLibrary.get_command_line()
    _, switches, arguments = ue.SystemLibrary.parse_command_line(command_line)
    only = arguments.get("DailyArtOnly", "")
    if not only:
        match = re.search(r"(?:^|\s)-DailyArtOnly=([^\s]+)", command_line)
        only = match.group(1) if match else ""
    catalog = arguments.get("DailyArtCatalog", "")
    return {
        "only": {part.strip() for part in only.split(",") if part.strip()} - {"all"},
        "catalog": Path(catalog) if catalog else CATALOG_DEFAULT,
        "switches": switches,
    }


def _read_catalog(path):
    if not path.exists():
        raise FileNotFoundError("Missing native catalog: %s" % path)
    data = json.loads(path.read_text(encoding="utf-8"))
    if data.get("schema") != 1 or not isinstance(data.get("entries"), list):
        raise ValueError("Expected catalog contract {schema:1, entries:[...]}")
    if len(data["entries"]) != 12:
        raise ValueError("Expected 12 daily art entries, got %d" % len(data["entries"]))
    ids = {entry.get("id") for entry in data["entries"]}
    if ids != REQUIRED_IDS:
        raise ValueError("Daily art IDs differ: expected %s, got %s" % (sorted(REQUIRED_IDS), sorted(ids)))
    for entry in data["entries"]:
        if not entry.get("name_zh") or not entry.get("kind") or not isinstance(entry.get("parts"), list) or not entry["parts"]:
            raise ValueError("Invalid entry shape: %s" % entry.get("id"))
        for part in entry["parts"]:
            required = ("mesh_path", "offset_cm", "yaw_degrees", "scale", "center_native_bounds")
            if any(key not in part for key in required):
                raise ValueError("Part in %s is missing a required transform field" % entry["id"])
            if len(part["offset_cm"]) != 3 or len(part["scale"]) != 3:
                raise ValueError("Bad transform vector in %s" % entry["id"])
            color = part.get("color_linear")
            if color is not None and len(color) != 4:
                raise ValueError("color_linear must be RGBA or null in %s" % entry["id"])
    return data


def _vec(value):
    return ue.Vector(float(value[0]), float(value[1]), float(value[2]))


def _rotate_yaw(vector, yaw_degrees):
    radians = math.radians(float(yaw_degrees))
    cosine, sine = math.cos(radians), math.sin(radians)
    return ue.Vector(
        vector.x * cosine - vector.y * sine,
        vector.x * sine + vector.y * cosine,
        vector.z,
    )


def _scaled(vector, scale):
    return ue.Vector(vector.x * float(scale[0]), vector.y * float(scale[1]), vector.z * float(scale[2]))


def _normalized(vector):
    length = math.sqrt(vector.x ** 2 + vector.y ** 2 + vector.z ** 2)
    return vector / length


def _asset(path):
    mesh = ue.load_asset(path)
    if mesh is None or not isinstance(mesh, ue.StaticMesh):
        raise RuntimeError("Native StaticMesh unavailable: %s" % path)
    return mesh


def _bounds_for_part(mesh, offset, yaw, scale, center_native_bounds):
    """Return world AABB after applying the catalog's native UE transform."""
    bounds = mesh.get_bounds()
    source_center = _rotate_yaw(_scaled(bounds.origin, scale), yaw)
    location = offset - source_center if center_native_bounds else offset
    extent = bounds.box_extent
    scaled_extent = _scaled(extent, [abs(float(value)) for value in scale])
    radians = math.radians(float(yaw))
    cosine, sine = abs(math.cos(radians)), abs(math.sin(radians))
    world_extent = ue.Vector(
        cosine * scaled_extent.x + sine * scaled_extent.y,
        sine * scaled_extent.x + cosine * scaled_extent.y,
        abs(scaled_extent.z),
    )
    center = location + source_center
    return center - world_extent, center + world_extent, location


def _aabb_points(minimum, maximum):
    for x in (minimum.x, maximum.x):
        for y in (minimum.y, maximum.y):
            for z in (minimum.z, maximum.z):
                yield ue.Vector(x, y, z)


def _look_at(location, target):
    direction = target - location
    horizontal = math.sqrt(direction.x * direction.x + direction.y * direction.y)
    pitch = math.degrees(math.atan2(direction.z, horizontal))
    yaw = math.degrees(math.atan2(direction.y, direction.x))
    return ue.Rotator(pitch=pitch, yaw=yaw, roll=0.0)


class DailyArtRenderer:
    def __init__(self, catalog, catalog_path, only):
        self.catalog = catalog
        self.catalog_path = catalog_path
        self.entries = [entry for entry in catalog["entries"] if not only or entry["id"] in only]
        unknown = only - {entry["id"] for entry in catalog["entries"]}
        if unknown:
            raise ValueError("Unknown -DailyArtOnly IDs: %s" % sorted(unknown))
        self.started = time.monotonic()
        self.next_due = self.started
        self.entry_index = 0
        self.stage = "load"
        self.staged_parts = []
        self.owned_actors = []
        self.capture_actor = None
        self.capture_component = None
        self.render_target = None
        self.tint_material = None
        self.tick_handle = None
        self.unregister_tick = None
        self.records = []
        self.error = None
        WORK.mkdir(parents=True, exist_ok=True)
        THUMBS.mkdir(parents=True, exist_ok=True)
        self._write_manifest("started")

    def _write_manifest(self, state, **extra):
        payload = {
            "schema": 1,
            "state": state,
            "catalog": str(self.catalog_path),
            "output_directory": str(WORK),
            "thumbnail_directory": str(THUMBS),
            "resolution": [WIDTH, HEIGHT],
            "renderer": "Unreal native SceneCapture2D / FinalColorLDR",
            "lighting": "neutral studio directional key + fill + skylight",
            "native_transform": "native UE centimetres and UE yaw; center uses mesh.bounds.origin * scale rotated by yaw",
            "source_baseline": "2366a8b 2026-09-07 10:02; source GLBs unchanged; 3 existing Blend source files differ",
            "catalog_metadata": self.catalog.get("metadata", {}),
            "entries_requested": [entry["id"] for entry in self.entries],
            "records": self.records,
        }
        payload.update(extra)
        MANIFEST_PATH.write_text(json.dumps(payload, ensure_ascii=False, indent=2), encoding="utf-8")

    def _spawn_actor(self, actor_class, location=ue.Vector(0, 0, 0), rotation=ue.Rotator(0, 0, 0), label=""):
        actors = ue.get_editor_subsystem(ue.EditorActorSubsystem)
        actor = actors.spawn_actor_from_class(actor_class, location, rotation)
        if actor is None:
            raise RuntimeError("Could not spawn %s" % actor_class)
        self.owned_actors.append(actor)
        if label:
            actor.set_actor_label(OWN_PREFIX + label)
        return actor

    def _setup_world(self):
        if not hasattr(ue, "EditorLoadingAndSavingUtils") or not hasattr(ue.EditorLoadingAndSavingUtils, "new_blank_map"):
            raise RuntimeError("UE Python API lacks EditorLoadingAndSavingUtils.new_blank_map")
        ue.EditorLoadingAndSavingUtils.new_blank_map(False)
        world = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()
        if world is None:
            raise RuntimeError("Transient editor world unavailable")

        key = self._spawn_actor(ue.DirectionalLight, ue.Vector(0, 0, 1600), ue.Rotator(pitch=-50, yaw=125), "Key")
        key.light_component.set_editor_property("intensity", 5.0)
        key.light_component.set_editor_property("light_color", ue.Color(255, 245, 220, 255))
        fill = self._spawn_actor(ue.DirectionalLight, ue.Vector(0, 0, 1200), ue.Rotator(pitch=-30, yaw=30), "Fill")
        fill.light_component.set_editor_property("intensity", 3.0)
        fill.light_component.set_editor_property("cast_shadows", False)
        fill.light_component.set_editor_property("light_color", ue.Color(205, 225, 255, 255))
        for index, yaw in enumerate((220.0, 310.0)):
            bounce = self._spawn_actor(ue.DirectionalLight, rotation=ue.Rotator(pitch=-35, yaw=yaw), label="Bounce%d" % index)
            bounce.light_component.set_editor_property("intensity", 1.6)
            bounce.light_component.set_editor_property("cast_shadows", False)
        ambient = self._spawn_actor(ue.SkyLight, ue.Vector(0, 0, 600), ue.Rotator(0, 0, 0), "Ambient")
        ambient.light_component.set_editor_property("intensity", 0.65)

        self.capture_actor = self._spawn_actor(ue.SceneCapture2D, ue.Vector(0, 0, 0), ue.Rotator(0, 0, 0), "Capture")
        self.capture_component = self.capture_actor.capture_component2d
        self.capture_component.set_editor_property("capture_source", ue.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
        self.capture_component.set_editor_property("projection_type", ue.CameraProjectionMode.ORTHOGRAPHIC)
        self.capture_component.set_editor_property("capture_every_frame", False)
        self.capture_component.set_editor_property("capture_on_movement", False)
        self.capture_component.set_editor_property("always_persist_rendering_state", True)
        self.render_target = ue.RenderingLibrary.create_render_target2d(
            world, WIDTH, HEIGHT, ue.TextureRenderTargetFormat.RTF_RGBA8,
            ue.LinearColor(0.95, 0.92, 0.84, 1.0)
        )
        if self.render_target is None:
            raise RuntimeError("RenderingLibrary.create_render_target2d failed")
        self.render_target.set_editor_property("clear_color", ue.LinearColor(0.95, 0.92, 0.84, 1.0))
        self.capture_component.set_editor_property("texture_target", self.render_target)
        self.tint_material = ue.load_asset(TINT_MATERIAL_PATH)
        if self.tint_material is None:
            raise RuntimeError("Missing plant override material: %s" % TINT_MATERIAL_PATH)
        post = self.capture_component.get_editor_property("post_process_settings")
        for prop, value in {
            "override_auto_exposure_min_brightness": True,
            "override_auto_exposure_max_brightness": True,
            "auto_exposure_min_brightness": 1.0,
            "auto_exposure_max_brightness": 1.0,
            "override_auto_exposure_bias": True,
            "auto_exposure_bias": 0.0,
            "override_vignette_intensity": True,
            "vignette_intensity": 0.0,
        }.items():
            post.set_editor_property(prop, value)
        self.capture_component.set_editor_property("post_process_settings", post)
        self.capture_component.set_editor_property("post_process_blend_weight", 1.0)

    def _apply_catalog_color(self, component, color, actor):
        if color is None:
            return
        dynamic = component.create_dynamic_material_instance(0, self.tint_material)
        if dynamic is None:
            raise RuntimeError("Could not create M_VillageTint instance")
        dynamic.set_vector_parameter_value(TINT_PARAMETER, ue.LinearColor(*[float(x) for x in color]))
        material_count = component.get_num_materials()
        for slot in range(material_count):
            component.set_material(slot, dynamic)

    def _clear_staged_parts(self):
        actors = ue.get_editor_subsystem(ue.EditorActorSubsystem)
        for actor in list(self.staged_parts):
            if actor in self.owned_actors:
                self.owned_actors.remove(actor)
            if actor is not None and actors.destroy_actor(actor):
                pass
        self.staged_parts = []

    def _stage_entry(self, entry):
        self._clear_staged_parts()
        minimum = ue.Vector(float("inf"), float("inf"), float("inf"))
        maximum = ue.Vector(float("-inf"), float("-inf"), float("-inf"))
        part_records = []
        for part_index, part in enumerate(entry["parts"]):
            mesh = _asset(part["mesh_path"])
            offset = _vec(part["offset_cm"])
            scale = part["scale"]
            yaw = float(part["yaw_degrees"])
            bounds_min, bounds_max, location = _bounds_for_part(
                mesh, offset, yaw, scale, bool(part["center_native_bounds"])
            )
            minimum = ue.Vector(min(minimum.x, bounds_min.x), min(minimum.y, bounds_min.y), min(minimum.z, bounds_min.z))
            maximum = ue.Vector(max(maximum.x, bounds_max.x), max(maximum.y, bounds_max.y), max(maximum.z, bounds_max.z))
            # UE Python's positional field order differs from C++ FRotator.
            actor = self._spawn_actor(ue.StaticMeshActor, location, ue.Rotator(pitch=0, yaw=yaw, roll=0), "%s_part_%03d" % (entry["id"], part_index))
            actual_rotation = actor.get_actor_rotation()
            yaw_error = abs((actual_rotation.yaw - yaw + 180.0) % 360.0 - 180.0)
            if yaw_error > 0.001 or abs(actual_rotation.pitch) > 0.001 or abs(actual_rotation.roll) > 0.001:
                raise RuntimeError("Native part rotation differs from catalog: %s/%d" % (entry["id"], part_index))
            component = actor.static_mesh_component
            component.set_static_mesh(mesh)
            component.set_mobility(ue.ComponentMobility.MOVABLE)
            component.set_collision_enabled(ue.CollisionEnabled.NO_COLLISION)
            actor.set_actor_scale3d(_vec(scale))
            self._apply_catalog_color(component, part.get("color_linear"), actor)
            self.staged_parts.append(actor)
            part_records.append({
                "mesh_path": part["mesh_path"],
                "asset_name": mesh.get_name(),
                "actor_label": actor.get_actor_label(),
                "offset_cm": list(part["offset_cm"]),
                "yaw_degrees": yaw,
                "scale": list(scale),
                "center_native_bounds": bool(part["center_native_bounds"]),
                "color_linear": part.get("color_linear"),
                "bounds_origin_cm": [bounds_min.x, bounds_min.y, bounds_min.z],
                "bounds_max_cm": [bounds_max.x, bounds_max.y, bounds_max.z],
            })
        # A disposable studio floor gives shadows and a neutral background.
        floor = self._spawn_actor(ue.StaticMeshActor,
            ue.Vector((minimum.x + maximum.x) * 0.5, (minimum.y + maximum.y) * 0.5, minimum.z - 2),
            label="StudioFloor")
        floor.static_mesh_component.set_static_mesh(_asset("/Engine/BasicShapes/Plane.Plane"))
        span = max(maximum.x - minimum.x, maximum.y - minimum.y, maximum.z - minimum.z, 100.0) * 30.0
        floor.set_actor_scale3d(ue.Vector(span / 100.0, span / 100.0, 1.0))
        self._apply_catalog_color(floor.static_mesh_component, [0.72, 0.70, 0.64, 1.0], floor)
        self.staged_parts.append(floor)
        self._position_capture(minimum, maximum)
        self.capture_component.capture_scene()
        return minimum, maximum, part_records

    def _position_capture(self, minimum, maximum):
        center = (minimum + maximum) * 0.5
        points = list(_aabb_points(minimum, maximum))
        direction = _normalized(ue.Vector(6.0, -9.0, 6.0))
        diagonal = maximum - minimum
        length = math.sqrt(diagonal.x ** 2 + diagonal.y ** 2 + diagonal.z ** 2)
        camera_location = center + direction * max(length, 1000.0) * 4.0
        right = _normalized(ue.Vector(-direction.y, direction.x, 0.0))
        up = _normalized(ue.Vector(-direction.x * direction.z, -direction.y * direction.z, direction.x * direction.x + direction.y * direction.y))
        def dot(a, b):
            return a.x * b.x + a.y * b.y + a.z * b.z
        width = max(abs(dot(point - center, right)) for point in points) * 2.0
        height = max(abs(dot(point - center, up)) for point in points) * 2.0
        aspect = WIDTH / float(HEIGHT)
        ortho_width = max(width, height * aspect, 100.0) * 1.22
        self.capture_actor.set_actor_location(camera_location, False, False)
        self.capture_actor.set_actor_rotation(_look_at(camera_location, center), False)
        self.capture_component.set_editor_property("ortho_width", ortho_width)
        ue.log("[DailyArtCatalog] camera=%s rotation=%s width=%.2f bounds=%s/%s" % (
            self.capture_actor.get_actor_location(), self.capture_actor.get_actor_rotation(), ortho_width, minimum, maximum))

    def _capture_entry(self, entry, minimum, maximum, part_records):
        self.capture_component.capture_scene()
        target = THUMBS / (entry["id"] + ".png")
        export_result = ue.RenderingLibrary.export_render_target(
            ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world(),
            self.render_target,
            str(THUMBS),
            target.name,
        )
        if not target.exists():
            # Some UE point releases treat the file argument as a complete name.
            ue.RenderingLibrary.export_render_target(
                ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world(),
                self.render_target,
                str(THUMBS),
                target.name,
            )
        if not target.exists():
            raise RuntimeError("Render target export did not create %s (return=%s)" % (target, export_result))
        record = {
            "id": entry["id"],
            "name_zh": entry["name_zh"],
            "kind": entry["kind"],
            "entry_metadata": entry.get("metadata", {}),
            "part_count": len(part_records),
            "thumbnail": str(target),
            "resolution": [WIDTH, HEIGHT],
            "parts": part_records,
            "world_bounds_cm": {
                "min": [minimum.x, minimum.y, minimum.z],
                "max": [maximum.x, maximum.y, maximum.z],
            },
            "capture": {
                "renderer": "Unreal SceneCapture2D",
                "capture_source": "FinalColorLDR",
                "render_target_format": "RTF_RGBA8",
                "camera_pose_direction": [6, -9, 6],
                "background": "neutral studio floor",
                "materials": "native mesh asset materials; catalog color only through M_VillageTint",
            },
        }
        self.records.append(record)
        self._write_manifest("rendering", last_completed=entry["id"])
        ue.log("[DailyArtCatalog] rendered %s -> %s" % (entry["id"], target))

    def _finish(self, state, error=None):
        if self.tick_handle is not None and self.unregister_tick is not None:
            self.unregister_tick(self.tick_handle)
            self.tick_handle = None
        self._clear_staged_parts()
        actors = ue.get_editor_subsystem(ue.EditorActorSubsystem)
        for actor in list(self.owned_actors):
            if actor is not None and actors.destroy_actor(actor):
                pass
        self.owned_actors = []
        self.error = error
        self._write_manifest(state, error=error, completed=len(self.records), finished_seconds=round(time.monotonic() - self.started, 3))
        REPORT_PATH.write_text(json.dumps(self.records, ensure_ascii=False, indent=2), encoding="utf-8")
        if error:
            ue.log_error("[DailyArtCatalog] " + error)
        else:
            ue.log("[DailyArtCatalog] complete: %d item(s)" % len(self.records))
        ue.SystemLibrary.quit_editor()

    def tick(self, *_args):
        if self.error:
            return
        now = time.monotonic()
        if now - self.started > WALL_CAP_SECONDS:
            self._finish("timed_out", "180 second wall cap exceeded")
            return
        if now < self.next_due:
            return
        try:
            if self.stage == "load":
                if self.entry_index >= len(self.entries):
                    self._finish("complete")
                    return
                self._setup_world() if self.capture_actor is None else None
                entry = self.entries[self.entry_index]
                self.current_entry = entry
                self.current_bounds = self._stage_entry(entry)
                self.stage = "capture"
                self.next_due = time.monotonic() + 3.0
                self._write_manifest("asset_loaded", current=entry["id"])
            elif self.stage == "capture":
                entry = self.current_entry
                minimum, maximum, part_records = self.current_bounds
                self._capture_entry(entry, minimum, maximum, part_records)
                self.entry_index += 1
                self.stage = "load"
                self.next_due = now + ITEM_DELAY_SECONDS
        except Exception as exc:
            self._finish("failed", str(exc))

    def start(self):
        ue.EditorPythonScripting.set_keep_python_script_alive(True)
        register = getattr(ue, "register_slate_post_tick_callback", None)
        unregister = getattr(ue, "unregister_slate_post_tick_callback", None)
        if register is None or unregister is None:
            register = getattr(ue, "register_slate_pre_tick_callback", None)
            unregister = getattr(ue, "unregister_slate_pre_tick_callback", None)
        if register is None or unregister is None:
            raise RuntimeError("UE Python tick callback API is unavailable")
        self.unregister_tick = unregister
        self.tick_handle = register(self.tick)
        globals()["_DAILY_ART_RENDERER"] = self
        ue.log("[DailyArtCatalog] staged %d item(s); wall cap=%ss" % (len(self.entries), WALL_CAP_SECONDS))


def main():
    options = _command_line_options()
    try:
        catalog = _read_catalog(options["catalog"])
        renderer = DailyArtRenderer(catalog, options["catalog"], options["only"])
        renderer.start()
    except Exception as exc:
        ue.log_error("[DailyArtCatalog] startup failed: %s" % exc)
        ue.SystemLibrary.quit_editor()


main()
