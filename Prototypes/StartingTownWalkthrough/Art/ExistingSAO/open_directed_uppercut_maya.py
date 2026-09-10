"""Load the demo into Maya, retaining unsaved work and restoring playback UI."""
from pathlib import Path
import maya.cmds as c
import runpy,json,traceback
from datetime import datetime
base=Path('C:/vibeGamingDemo1/Prototypes/StartingTownWalkthrough/Art/ExistingSAO')
out=base/'Maya'
try:
    c.play(state=False)
    if c.file(q=True,modified=True):
        c.file(rename=str(out/('Before_Directed_Demo_'+datetime.now().strftime('%H%M%S')+'.ma')))
        c.file(save=True,type='mayaAscii',force=True)
    c.file(str(out/'Kirito_Directed_Uppercut_Demo.ma'),open=True,force=True)
    for panel in c.getPanel(type='modelPanel') or []:
        c.modelEditor(panel,e=True,displayTextures=True,displayLights='all',displayAppearance='smoothShaded',twoSidedLighting=True,shadows=True,grid=False,selectionHiliteDisplay=False,joints=False,lights=False,cameras=False)
    d=runpy.run_path(str(base/'direct_uppercut_demo_maya.py'))
    d['show_ui']()
    c.currentTime(55)
    c.file(save=True,type='mayaAscii',force=True)
    d['play']()
    (out/'directed_demo_loaded.json').write_text(json.dumps({'loaded':True,'scene':c.file(q=True,sn=True)}))
except Exception:
    (out/'directed_demo_load_error.txt').write_text(traceback.format_exc(),encoding='utf-8')
