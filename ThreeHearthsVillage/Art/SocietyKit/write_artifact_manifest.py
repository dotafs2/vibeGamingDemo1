"""Record immutable import inputs for the current SocietyKit export."""
from pathlib import Path
import hashlib
import json

OUT=Path(__file__).resolve().parent
paths=sorted(list((OUT/'modules').glob('*.glb'))+list((OUT/'examples').glob('*.glb')))
report={'schema_version':1,'kit_id':'society_kit_01','scope':'current exported files',
    'byte_identical_regeneration_guaranteed':False,
    'artifacts':[{'file':p.relative_to(OUT).as_posix(),'bytes':p.stat().st_size,
                  'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for p in paths]}
(OUT/'artifact-manifest.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
print('Frozen GLB artifacts:',len(paths))
