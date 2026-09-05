"""Compatibility entry point for the corrected shared roof UV workflow.

Run in Blender with HearthCottage.blend loaded. Writes HearthCottage_SharedUV,
preserving earlier files and existing unsaved Blender windows.
"""
from pathlib import Path

repair = Path(__file__).resolve().with_name('repair_shared_uv.py')
exec(compile(repair.read_text(encoding='utf-8'), str(repair), 'exec'),
     {'__name__':'__main__', '__file__':str(repair)})
