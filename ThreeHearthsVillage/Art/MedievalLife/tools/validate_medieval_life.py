"""Offline validation for MedievalLife module/layer source contracts.

This does not launch Blender or Unreal.  It checks the authored catalog, GLB
layer nodes, anchor metadata, semantic material roles, and the explicit
unrigged horse/human limitation before a UE import is attempted.
"""
from __future__ import annotations

import argparse
import json
import struct
from collections import Counter
from pathlib import Path


ART = Path(__file__).resolve().parents[1]
CATALOG = ART / "catalog.json"
LAYERS = {"structure", "finish", "attachments", "weathering"}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--report", type=Path, default=None,
                        help="optional JSON report path")
    return parser.parse_args()


def glb_json(path: Path) -> dict:
    data = path.read_bytes()
    if data[:4] != b"glTF":
        raise RuntimeError("Not a GLB: " + str(path))
    offset = 12
    while offset < len(data):
        size, kind = struct.unpack_from("<I4s", data, offset)
        payload = data[offset + 8:offset + 8 + size]
        offset += 8 + size
        if kind == b"JSON":
            return json.loads(payload.rstrip(b" "))
    raise RuntimeError("GLB has no JSON chunk: " + str(path))


def validate() -> dict:
    catalog = json.loads(CATALOG.read_text(encoding="utf-8"))
    modules = catalog.get("modules", [])
    if len(modules) != 21:
        raise RuntimeError(f"Expected 21 modules, found {len(modules)}")
    ids = [row["id"] for row in modules]
    if len(set(ids)) != len(ids):
        raise RuntimeError("Duplicate MedievalLife module ID")
    layer_counts = Counter()
    checked_layers = 0
    banner_layers = []
    static_character_modules = []
    for row in modules:
        module_id = row["id"]
        layers = set(row.get("layers", []))
        if not layers or not layers <= LAYERS:
            raise RuntimeError(f"Invalid layer set for {module_id}: {sorted(layers)}")
        path = ART / row["path"]
        if not path.is_file():
            raise RuntimeError("Missing module GLB: " + str(path))
        doc = glb_json(path)
        layer_nodes = [node for node in doc.get("nodes", [])
                       if isinstance(node.get("extras"), dict)
                       and node["extras"].get("layer") in LAYERS]
        actual = {node["extras"]["layer"] for node in layer_nodes}
        if actual != layers:
            raise RuntimeError(f"GLB layer mismatch for {module_id}: "
                               f"catalog={sorted(layers)} glb={sorted(actual)}")
        if any(node.get("mesh") is None for node in layer_nodes):
            raise RuntimeError("Layer node missing mesh for " + module_id)
        for node in layer_nodes:
            layer = node["extras"]["layer"]
            roles = node["extras"].get("material_roles", [])
            if not roles:
                raise RuntimeError(f"Layer has no semantic material roles: {module_id}/{layer}")
            layer_counts[layer] += 1
            checked_layers += 1
        if module_id == "royal_banner":
            if "blue" not in {role for node in layer_nodes
                               for role in node["extras"].get("material_roles", [])}:
                raise RuntimeError("Royal banner has no blue semantic cloth role")
            banner_layers = sorted(actual)
        if module_id in {"horse_bay", "horse_grey", "royal_guard_body", "carter_body"}:
            note = row.get("notes", "").lower()
            if "unrigged" not in note and "no skeleton" not in note:
                raise RuntimeError("Missing unrigged limitation note for " + module_id)
            static_character_modules.append(module_id)
        for name, anchor in row.get("anchors", {}).items():
            if not isinstance(anchor, list) or len(anchor) != 3:
                raise RuntimeError(f"Invalid anchor {module_id}:{name}")
    limitations = [str(value).lower() for value in catalog.get("limitations", [])]
    if not any("animation" in value for value in limitations):
        raise RuntimeError("Catalog limitations do not declare missing animation")
    if not any("unrigged" in value or "no armature" in value for value in limitations):
        raise RuntimeError("Catalog limitations do not declare unrigged art")
    result = {
        "status": "passed",
        "catalog": "Art/MedievalLife/catalog.json",
        "module_count": len(modules),
        "layer_asset_count": checked_layers,
        "layer_counts": dict(sorted(layer_counts.items())),
        "royal_banner_layers": banner_layers,
        "static_unrigged_character_modules": sorted(static_character_modules),
        "authoring": {"up": "+Z", "front": "-Y", "units": "meters"},
        "ue_conversion": {
            "matrix": [[100.0, 0.0, 0.0, 0.0],
                       [0.0, -100.0, 0.0, 0.0],
                       [0.0, 0.0, 100.0, 0.0],
                       [0.0, 0.0, 0.0, 1.0]],
            "yaw": "yaw_ue = -yaw_blender",
        },
    }
    return result


def main() -> None:
    result = validate()
    args = parse_args()
    if args.report:
        path = args.report if args.report.is_absolute() else ART / args.report
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n",
                        encoding="utf-8")
    print(json.dumps(result, ensure_ascii=False, sort_keys=True))


if __name__ == "__main__":
    main()
