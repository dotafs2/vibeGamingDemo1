"""Offline conversion of host-board art requests into reviewable art briefs.

This module deliberately has no network, Blender, Unreal, or model-provider dependency.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
import re
import tempfile
from pathlib import Path
from typing import Any

MAX_BYTES = 2 * 1024 * 1024
MAX_REQUESTS = 64
MAX_TEXT = 512
MAX_ID = 128
ID_RE = re.compile(r"^[A-Za-z0-9_][A-Za-z0-9_.:-]{0,127}$")
ALLOWED_CATEGORIES = {"narrative", "existing_action", "asset", "mechanic", "clarification"}
ALLOWED_STATUSES = {"proposed", "resolved_existing", "needs_details", "deferred"}
OUTPUT_JSON = "hearth_art_jobs.json"
OUTPUT_MD = "hearth_art_jobs.md"
ASSET_CONTEXT_KEYS = {
    "purpose", "target_id", "resident_contexts", "target_position_cm", "observation_id",
}
RESIDENT_CONTEXT_KEYS = {"resident_id", "name", "personality", "inner_story", "design_goal"}


class InputError(ValueError):
    """Raised for an invalid, unsafe, or unsupported board/catalog document."""


def _text(value: Any, field: str, *, required: bool = True) -> str:
    if not isinstance(value, str) or (required and not value.strip()):
        raise InputError(f"{field} must be a non-empty string")
    if len(value) > MAX_TEXT:
        raise InputError(f"{field} is too long")
    if any(ord(c) < 32 and not c.isspace() for c in value) or any(ord(c) == 127 for c in value):
        raise InputError(f"{field} contains disallowed control characters")
    return " ".join(value.split())


def _id(value: Any, field: str) -> str:
    value = _text(value, field)
    if len(value) > MAX_ID or not ID_RE.fullmatch(value):
        raise InputError(f"{field} is not a safe identifier")
    return value


def _optional_id(value: Any, field: str) -> str:
    if value == "":
        return ""
    return _id(value, field)


def _position(value: Any, field: str) -> list[int | float] | None:
    if value is None:
        return None
    if not isinstance(value, list) or len(value) != 3:
        raise InputError(f"{field} must be null or a three-number array")
    if any(isinstance(item, bool) or not isinstance(item, (int, float)) or not math.isfinite(item) for item in value):
        raise InputError(f"{field} must contain only finite numbers")
    return list(value)


def _asset_context(value: Any, request_id: str, requester_ids: list[str], summary: str) -> dict[str, Any]:
    if not isinstance(value, dict) or not ASSET_CONTEXT_KEYS.issubset(value):
        raise InputError(f"asset_context on {request_id} is missing required keys")
    target_id = _optional_id(value.get("target_id"), f"request {request_id} asset_context target_id")
    residents = value.get("resident_contexts")
    if not isinstance(residents, list) or not 1 <= len(residents) <= 32:
        raise InputError(f"resident_contexts on {request_id} must contain 1..32 residents")
    normalized_residents = []
    for index, resident in enumerate(residents):
        if not isinstance(resident, dict) or not RESIDENT_CONTEXT_KEYS.issubset(resident):
            raise InputError(f"resident_contexts entry {index} on {request_id} is missing required keys")
        resident_id = _id(resident.get("resident_id"), f"request {request_id} resident {index} resident_id")
        if resident_id not in requester_ids:
            raise InputError(f"resident_id on {request_id} must be listed in requester_ids")
        if any(item["resident_id"] == resident_id for item in normalized_residents):
            raise InputError(f"resident_id on {request_id} must be unique")
        normalized_residents.append({
            "resident_id": resident_id,
            "name": _text(resident.get("name"), f"request {request_id} resident {index} name", required=False),
            "personality": _text(resident.get("personality"), f"request {request_id} resident {index} personality", required=False),
            "inner_story": _text(resident.get("inner_story"), f"request {request_id} resident {index} inner_story", required=False),
            "design_goal": _text(resident.get("design_goal"), f"request {request_id} resident {index} design_goal", required=False),
        })
    if len(set(requester_ids)) != len(requester_ids) or {r["resident_id"] for r in normalized_residents} != set(requester_ids):
        raise InputError(f"resident contexts on {request_id} must match its unique requester IDs")
    return {
        "purpose": _context_purpose(value.get("purpose"), request_id, summary),
        "target_id": target_id,
        "resident_contexts": normalized_residents,
        "target_position_cm": _position(value.get("target_position_cm"), f"request {request_id} target_position_cm"),
        "observation_id": _text(value.get("observation_id"), f"request {request_id} observation_id", required=False),
    }


def _context_purpose(value: Any, request_id: str, summary: str) -> str:
    purpose = _text(value, f"request {request_id} asset_context purpose")
    if purpose.lower() != summary.lower():
        raise InputError(f"asset_context purpose on {request_id} must match the normalized request summary")
    return purpose


def _digest(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(65536), b""):
            digest.update(block)
    return digest.hexdigest()


def _load_json(path: Path, label: str) -> Any:
    if not path.is_file() or path.stat().st_size > MAX_BYTES:
        raise InputError(f"{label} is missing or exceeds {MAX_BYTES} bytes")
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise InputError(f"invalid {label}: {exc}") from exc


def _catalog_entries(catalog: Any, catalog_path: Path) -> tuple[list[dict[str, str]], dict[str, str]]:
    if not isinstance(catalog, dict) or catalog.get("schema_version") != 2:
        raise InputError("catalog schema_version must be 2")
    raw = catalog.get("components")
    if not isinstance(raw, list) or not raw or len(raw) > 1000:
        raise InputError("catalog must contain a bounded components/assets list")
    entries: list[dict[str, str]] = []
    for index, item in enumerate(raw):
        if not isinstance(item, dict):
            raise InputError(f"catalog entry {index} must be an object")
        asset = item.get("component_asset_id")
        glb = item.get("source_glb")
        if asset is None or glb is None:
            continue
        asset_id = _id(asset, f"catalog entry {index} asset id")
        glb = _text(glb, f"catalog entry {index} glb")
        glb_path = Path(glb)
        catalog_root = catalog_path.resolve().parent.parent
        if glb_path.is_absolute() or not glb.lower().endswith(".glb"):
            raise InputError(f"catalog entry {index} has unsafe GLB path")
        try:
            (catalog_path.resolve().parent / glb_path).resolve().relative_to(catalog_root)
        except ValueError as exc:
            raise InputError(f"catalog entry {index} escapes catalog root") from exc
        entries.append({"asset_id": asset_id, "asset_glb": glb.replace("\\", "/")})
    if not entries:
        raise InputError("catalog has no usable GLB entries")
    metadata = {
        "units": _text(catalog.get("units"), "catalog units"),
        "authoring_axes": _text(catalog.get("authoring_axes"), "catalog authoring_axes"),
        "glb_to_authoring_coordinates": _text(catalog.get("glb_to_authoring_coordinates"), "catalog coordinate conversion"),
    }
    return sorted(entries, key=lambda item: (item["asset_id"], item["asset_glb"])), metadata


def validate_board(board: Any) -> dict[str, Any]:
    if not isinstance(board, dict) or board.get("schema_version") != 1:
        raise InputError("board schema_version must be 1")
    world_id = _id(board.get("world_id"), "world_id")
    requests = board.get("requests")
    if not isinstance(requests, list) or len(requests) > MAX_REQUESTS:
        raise InputError(f"requests must be a list of at most {MAX_REQUESTS} items")
    seen: set[str] = set()
    normalized = []
    for index, request in enumerate(requests):
        if not isinstance(request, dict):
            raise InputError(f"request {index} must be an object")
        request_id = _id(request.get("id"), f"request {index} id")
        if request_id in seen:
            raise InputError(f"duplicate request id: {request_id}")
        seen.add(request_id)
        category = _text(request.get("category"), f"request {request_id} category")
        status = _text(request.get("status"), f"request {request_id} status")
        if category not in ALLOWED_CATEGORIES or status not in ALLOWED_STATUSES:
            raise InputError(f"unsupported category/status on {request_id}")
        summary = _text(request.get("summary"), f"request {request_id} summary")
        resolution = _text(request.get("resolution"), f"request {request_id} resolution")
        requester_ids = request.get("requester_ids")
        if not isinstance(requester_ids, list) or not 1 <= len(requester_ids) <= 32:
            raise InputError(f"requester_ids on {request_id} must contain 1..32 IDs")
        normalized_request = {
            "id": request_id, "category": category, "status": status,
            "summary": summary, "resolution": resolution,
            "requester_ids": [_id(value, f"request {request_id} requester") for value in requester_ids],
        }
        if "asset_context" in request:
            if category != "asset" or status != "proposed":
                raise InputError(f"asset_context on {request_id} requires an asset/proposed request")
            if len(set(normalized_request["requester_ids"])) != len(normalized_request["requester_ids"]):
                raise InputError(f"requester_ids on {request_id} must be unique when asset_context is present")
            normalized_request["asset_context"] = _asset_context(request["asset_context"], request_id, normalized_request["requester_ids"], summary)
        normalized.append(normalized_request)
    return {"schema_version": 1, "world_id": world_id, "requests": normalized}


def _choose_assets(summary: str, entries: list[dict[str, str]]) -> list[dict[str, str]]:
    words = {word for word in re.findall(r"[a-z0-9]+", summary.lower()) if len(word) >= 4}
    matches = [entry for entry in entries if any(word in entry["asset_id"].lower() for word in words)]
    return matches[:4]


def _markdown_text(value: str) -> str:
    """Keep board-controlled text readable without allowing markdown structure."""
    return re.sub(r"([\\`*_{}\[\]()#+.!|<>~-])", r"\\\1", value)


def build_manifest(board: dict[str, Any], entries: list[dict[str, str]], catalog_metadata: dict[str, str],
                   input_sha256: str, catalog_sha256: str) -> dict[str, Any]:
    jobs = []
    for request in board["requests"]:
        if request["category"] != "asset" or request["status"] != "proposed":
            continue
        context = request.get("asset_context")
        reuse_first = [dict(item, candidate_status="unverified_candidate") for item in _choose_assets(request["summary"], entries)]
        reuse_status = "lexical_candidates_unverified" if reuse_first else "needs_catalog_review"
        target_position = context["target_position_cm"] if context else None
        purpose = context["purpose"] if context and context["purpose"] else request["summary"]
        world_id = board["world_id"]
        request_id = request["id"]
        job_id = f"art-{len(world_id)}-{world_id}-{len(request_id)}-{request_id}"
        jobs.append({
            "job_id": job_id,
            "request_id": request_id,
            "world_id": world_id,
            "state": "proposed",
            "requires_review": True,
            "brief": {
                "objective": request["summary"],
                "resolution": request["resolution"],
                "style": "low-poly medieval village; shared tiled UV; reuse the existing catalog first",
                "coordinate_convention": catalog_metadata,
                "asset_context": context,
                "model_deliverable": "A reviewable GLB model asset with stable naming, clean transforms, shared UV0, and documented materials.",
                "deliverable": "A reviewable GLB model asset; functional behavior remains a separate assessment.",
                "design_questions": {
                    "dimensions_cm": "unknown",
                    "placement": {"target_snapshot_cm": target_position, "status": "target snapshot available" if target_position is not None else "unknown"},
                    "requested_purpose": purpose,
                    "required_capabilities": "pending definition",
                    "runtime_binding": "pending assessment",
                    "npc_review": "pending",
                    "import_placement": "pending",
                },
                "acceptance": [
                    "GLB opens without missing external textures or dependencies.",
                    "Grid scale and authoring coordinate convention match the catalog metadata above.",
                    "UVs are shared/reusable and the result stays within the request scope.",
                ],
                "functional_acceptance": [
                    "Required capabilities must be defined and verified separately from model delivery.",
                    "Runtime binding, import/placement, actual Unreal evidence, and NPC review remain pending.",
                ],
                "return_contract": {
                    "delivered_asset_id": "pending",
                    "delivered_files": [],
                    "actual_bounds_and_origin": "pending",
                    "uv": "shared; verify on delivery",
                    "materials": "pending",
                    "collision": "pending",
                    "interaction_points": "pending",
                    "capability_binding_proof": "pending",
                    "actual_ue_screenshot": "pending",
                    "npc_review": "pending",
                },
                "failure_isolation": "Keep this asset in its own source/export folder; a failed job must not alter existing catalog files.",
            },
            "reuse_first": reuse_first,
            "reuse_status": reuse_status,
            "provenance": {
                "world_id": world_id, "requester_ids": request["requester_ids"],
                "asset_context": context,
                "source_board_sha256": input_sha256, "catalog_sha256": catalog_sha256,
                "external_models_called": False, "blender_launched": False,
            },
        })
    return {
        "schema_version": 1, "manifest_kind": "hearth_art_jobs", "world_id": board["world_id"],
        "execution": "offline_review_only", "jobs": jobs,
    }


def _markdown(manifest: dict[str, Any]) -> str:
    lines = [f"# Hearth art jobs: `{manifest['world_id']}`", "", "Offline review manifest; no model provider or Blender execution is implied.", ""]
    if not manifest["jobs"]:
        lines.append("No proposed asset requests produced jobs.")
        return "\n".join(lines) + "\n"
    for job in manifest["jobs"]:
        brief = job["brief"]
        lines += [f"## {job['job_id']} — proposed / requires review", "", f"**Objective:** {_markdown_text(brief['objective'])}", f"**Resolution:** {_markdown_text(brief['resolution'])}", f"**Style:** {_markdown_text(brief['style'])}", f"**Coordinates:** `{_markdown_text(json.dumps(brief['coordinate_convention'], sort_keys=True))}`", "", "Reusable catalog candidates:"]
        if job["reuse_first"]:
            lines += [f"- {_markdown_text(item['asset_id'])} — {_markdown_text(item['asset_glb'])} (unverified candidate)" for item in job["reuse_first"]]
        else:
            lines.append("- None selected; catalog review is required.")

        context = brief["asset_context"]
        lines += ["", "NPC and target context:"]
        if context is None:
            lines.append("- No NPC target context supplied.")
        else:
            position = json.dumps(context["target_position_cm"], ensure_ascii=False) if context["target_position_cm"] is not None else "unknown"
            target_id = context["target_id"] or "unknown"
            observation_id = context["observation_id"] or "empty"
            lines += [
                f"- Purpose: {_markdown_text(context['purpose'])}",
                f"- Target: `{_markdown_text(target_id)}`; position snapshot: `{_markdown_text(position)}`; observation: `{_markdown_text(observation_id)}`",
            ]
            for resident in context["resident_contexts"]:
                name = resident["name"] or resident["resident_id"]
                story = resident["inner_story"] or "empty"
                goal = resident["design_goal"] or "empty"
                personality = resident["personality"] or "empty"
                lines.append(
                    f"- Resident {_markdown_text(name)} (`{_markdown_text(resident['resident_id'])}`): "
                    f"personality {_markdown_text(personality)}; story {_markdown_text(story)}; goal {_markdown_text(goal)}"
                )

        questions = brief["design_questions"]
        placement = questions["placement"]
        lines += [
            "", "Design questions:",
            f"- Dimensions: {_markdown_text(questions['dimensions_cm'])}",
            f"- Placement: {_markdown_text(placement['status'])}; target snapshot `{_markdown_text(json.dumps(placement['target_snapshot_cm'], ensure_ascii=False) if placement['target_snapshot_cm'] is not None else 'unknown')}`",
            f"- Requested purpose: {_markdown_text(questions['requested_purpose'])}",
            f"- Required capabilities: {_markdown_text(questions['required_capabilities'])}",
            f"- Runtime binding: {_markdown_text(questions['runtime_binding'])}",
            f"- NPC review: {_markdown_text(questions['npc_review'])}",
            f"- Import/placement: {_markdown_text(questions['import_placement'])}",
            "", "Model acceptance:",
        ] + [f"- {item}" for item in brief["acceptance"]]
        lines += ["", "Functional acceptance:"] + [f"- {item}" for item in brief["functional_acceptance"]]

        contract_labels = {
            "delivered_asset_id": "Delivered asset ID",
            "delivered_files": "Delivered files",
            "actual_bounds_and_origin": "Actual bounds and origin",
            "uv": "UV",
            "materials": "Materials",
            "collision": "Collision",
            "interaction_points": "Interaction points",
            "capability_binding_proof": "Capability binding proof",
            "actual_ue_screenshot": "Actual Unreal screenshot",
            "npc_review": "NPC review",
        }
        lines += ["", "Return contract:"]
        for key, label in contract_labels.items():
            value = brief["return_contract"][key]
            if isinstance(value, list):
                value = ", ".join(str(item) for item in value) or "pending"
            lines.append(f"- {label}: {_markdown_text(str(value))}")
        lines += ["", f"Reuse status: {job['reuse_status']}", f"Failure isolation: {brief['failure_isolation']}", f"Provenance: board `{job['provenance']['source_board_sha256'][:12]}`; catalog `{job['provenance']['catalog_sha256'][:12]}`.", ""]
    return "\n".join(lines)


def _atomic_write(path: Path, data: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(dir=path.parent, prefix=f".{path.name}.", delete=False) as handle:
        temp = Path(handle.name)
        handle.write(data)
        handle.flush()
        os.fsync(handle.fileno())
    os.replace(temp, path)


def export(input_path: Path, output_dir: Path, catalog_path: Path) -> dict[str, Any]:
    board = validate_board(_load_json(input_path, "board"))
    catalog_document = _load_json(catalog_path, "catalog")
    entries, catalog_metadata = _catalog_entries(catalog_document, catalog_path)
    manifest = build_manifest(board, entries, catalog_metadata, _digest(input_path), _digest(catalog_path))
    encoded = json.dumps(manifest, ensure_ascii=False, indent=2, sort_keys=True).encode("utf-8") + b"\n"
    _atomic_write(output_dir / OUTPUT_JSON, encoded)
    _atomic_write(output_dir / OUTPUT_MD, _markdown(manifest).encode("utf-8"))
    return manifest


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--output-dir", required=True, type=Path)
    parser.add_argument("--catalog", type=Path, default=Path(__file__).resolve().parents[3] / "Art" / "Stage4HouseAudit" / "component-catalog.json")
    args = parser.parse_args(argv)
    try:
        export(args.input, args.output_dir, args.catalog)
    except (InputError, OSError) as exc:
        parser.error(str(exc))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
