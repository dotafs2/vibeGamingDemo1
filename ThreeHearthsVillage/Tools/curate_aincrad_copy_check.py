#!/usr/bin/env python3
"""Curate the original evidence named by a local workpoint check."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import re
import shutil
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / "Saved" / "ThreeHearths" / "AincradLevel0"
CHECK_ROOT = BASE / "TwoHourIteration"
DEST_ROOT = ROOT / "Docs" / "Validation" / "Two_Hour_Iteration_2026-09-08" / "NativeWorkpoint"
NAME_RE = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._-]*$")
GUID_RE = re.compile(r"^[0-9A-Fa-f]{8}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{12}$")


def read_json(path: Path) -> dict[str, Any]:
    try:
        value = json.loads(path.read_text(encoding="utf-8-sig"))
    except (OSError, UnicodeError, json.JSONDecodeError) as exc:
        raise RuntimeError(f"cannot read JSON: {path}: {exc}") from exc
    if not isinstance(value, dict):
        raise RuntimeError(f"JSON object required: {path}")
    return value


def inside(path: Path, root: Path) -> bool:
    try:
        path.relative_to(root)
        return True
    except ValueError:
        return False


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def resolve_check(value: str) -> Path:
    candidate = Path(value)
    if not candidate.is_absolute():
        candidate = ROOT / candidate
    candidate = candidate.resolve()
    if candidate.suffix.lower() != ".json" or not inside(candidate, CHECK_ROOT.resolve()):
        raise RuntimeError("--check must be a JSON file inside Saved/ThreeHearths/AincradLevel0/TwoHourIteration")
    if not candidate.is_file():
        raise RuntimeError(f"check file does not exist: {candidate}")
    return candidate


def copy_one(source: Path, destination: Path, copied: list[dict[str, Any]]) -> None:
    if not source.is_file():
        raise RuntimeError(f"required evidence file does not exist: {source}")
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, destination)
    copied.append({"file": destination.relative_to(DEST_ROOT).as_posix(), "sha256": sha256(destination)})


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", required=True, help="workpoint check JSON")
    parser.add_argument("--copy-name", required=True, help="single destination directory name")
    args = parser.parse_args()
    if not NAME_RE.fullmatch(args.copy_name) or args.copy_name in {".", ".."}:
        raise RuntimeError("--copy-name must be one safe directory name")

    check_path = resolve_check(args.check)
    check = read_json(check_path)
    if check.get("source") != "local_verification" or check.get("formal_unchanged") is not True:
        raise RuntimeError("check must assert source=local_verification and formal_unchanged=true")
    world_id = check.get("world_id")
    if not isinstance(world_id, str) or not GUID_RE.fullmatch(world_id):
        raise RuntimeError("check.world_id must be a GUID")
    residents = check.get("residents")
    if not isinstance(residents, list) or len(residents) != 3:
        raise RuntimeError("check must contain exactly three residents")

    destination = (DEST_ROOT / args.copy_name).resolve()
    if not inside(destination, DEST_ROOT.resolve()):
        raise RuntimeError("copy destination escapes the validation directory")
    if destination.exists():
        raise RuntimeError(f"copy destination already exists: {destination}")
    verification_root = (BASE / "VerificationObservations").resolve()
    source_world = (verification_root / args.copy_name / world_id).resolve()
    if not inside(source_world, verification_root) or not source_world.is_dir():
        raise RuntimeError(f"verified source directory does not exist: {source_world}")
    plans: list[tuple[str, str, Path, Path, Path, Path, Path]] = []
    for resident in residents:
        if not isinstance(resident, dict) or not isinstance(resident.get("id"), str) or not GUID_RE.fullmatch(resident["id"]):
            raise RuntimeError("each resident must have an id")
        resident_id = resident["id"]
        observation_id = resident.get("observation") or resident.get("last_observation_id")
        if not isinstance(observation_id, str) or not re.fullmatch(r"obs_[0-9]+", observation_id):
            raise RuntimeError(f"resident {resident_id} has no safe observation id")
        source_dir = (source_world / resident_id).resolve()
        if not inside(source_dir, source_world) or not source_dir.is_dir():
            raise RuntimeError(f"verified resident source directory does not exist: {source_dir}")
        metadata = source_dir / f"{observation_id}.json"
        image = source_dir / f"{observation_id}.png"
        if not metadata.is_file() or not image.is_file():
            raise RuntimeError(f"verified observation PNG and metadata are required: {source_dir}")
        if not (source_dir / "events.jsonl").is_file() or not (source_dir / "history.jsonl").is_file():
            raise RuntimeError(f"verified events.jsonl and history.jsonl are required: {source_dir}")
        metadata_obj = read_json(metadata)
        if metadata_obj.get("world_id") != world_id or metadata_obj.get("resident_id") != resident_id:
            raise RuntimeError(f"observation metadata identity mismatch: {metadata}")
        if metadata_obj.get("observation_id") != observation_id:
            raise RuntimeError(f"observation id mismatch: {metadata}")
        if metadata_obj.get("source") != "local_verification":
            raise RuntimeError(f"observation metadata source is not local_verification: {metadata}")
        expected = resident.get("position_cm")
        actual = metadata_obj.get("position_cm")
        if not isinstance(expected, list) or not isinstance(actual, list) or len(expected) != 3 or len(actual) != 3:
            raise RuntimeError(f"position_cm must contain three coordinates: {metadata}")
        if any(not isinstance(a, (int, float)) or not isinstance(b, (int, float)) or not math.isclose(float(a), float(b), abs_tol=0.05) for a, b in zip(actual, expected)):
            raise RuntimeError(f"observation position differs from check by more than 0.05 cm: {metadata}")
        if metadata_obj.get("image_sha1", "").upper() != hashlib.sha1(image.read_bytes()).hexdigest().upper():
            raise RuntimeError(f"observation PNG SHA-1 mismatch: {metadata}")
        plans.append((resident_id, observation_id, source_dir, image, metadata, source_dir / "events.jsonl", source_dir / "history.jsonl"))

    copied: list[dict[str, Any]] = []
    observations: list[dict[str, Any]] = []
    for resident_id, observation_id, source_dir, image, metadata, events, history in plans:
        out_dir = destination / resident_id
        copy_one(image, out_dir / image.name, copied)
        copy_one(metadata, out_dir / metadata.name, copied)
        copy_one(events, out_dir / events.name, copied)
        copy_one(history, out_dir / history.name, copied)
        observations.append({"resident_id": resident_id, "observation_id": observation_id})

    copy_one(check_path, destination / "workpoint-check.json", copied)
    manifest = {
        "world_id": world_id,
        "source": "local_verification",
        "formal_unchanged": True,
        "check": check_path.relative_to(ROOT).as_posix(),
        "observations": observations,
        "files": copied,
        "limits": "Original PNG, metadata, events, and history bytes were copied; arrival evidence does not prove work completion.",
    }
    destination.mkdir(parents=True, exist_ok=True)
    (destination / "manifest.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"copy_name": args.copy_name, "world_id": world_id, "residents": len(observations), "files": len(copied)}, ensure_ascii=False))


if __name__ == "__main__":
    try:
        main()
    except RuntimeError as exc:
        raise SystemExit(f"error: {exc}") from exc
