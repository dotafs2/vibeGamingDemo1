"""Create the independent SAO first-floor native map; no legacy save access."""
import unreal

PATH='/Game/ThreeHearths/Maps/L_AincradLevel0'
CLASS='/Script/ThreeHearths.HearthAincradLevel'
MODE='/Script/ThreeHearths.HearthAincradGameMode'
library=unreal.EditorLevelLibrary
subsystem=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not unreal.EditorAssetLibrary.does_asset_exist(PATH), 'Map already exists; do not silently replace authored edits.'
assert subsystem.new_level(PATH), 'New map failed'
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
world.get_world_settings().set_editor_property('default_game_mode',unreal.load_class(None,MODE))
level=library.spawn_actor_from_class(unreal.load_class(None,CLASS),unreal.Vector(0,0,0))
level.set_actor_label('Level0 — Aincrad First Floor')
# Native geometry is reconstructed deterministically at runtime. Editor map
# keeps only the world generator, the atmosphere and a player start.
sky=library.spawn_actor_from_class(unreal.SkyAtmosphere,unreal.Vector(0,0,0))
sky.set_actor_label('Aincrad Daylight Sky')
start=library.spawn_actor_from_class(unreal.PlayerStart,unreal.Vector(61000,-523000,62000),unreal.Rotator(-40,-40,0))
start.set_actor_label('Start Above Town of Beginnings')
assert subsystem.save_current_level(), 'Map save failed'
unreal.log('LEVEL0_MAP_SAVED '+PATH)
