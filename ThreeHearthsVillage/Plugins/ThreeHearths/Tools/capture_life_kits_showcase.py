"""Local client for the one explicitly started art editor's Python remote endpoint."""
import argparse
import json
from pathlib import Path
import sys
import time
ROOT=Path(__file__).resolve().parents[3]
sys.path.insert(0,'D:/UE_5.8/Engine/Plugins/Experimental/PythonScriptPlugin/Content/Python')
from remote_execution import RemoteExecution,RemoteExecutionConfig

def main():
    parser=argparse.ArgumentParser();parser.add_argument('kit',choices=['ResidentKit','HomeLifeKit'])
    parser.add_argument('view',nargs='?',default='overview',choices=['overview','cabin','meal']);parser.add_argument('--quit',action='store_true')
    parser.add_argument('--audit',action='store_true')
    args=parser.parse_args();config=RemoteExecutionConfig();config.command_endpoint=('127.0.0.1',6784)
    remote=RemoteExecution(config)
    try:
        remote.start();until=time.monotonic()+10;nodes=[]
        while time.monotonic()<until:
            nodes=[n for n in remote.remote_nodes if n.get('project_name')=='CropoutSampleProject']
            if nodes:break
            time.sleep(.2)
        assert len(nodes)==1,('Expected only the dedicated art editor',nodes)
        remote.open_command_connection(nodes[0]['node_id'])
        if args.quit:code="import unreal as ue; ue.EditorLevelLibrary.save_current_level(); ue.SystemLibrary.quit_editor()"
        elif args.audit:
            path=str(ROOT/'Plugins/ThreeHearths/Tools/create_life_kits_showcase.py')
            code='import runpy\nart_showcase_tools=runpy.run_path('+repr(path)+')\nprint(art_showcase_tools["audit_imports"]())'
        else:
            path=str(ROOT/'Plugins/ThreeHearths/Tools/create_life_kits_showcase.py')
            code='import runpy\nart_showcase_tools=runpy.run_path('+repr(path)+')\nart_screenshot_task=art_showcase_tools["request_capture"]('+repr(args.kit)+','+repr(args.view)+')'
        result=remote.run_command(code);print(json.dumps(result,ensure_ascii=False));assert result.get('success'),result
    finally:remote.stop()

if __name__=='__main__':main()
