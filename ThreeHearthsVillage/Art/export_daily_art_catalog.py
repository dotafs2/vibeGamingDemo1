"""Export the 2026-09-07 art recipes from the current compiled UE plugin."""
from pathlib import Path
import json
import unreal as ue

world = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()
ue.SystemLibrary.execute_console_command(world, "Hearth.ExportArtCatalog")
catalog = Path(ue.Paths.project_saved_dir()) / "ThreeHearths/ArtCatalog/catalog.json"
data = json.loads(catalog.read_text(encoding="utf-8"))
assert data.get("schema") == 1 and len(data.get("entries", [])) == 12
ue.log("DAILY_ART_EXPORTED " + str([(entry["id"], len(entry["parts"])) for entry in data["entries"]]))
