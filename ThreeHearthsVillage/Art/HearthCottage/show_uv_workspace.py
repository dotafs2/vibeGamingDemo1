"""Arrange the user-facing Blender window after its UI has initialized."""
import bpy
import json
import traceback
from pathlib import Path
from mathutils import Vector

bpy.context.preferences.view.show_splash = False
out = Path(__file__).resolve().parent
loaded_path = Path(bpy.data.filepath)
switched = False

def arrange():
    global switched
    # Workspace activation is deferred until the next UI event-loop iteration.
    if not switched:
        for window in bpy.context.window_manager.windows:
            window.workspace=bpy.data.workspaces['UV Editing']
        switched=True
        return .5
    for window in bpy.context.window_manager.windows:
        workspace = bpy.data.workspaces.get('UV Editing')
        if not workspace:
            continue
        with bpy.context.temp_override(window=window):
            for area in window.screen.areas:
                if area.type == 'IMAGE_EDITOR':
                    area.ui_type = 'UV'
                    area.spaces.active.image = bpy.data.images.get('UV inspection grid | generated, not a model texture')
                    area.spaces.active.show_region_ui = False
                    area.spaces.active.show_region_toolbar = False
                    region=next(r for r in area.regions if r.type=='WINDOW')
                    with bpy.context.temp_override(area=area,region=region):
                        bpy.ops.image.view_all(fit_view=False)
                        bpy.ops.image.view_zoom_ratio(ratio=.625)
                elif area.type == 'VIEW_3D':
                    space=area.spaces.active
                    space.shading.type='SOLID'
                    space.shading.color_type='MATERIAL'
                    space.overlay.show_floor=False
                    space.overlay.show_axis_x=False
                    space.overlay.show_axis_y=False
                    space.overlay.show_faces=False
                    space.overlay.show_relationship_lines=False
                    space.region_3d.view_location=Vector((0,-.10,1.55))
                    space.region_3d.view_rotation=bpy.context.scene.camera.rotation_euler.to_quaternion()
                    space.region_3d.view_distance=6.2
                    space.region_3d.view_perspective='ORTHO'
                area.tag_redraw()
            bpy.ops.wm.save_as_mainfile(filepath=str(loaded_path))
            (out/'uv-view-status.json').write_text(json.dumps({'workspace':window.workspace.name,
                'areas':[a.type for a in window.screen.areas]},indent=2),encoding='utf-8')
    return None

def safe_arrange():
    try:
        return arrange()
    except Exception:
        (out/'uv-view-status.json').write_text(json.dumps({'error':traceback.format_exc()},indent=2),encoding='utf-8')
        return None

bpy.app.timers.register(safe_arrange, first_interval=1.2)
