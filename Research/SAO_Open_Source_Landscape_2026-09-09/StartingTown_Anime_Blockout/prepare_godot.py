"""Download a pinned portable editor from the official Godot download redirect.
No machine-wide install, PATH change, or project dependency execution.
"""
from pathlib import Path
import hashlib,json,time,urllib.request,zipfile

ROOT=Path('C:/Users/quchenxi/.cache/level0-tools/godot-4.7.2-mono')
ROOT.mkdir(parents=True,exist_ok=True)
archive=ROOT/'Godot_v4.7.2-stable_mono_win64.zip'
url='https://downloads.godotengine.org/?flavor=stable&platform=windows.64&slug=mono_win64.zip&version=4.7.2'
if not archive.exists():
    req=urllib.request.Request(url,headers={'User-Agent':'Level0-local-prototype/1.0'})
    start=time.monotonic(); total=0
    with urllib.request.urlopen(req,timeout=25) as response, archive.with_suffix('.partial').open('wb') as out:
        print('source='+response.geturl(),flush=True)
        while chunk:=response.read(256*1024):
            total+=len(chunk)
            if total>250*1024*1024 or time.monotonic()-start>150: raise RuntimeError('Bounded download exceeded limit')
            out.write(chunk)
    archive.with_suffix('.partial').replace(archive)
with zipfile.ZipFile(archive) as z:
    if sum(i.file_size for i in z.infolist())>900*1024*1024: raise RuntimeError('Expanded archive too large')
    for info in z.infolist():
        target=(ROOT/info.filename).resolve()
        if not target.is_relative_to(ROOT.resolve()): raise RuntimeError('Unsafe zip member')
    z.extractall(ROOT)
executables=sorted(str(p) for p in ROOT.glob('*/*.exe'))+sorted(str(p) for p in ROOT.glob('*.exe'))
print(json.dumps({'zip_sha256':hashlib.sha256(archive.read_bytes()).hexdigest(),'executables':executables},indent=2),flush=True)
