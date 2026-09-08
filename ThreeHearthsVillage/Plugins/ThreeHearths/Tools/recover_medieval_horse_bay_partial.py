"""Recover only the known NativeImport2 bay idle assets into the horse report.

This is a bounded recovery helper for the one interrupted import pass. It
does not import, delete, move, or resave any asset. It enumerates only direct
assets in ``.../Rigged/horse_bay`` (never ``Walk`` or any other project path),
checks that the three SkeletalMesh layers and the Scene AnimSequence retain
the bay-idle FBX in their import data, and writes a partial report. The normal
horse importer can then resume and import only Walk.

Run inside the existing headless UE 5.8 commandlet after NativeImport2 has
saved the bay idle assets, for example:

  UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript \
    -script=Plugins/ThreeHearths/Tools/recover_medieval_horse_bay_partial.py \
    -HearthDisableApi -HearthNoWorldPersistence -unattended
"""
from __future__ import annotations

import importlib.util
import json
from pathlib import Path

import unreal as ue


HERE = Path(__file__).resolve().parent
IMPORTER_PATH = HERE / "import_medieval_horses_rigged.py"
_spec = importlib.util.spec_from_file_location("medieval_horse_importer", IMPORTER_PATH)
if _spec is None or _spec.loader is None:
    raise RuntimeError("Cannot load horse importer helpers: " + str(IMPORTER_PATH))
_importer = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_importer)

HORSE_ID = "horse_bay"
BAY_DEST = _importer.DEST + "/horse_bay"
IDLE_SOURCE = _importer.HORSES["bay"]["idle"]
REPORT = _importer.REPORT
LAYERS = set(_importer.LAYERS)


def _direct_bay_assets() -> list[object]:
    """Load only direct assets in the known bay folder, excluding Walk."""
    paths = ue.EditorAssetLibrary.list_assets(BAY_DEST, False, False)
    direct = []
    prefix = BAY_DEST + "/"
    for path in paths:
        path = str(path)
        if not path.startswith(prefix) or "/" in path[len(prefix):]:
            continue
        obj = ue.load_asset(path)
        if obj is not None:
            direct.append(obj)
    if not direct:
        raise RuntimeError("No direct NativeImport2 assets found in bounded bay folder: " + BAY_DEST)
    return direct


def _source_filename(asset: object) -> str:
    try:
        import_data = asset.get_editor_property("asset_import_data")
    except Exception as exc:
        raise RuntimeError("Asset has no asset_import_data: " + asset.get_path_name()) from exc
    if import_data is None:
        raise RuntimeError("Asset import data is empty: " + asset.get_path_name())
    for method_name in ("get_first_filename", "script_get_first_filename",
                        "k2_get_first_filename"):
        method = getattr(import_data, method_name, None)
        if method is None:
            continue
        try:
            filename = str(method())
        except Exception:
            continue
        if filename:
            return filename
    raise RuntimeError("Asset import data has no source filename: " + asset.get_path_name())


def _same_file(actual: str, expected: Path) -> bool:
    try:
        return Path(actual).resolve().as_posix().casefold() == expected.resolve().as_posix().casefold()
    except OSError:
        return str(actual).replace("\\", "/").casefold() == expected.as_posix().casefold()


def _assert_idle_source(asset: object) -> str:
    filename = _source_filename(asset)
    if not _same_file(filename, IDLE_SOURCE):
        raise RuntimeError(f"Bay asset source is not the idle FBX: {asset.get_path_name()} -> {filename}")
    return filename


def _existing_failed_report() -> dict:
    if not REPORT.is_file():
        raise RuntimeError("Expected interrupted import report: " + str(REPORT))
    report = json.loads(REPORT.read_text(encoding="utf-8"))
    if report.get("status") != "failed" or report.get("horses"):
        raise RuntimeError("Recovery is bounded to the empty-horses failed NativeImport2 report")
    return report


def main() -> None:
    previous_report = _existing_failed_report()
    source_info = _importer._validate_sources()
    assets = _direct_bay_assets()
    meshes = [asset for asset in assets if isinstance(asset, ue.SkeletalMesh)]
    skeletons = [asset for asset in assets if isinstance(asset, ue.Skeleton)]
    animations = [asset for asset in assets if isinstance(asset, ue.AnimSequence)]
    if len(meshes) != len(_importer.LAYERS):
        raise RuntimeError("Expected exactly three direct bay SkeletalMeshes; got " +
                           repr([mesh.get_path_name() for mesh in meshes]))
    if len(skeletons) != 1:
        raise RuntimeError("Expected exactly one direct bay Skeleton; got " +
                           repr([asset.get_path_name() for asset in skeletons]))
    if len(animations) != 1:
        raise RuntimeError("Expected exactly one direct bay Idle AnimSequence (Scene); got " +
                           repr([asset.get_path_name() for asset in animations]))
    skeleton_path = skeletons[0].get_path_name()
    if "/Walk/" in skeleton_path:
        raise RuntimeError("Recovery selected a Walk asset unexpectedly: " + skeleton_path)
    mesh_records = []
    seen_layers = set()
    source_assets = []
    for mesh in meshes:
        source_assets.append(mesh)
        _assert_idle_source(mesh)
        record = _importer._mesh_record(mesh, skeleton_path, HORSE_ID)
        if record["layer"] in seen_layers:
            raise RuntimeError("Duplicate bay layer: " + record["layer"])
        seen_layers.add(record["layer"])
        mesh_records.append(record)
    if seen_layers != LAYERS:
        raise RuntimeError("Bay idle layer set mismatch: " + repr(sorted(seen_layers)))
    animation = animations[0]
    source_assets.append(animation)
    _assert_idle_source(animation)
    idle_record = _importer._anim_record(animation, skeleton_path, "Idle")
    for asset in source_assets:
        if "/Walk/" in asset.get_path_name():
            raise RuntimeError("Recovery scope escaped bay idle folder: " + asset.get_path_name())
    hashes = source_info["hashes"]["bay"]
    row = {
        "id": HORSE_ID,
        "source_sha256": hashes,
        "source_files": {clip: _importer.HORSES["bay"][clip].relative_to(_importer.ROOT).as_posix()
                         for clip in ("idle", "walk")},
        "skeleton": skeleton_path,
        "meshes": sorted(mesh_records, key=lambda item: item["layer"]),
        "animations": {"Idle": idle_record},
        "source_bones_expected": 18,
        "authoring_units": "metres",
        "ue_units": "centimetres",
        "source_to_unreal_matrix": _importer.SOURCE_TO_UNREAL_MATRIX,
        "import_seconds": 0.0,
        "reused": False,
        "partial_recovery": "NativeImport2 bay idle assets only; resume importer for Walk",
    }
    partial = {
        "schema_version": previous_report.get("schema_version", 1),
        "status": "partial",
        "destination_root": _importer.DEST,
        "expected_horses": 2,
        "expected_skeletal_mesh_layers": 6,
        "horses": [row],
        "partial_recovery": {
            "scope": BAY_DEST,
            "source": str(IDLE_SOURCE),
            "assets_checked": [asset.get_path_name() for asset in source_assets],
            "walk_checked": False,
            "deletes": 0,
            "imports": 0,
        },
    }
    REPORT.write_text(json.dumps(partial, ensure_ascii=False, indent=2) + "\n",
                      encoding="utf-8")
    ue.log("[MedievalLifeRiggedHorseRecovery] wrote bounded bay partial report: " +
           json.dumps({"meshes": len(mesh_records), "idle": idle_record["asset"],
                       "report": str(REPORT)}))


if __name__ == "__main__":
    main()
