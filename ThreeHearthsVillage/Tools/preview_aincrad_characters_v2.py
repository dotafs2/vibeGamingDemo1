"""Transient editor preview for the three imported Aincrad V2 characters.

Run from an open Unreal Editor Python console with::

    UnrealEditor.exe CropoutSampleProject.uproject -ExecutePythonScript=<absolute script path> -unattended

The script creates an unsaved temporary editor level, captures twelve native
UE screenshots (front/side for Idle/Walk for each person), writes a report, and
cleans up spawned actors. It never saves a map or changes runtime state.
"""
import json
import datetime
from pathlib import Path

import unreal as ue


ROOT = Path(ue.Paths.project_dir())
OUT = ROOT / "Art" / "AincradCharacters" / "V2" / "Previews"
DEST = "/Game/ThreeHearths/Generated/AincradCharactersV2"
NAMES = ("Aileen", "Takuma", "Kashiwagi")
STATES = ("Idle", "Walk")
VIEWS = ("front", "side")
AUTO_QUIT = __name__ == "__main__"
_RUN = None


def _component(actor, cls):
    result = actor.get_component_by_class(cls)
    assert result, (actor, cls)
    return result


class PreviewRun:
    def __init__(self):
        global _RUN
        OUT.mkdir(parents=True, exist_ok=True)
        # NewBlankMap(false) creates an untitled transient editor world and does
        # not save the current map. It is the native API used by UE's editor
        # build tools for scratch worlds.
        self.world = ue.EditorLoadingAndSavingUtils.new_blank_map(False)
        assert self.world, "Could not create transient blank editor world"
        self.run_id = datetime.datetime.utcnow().strftime("%Y%m%dT%H%M%SZ")
        self.run_dir = OUT / ("run_" + self.run_id)
        self.run_dir.mkdir(parents=True, exist_ok=False)
        self.actors = ue.get_editor_subsystem(ue.EditorActorSubsystem)
        self.spawned = []
        self.camera = self._spawn(ue.CameraActor, ue.Vector(0, -800, 300), ue.Rotator())
        camera = _component(self.camera, ue.CameraComponent)
        camera.set_field_of_view(38.0)
        self._spawn_lighting()
        self._spawn_floor()
        self.characters = {}
        for index, name in enumerate(NAMES):
            mesh_path = f"{DEST}/{name}/SK_{name}"
            mesh = ue.load_asset(mesh_path)
            assert isinstance(mesh, ue.SkeletalMesh), mesh_path
            skeleton = mesh.skeleton
            assert isinstance(skeleton, ue.Skeleton), mesh_path
            anims = {}
            for state in STATES:
                anim_path = f"{DEST}/{name}/AC_{name}_{state}"
                anim = ue.load_asset(anim_path)
                assert isinstance(anim, ue.AnimSequence), anim_path
                assert anim.get_editor_property("skeleton") == skeleton, (anim_path, skeleton.get_path_name())
                anims[state] = anim
            actor = self._spawn(ue.SkeletalMeshActor, ue.Vector((index - 1) * 260, 0, 0), ue.Rotator())
            component = _component(actor, ue.SkeletalMeshComponent)
            component.set_skeletal_mesh_asset(mesh)
            component.set_enable_animation(True)
            # UE otherwise initializes editor-only components in reference pose;
            # native animation evaluation may never replace it in an unattended viewport.
            component.set_editor_property("use_ref_pose_on_init_anim", False)
            component.set_update_animation_in_editor(True)
            component.set_collision_enabled(ue.CollisionEnabled.NO_COLLISION)
            bounds = mesh.get_bounds()
            actor.set_actor_location(ue.Vector((index - 1) * 260, 0, -bounds.origin.z + bounds.box_extent.z), False, False)
            slots = {}
            for slot in mesh.materials:
                slots[str(slot.material_slot_name)] = slot.material_interface.get_path_name() if slot.material_interface else None
            assert slots and all(value for value in slots.values()), (name, slots)
            self.characters[name] = {
                "mesh": mesh, "skeleton": skeleton, "actor": actor,
                "component": component, "animations": anims,
                "bounds_cm": [bounds.box_extent.x * 2, bounds.box_extent.y * 2, bounds.box_extent.z * 2],
                "material_slots": slots,
            }
        self.queue = [(name, state, view) for name in NAMES for state in STATES for view in VIEWS]
        self.samples = {}
        self.pose_transforms = {}
        self.index = 0
        self.pose_wait_ticks = 0
        self.pending_pose = None
        self.waiting_for_file = False
        self.ticks = 0
        self.max_ticks = 900
        self.done = False
        self.tick_handle = ue.register_slate_post_tick_callback(self.tick)
        _RUN = self
        ue.log("AINCRAD_CHARACTERS_V2_PREVIEW_STARTED")

    def _spawn(self, cls, location, rotation):
        actor = self.actors.spawn_actor_from_class(cls, location, rotation)
        assert actor, cls
        self.spawned.append(actor)
        return actor

    def _spawn_floor(self):
        floor = self._spawn(ue.StaticMeshActor, ue.Vector(0, 0, -4), ue.Rotator())
        component = _component(floor, ue.StaticMeshComponent)
        component.set_static_mesh(ue.load_asset("/Engine/BasicShapes/Cube"))
        component.set_collision_enabled(ue.CollisionEnabled.NO_COLLISION)
        floor.set_actor_scale3d(ue.Vector(8, 5, .08))

    def _spawn_lighting(self):
        light = self._spawn(ue.DirectionalLight, ue.Vector(0, 0, 450), ue.Rotator(pitch=-35, yaw=-35, roll=0))
        _component(light, ue.DirectionalLightComponent).set_editor_property("intensity", 3.0)
        sky = self._spawn(ue.SkyLight, ue.Vector(0, 0, 400), ue.Rotator())
        _component(sky, ue.SkyLightComponent).set_editor_property("intensity", 1.0)
        # The meshes face +Y natively. This low-intensity fill reveals facial
        # normals without changing or masking any material sidedness.
        fill = self._spawn(ue.PointLight, ue.Vector(0, 320, 175), ue.Rotator())
        fill_component = _component(fill, ue.PointLightComponent)
        fill_component.set_editor_property("intensity", 250.0)
        fill_component.set_editor_property("attenuation_radius", 700.0)

    def _prepare_pose(self, name, state, view):
        for other_name, other in self.characters.items():
            other["component"].set_visibility(other_name == name, True)
        row = self.characters[name]
        component = row["component"]
        animation = row["animations"][state]
        sequence_length = float(animation.get_editor_property("sequence_length"))
        desired_sample_time = min(.25, sequence_length * .5)
        assert desired_sample_time > 0.0, (name, state, sequence_length)
        # PlayAnimation creates the transient single-node instance. Let it tick
        # before seeking; seeking in the same call stack can report the requested
        # time while the component still exposes its reference pose.
        component.set_editor_property("pause_anims", False)
        component.play_animation(animation, False)
        self.pending_pose = (name, state, view, desired_sample_time)
        self.pose_wait_ticks = 2

    @staticmethod
    def _translation(transform):
        value = transform.translation
        return [float(value.x), float(value.y), float(value.z)]

    def _capture_prepared_pose(self, name, state, view, desired_sample_time):
        row = self.characters[name]
        component = row["component"]
        actual_sample_time = float(component.get_position())
        assert abs(actual_sample_time - desired_sample_time) <= .02, (
            name, state, desired_sample_time, actual_sample_time)
        probes = {
            bone: self._translation(component.get_socket_transform(bone))
            for bone in ("hand_L", "hand_R", "foot_L", "foot_R")
        }
        self.pose_transforms[(name, state)] = probes
        actor = row["actor"]
        position = actor.get_actor_location()
        half_height = row["bounds_cm"][2] * .5
        target = position + ue.Vector(0, 0, half_height)
        offset = ue.Vector(650, 0, 80) if view == "front" else ue.Vector(0, 650, 80)
        self.camera.set_actor_location(position + offset, False, False)
        self.camera.set_actor_rotation(ue.MathLibrary.find_look_at_rotation(position + offset, target), False)
        output = self.run_dir / f"{name}_{state}_{view}.png"
        assert ue.AutomationLibrary.take_high_res_screenshot(900, 900, str(output), camera=self.camera, delay=.15)
        return output, actual_sample_time

    @staticmethod
    def _distance(a, b):
        return sum((a[index] - b[index]) ** 2 for index in range(3)) ** .5

    def _validate_pose_distinction(self):
        for name in NAMES:
            idle = self.pose_transforms[(name, "Idle")]
            walk = self.pose_transforms[(name, "Walk")]
            hand_delta = max(self._distance(idle[bone], walk[bone]) for bone in ("hand_L", "hand_R"))
            foot_delta = max(self._distance(idle[bone], walk[bone]) for bone in ("foot_L", "foot_R"))
            assert hand_delta >= .5 and foot_delta >= .5, (
                name, "Idle/Walk pose probes did not differ", hand_delta, foot_delta)

    def tick(self, _delta_seconds):
        if self.done:
            return
        self.ticks += 1
        try:
            if self.ticks > self.max_ticks:
                self.abort("screenshot wait timed out")
                return
            if self.index >= len(self.queue):
                self._validate_pose_distinction()
                self.finish()
                return
            name, state, view = self.queue[self.index]
            output = self.run_dir / f"{name}_{state}_{view}.png"
            if not self.waiting_for_file:
                if self.pending_pose is None:
                    self._prepare_pose(name, state, view)
                    return
                if self.pose_wait_ticks > 0:
                    self.pose_wait_ticks -= 1
                    return
                pending_name, pending_state, pending_view, desired_time = self.pending_pose
                assert (pending_name, pending_state, pending_view) == (name, state, view)
                component = self.characters[name]["component"]
                if self.pose_wait_ticks == 0:
                    component.set_position(desired_time, False)
                    component.set_play_rate(0.0)
                    self.pose_wait_ticks = -2
                    return
                if self.pose_wait_ticks < -1:
                    self.pose_wait_ticks += 1
                    return
                _, sample_time = self._capture_prepared_pose(name, state, view, desired_time)
                self.pending_pose = None
                self.current_sample_time = sample_time
                self.samples[(name, state)] = sample_time
                self.waiting_for_file = True
                return
            if not output.exists() or output.stat().st_size <= 0:
                return
            self.waiting_for_file = False
            self.index += 1
        except Exception as exc:
            self.abort(str(exc))

    def finish(self, error=None):
        global _RUN
        if self.done:
            return
        self.done = True
        ue.unregister_slate_post_tick_callback(self.tick_handle)
        report = {"temporary_world": self.world.get_path_name(), "saved_map": False,
                  "status": "failed" if error else "passed", "error": error,
                  "run_id": self.run_id, "screenshots": [], "characters": {}}
        for name, row in self.characters.items():
            report["characters"][name] = {
                "mesh": row["mesh"].get_path_name(),
                "skeleton": row["skeleton"].get_path_name(),
                "bone_names": [str(n) for n in row["skeleton"].get_reference_pose().get_bone_names()],
                "bounds_cm": row["bounds_cm"],
                "bounds_origin_cm": [row["mesh"].get_bounds().origin.x, row["mesh"].get_bounds().origin.y, row["mesh"].get_bounds().origin.z],
                "bounds_box_extent_cm": [row["mesh"].get_bounds().box_extent.x, row["mesh"].get_bounds().box_extent.y, row["mesh"].get_bounds().box_extent.z],
                "actor_yaw_degrees": row["actor"].get_actor_rotation().yaw,
                "material_slots": row["material_slots"],
                "animations": {state: row["animations"][state].get_path_name() for state in STATES},
                "pose_transforms_world_cm": {
                    state: self.pose_transforms.get((name, state)) for state in STATES
                },
            }
        for name, state, view in self.queue:
            report["screenshots"].append({
                "name": name, "state": state, "view": view,
                "path": str(self.run_dir / f"{name}_{state}_{view}.png"),
                "camera_offset_cm": [650, 0, 80] if view == "front" else [0, 650, 80],
                "sample_time_s": self.samples.get((name, state)),
                "orientation_assumption": "front_candidate_axis_x" if view == "front" else "side_candidate_axis_y",
            })
        report_path = self.run_dir / "UE_Preview_Report.json"
        report["report_path"] = str(report_path)
        report_path.write_text(json.dumps(report, indent=2), encoding="utf-8")
        for actor in self.spawned:
            if actor and not actor.is_actor_being_destroyed():
                self.actors.destroy_actor(actor)
        _RUN = None
        ue.log("AINCRAD_CHARACTERS_V2_PREVIEW_" + ("FAILED " + str(error) if error else "COMPLETE") + " " + str(report_path))
        if AUTO_QUIT:
            ue.EditorPythonScripting.set_keep_python_script_alive(False)
            ue.SystemLibrary.quit_editor()

    def abort(self, reason):
        self.finish(error=reason)


def start():
    """Start the tick-driven transient preview; returns immediately."""
    global _RUN
    assert _RUN is None, "A V2 preview is already running"
    return PreviewRun()


if AUTO_QUIT:
    ue.EditorPythonScripting.set_keep_python_script_alive(True)
    start()
