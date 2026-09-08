"""Rebuild only the new Level0 generator's owned preview and save that map."""
import unreal as ue
sub=ue.get_editor_subsystem(ue.LevelEditorSubsystem)
assert sub.load_level('/Game/ThreeHearths/Maps/L_AincradLevel0')
actors=ue.get_editor_subsystem(ue.EditorActorSubsystem).get_all_level_actors()
worlds=[a for a in actors if a.get_class().get_name()=='HearthAincradLevel']
assert len(worlds)==1
worlds[0].build_preview()
eye=ue.Vector(0,470100,210)
aim=ue.Vector(0,465000,280)
ue.EditorLevelLibrary.set_level_viewport_camera_info(eye,ue.MathLibrary.find_look_at_rotation(eye,aim))
assert sub.save_current_level()
ue.log('LEVEL0_EDITOR_PREVIEW_SAVED')
