"""Local offline UE art client; discovers exactly one project editor."""
from pathlib import Path
import argparse,json,sys,time
ROOT=Path(__file__).resolve().parents[3]
sys.path.insert(0,'D:/UE_5.8/Engine/Plugins/Experimental/PythonScriptPlugin/Content/Python')
from remote_execution import RemoteExecution,RemoteExecutionConfig

def main():
    parser=argparse.ArgumentParser();parser.add_argument('action',choices=['build','capture','reference','quit'])
    parser.add_argument('state',nargs='?',default='polished',choices=['original','polished'])
    parser.add_argument('view',nargs='?',default='house',choices=['house','roof']);args=parser.parse_args()
    config=RemoteExecutionConfig();config.command_endpoint=('127.0.0.1',6784);remote=RemoteExecution(config)
    try:
        remote.start();until=time.monotonic()+10;nodes=[]
        while time.monotonic()<until:
            nodes=[n for n in remote.remote_nodes if n.get('project_name')=='CropoutSampleProject']
            if nodes:break
            time.sleep(.2)
        assert len(nodes)==1,('Expected only the dedicated editor',nodes)
        remote.open_command_connection(nodes[0]['node_id'])
        code='import runpy\npolished_tools=runpy.run_path('+repr(str(ROOT/'Plugins/ThreeHearths/Tools/import_polished_cottage.py'))+')\n'
        if args.action=='build':code+='polished_tools["build"]()'
        elif args.action=='reference':code+='print(polished_tools["export_reference"]()); print(polished_tools["export_materials"]())'
        elif args.action=='quit':code+='import unreal as ue\nue.EditorLevelLibrary.save_current_level()\nue.SystemLibrary.quit_editor()'
        else:code+='polished_task=polished_tools["capture"]('+repr(args.state)+','+repr(args.view)+')'
        result=remote.run_command(code);print(json.dumps(result,ensure_ascii=False));assert result.get('success'),result
    finally:remote.stop()

if __name__=='__main__':main()
