"""Owned real-world neighborhood probe and separate-process reload verification."""
import json
from pathlib import Path
import time
import unreal

unreal.EditorPythonScripting.set_keep_python_script_alive(True)
reload_mode='HearthTownProbeReload' in unreal.SystemLibrary.get_command_line()
out=Path(r'C:\vibeGamingDemo1\.codex-ue58-diagnostics\town-v2')/('town-reload' if reload_mode else 'town-runtime')
out.mkdir(parents=True,exist_ok=True)
started=time.monotonic()
phase='waiting'
handle=None
closing_at=0

def write(name,data):
    (out/name).write_text(json.dumps(data,ensure_ascii=False,indent=2),encoding='utf-8')

def finish(data):
    global phase,closing_at
    data['real_seconds']=time.monotonic()-started
    write('result.json',data)
    phase='closing'
    closing_at=time.monotonic()
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()

def tick(delta):
    global phase,handle
    try:
        if phase=='closing':
            if time.monotonic()-closing_at>4:
                unreal.unregister_slate_post_tick_callback(handle)
                handle=None
                unreal.EditorPythonScripting.set_keep_python_script_alive(False)
                unreal.SystemLibrary.quit_editor()
            return
        elapsed=time.monotonic()-started
        if elapsed>170:
            finish({'error':'probe exceeded 170 seconds'})
            return
        if phase=='waiting' and elapsed>8:
            unreal.EditorLevelLibrary.editor_play_simulate()
            phase='playing'
        if phase!='playing':return
        world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        if not world:return
        actors=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.HearthVillage)
        if not actors:return
        village=actors[0]
        if elapsed>(23 if reload_mode else 85):
            village.set_simulation_speed(1)
            state=json.loads(village.export_world_state())
            write('final-world.json',state)
            plans=state.get('structure_plans',[])
            components=[c for s in state['sites'] for c in s['cottage_components']]
            result={'error':None,'api_requests':int(village.get_editor_property('api_requests')),
                    'world_id':state['Id'],'layout_version':state.get('town_layout_version'),
                    'organic_roads':state.get('organic_town_layout'),'residents':len(state['people']),
                    'homes_complete':sum(p['BuildProgress']>=1 for p in state['people']),
                    'plans':len(plans),'room_counts':[len(p['rooms']) for p in plans],
                    'parts':len(components),'parts_complete':sum(c['status']=='completed' for c in components),
                    'town_png':village.export_town_observation(),'home_png':village.export_design_observation(0),
                    'plots':state['plots'],'goals_injected':False}
            if reload_mode:
                before=json.loads((out.parent/'town-runtime/final-world.json').read_text(encoding='utf-8'))
                result['same_world']=before['Id']==state['Id']
                result['same_plots']=before['plots']==state['plots']
                result['same_layout']=before.get('town_layout_version')==state.get('town_layout_version') and before.get('organic_town_layout')==state.get('organic_town_layout')
                result['same_plan_geometry']=[(p['plan_id'],p['footprint'],p['components']) for p in before['structure_plans']]==[(p['plan_id'],p['footprint'],p['components']) for p in plans]
            else:
                village.toggle_pause()
                result['save_success']=bool(village.save_world())
            finish(result)
    except Exception as exc:
        finish({'error':str(exc)})

handle=unreal.register_slate_post_tick_callback(tick)
