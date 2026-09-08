"""Local-only HTTP fixture and v4 thinking-gate integration harness.

This tool never contacts an external host and never uses a real key or budget
ledger.  It writes an isolated api-config.json and world save under
Saved/MedievalReview/MockHttp, then optionally launches a caller-supplied UE
executable.  The HTTP fixture accepts only the small JSON choices that the
ThreeHearths decision parser accepts and records the resident/context and real
wall-clock request spacing for post-run analysis.

Examples (PowerShell):
  python Tools/test_thinking_http_runtime.py --prepare-only
  python Tools/test_thinking_http_runtime.py --launch --ue-exe D:/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe --project CropoutSampleProject.uproject

The launcher deliberately builds a raw Windows command line.  In particular,
the ExecCmds token remains ``-ExecCmds="..."`` with the equals sign outside
the quoted command, which is required by the UE command-line parser.

Acceptance is deliberately bounded: routine needs a real life request, no
invalid fixture responses, no same-resident life interval below 1800 real
seconds, and no UE crash; deadline needs a past deadline argument and zero
requests; restart additionally needs the same saved world GUID, valid thinking
sidecars, no second-run routine request, and natural UE exits.  A duration
timeout is recorded as harness termination and can never satisfy restart.
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass, field
from datetime import datetime, timedelta, timezone
import json
import math
from pathlib import Path
import subprocess
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from typing import Any, Optional
from urllib.parse import urlparse
import uuid


ROOT = Path(__file__).resolve().parents[3]
DEFAULT_OUTPUT = ROOT / "Saved" / "MedievalReview" / "MockHttp"


def utc_now() -> datetime:
    return datetime.now(timezone.utc)


def utc_iso(value: datetime) -> str:
    return value.astimezone(timezone.utc).isoformat().replace("+00:00", "Z")


def json_line(value: Any) -> str:
    return json.dumps(value, ensure_ascii=False, separators=(",", ":"))


def extract_json_context(body: dict[str, Any]) -> tuple[Optional[dict[str, Any]], str]:
    """Find the final decision-context JSON in a chat completion request."""
    messages = body.get("messages")
    if not isinstance(messages, list):
        return None, "missing_messages"
    for message in reversed(messages):
        if not isinstance(message, dict):
            continue
        content = message.get("content")
        candidates: list[str] = []
        if isinstance(content, str):
            candidates.append(content)
        elif isinstance(content, list):
            for part in content:
                if isinstance(part, dict) and isinstance(part.get("text"), str):
                    candidates.append(part["text"])
        for candidate in reversed(candidates):
            try:
                parsed = json.loads(candidate)
            except json.JSONDecodeError:
                continue
            if isinstance(parsed, dict):
                return parsed, "json_context"
    return None, "no_json_context"


def resident_id(context: dict[str, Any], headers: dict[str, str]) -> str:
    for key in ("resident_id", "persistent_character_id"):
        if isinstance(context.get(key), str) and context[key]:
            return context[key]
    person = context.get("resident")
    if isinstance(person, dict) and isinstance(person.get("stable_id"), str):
        return person["stable_id"]
    participants = context.get("participants_speaker_first")
    if isinstance(participants, list) and participants and isinstance(participants[0], dict):
        speaker = participants[0].get("stable_id")
        if isinstance(speaker, str) and speaker:
            return speaker
    if isinstance(headers.get("X-Hearth-Resident"), str) and headers["X-Hearth-Resident"]:
        return headers["X-Hearth-Resident"]
    return "unknown-resident"


def choose_reply(context: dict[str, Any]) -> tuple[dict[str, Any], str]:
    """Choose the first supplied option, never inventing an action or plot."""
    plots = context.get("available_plots")
    styles = context.get("available_house_styles")
    if isinstance(plots, list) and plots and isinstance(styles, list) and styles:
        plot = plots[0] if isinstance(plots[0], dict) else {}
        style = styles[0] if isinstance(styles[0], dict) else {}
        if isinstance(plot.get("id"), int) and isinstance(style.get("id"), int):
            return {"plot_id": plot["id"], "house_style_id": style["id"], "reason": "按提供的首个可用地块与样式。"}, "house"
        raise ValueError("supplied house options are not integer keyed")

    for key in ("available_actions", "allowed_intents"):
        choices = context.get(key)
        if isinstance(choices, list) and choices:
            first = choices[0] if isinstance(choices[0], dict) else {}
            if isinstance(first.get("id"), int):
                kind = "social" if key == "allowed_intents" else "life"
                return {"action_id": first["id"], "reason": "按提供的首个可执行选项。"}, kind
            raise ValueError(f"supplied {key} has no integer id")

    # Visual review carries its bounded option list as prose rather than an
    # array.  Option 0 is always part of that host supplied list.
    if isinstance(context.get("options"), str):
        return {"action_id": 0, "reason": "看见：外观结构；未知：内部状态；打算：继续观察。"}, "visual"
    raise ValueError("request did not supply a recognized bounded option set")


@dataclass
class RequestRecord:
    at_utc: str
    monotonic: float
    phase: str
    resident: str
    kind: str
    model: str
    event_hint: str
    response: dict[str, Any]
    request: dict[str, Any]


@dataclass
class FixtureState:
    records: list[RequestRecord] = field(default_factory=list)
    received_count: int = 0
    invalid_requests: list[dict[str, Any]] = field(default_factory=list)
    phase: str = "unassigned"
    lock: threading.Lock = field(default_factory=threading.Lock)

    def set_phase(self, phase: str) -> None:
        with self.lock:
            self.phase = phase

    def add(self, record: RequestRecord) -> None:
        with self.lock:
            self.records.append(record)

    def received(self) -> None:
        with self.lock:
            self.received_count += 1

    def invalid(self, reason: str) -> None:
        with self.lock:
            self.invalid_requests.append({"at_utc": utc_iso(utc_now()), "phase": self.phase, "reason": reason})


class FixtureHandler(BaseHTTPRequestHandler):
    server_version = "ThreeHearthsOfflineFixture/1"

    def log_message(self, fmt: str, *args: Any) -> None:
        return

    def send_json(self, status: int, payload: dict[str, Any]) -> None:
        raw = json_line(payload).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(raw)))
        self.end_headers()
        self.wfile.write(raw)

    def do_POST(self) -> None:  # noqa: N802 - BaseHTTPRequestHandler API
        parsed = urlparse(self.path)
        state: FixtureState = self.server.fixture_state  # type: ignore[attr-defined]
        state.received()
        if not parsed.path.endswith("/chat/completions"):
            state.invalid("wrong_path")
            self.send_json(404, {"error": {"message": "fixture only serves /chat/completions"}})
            return
        try:
            length = int(self.headers.get("Content-Length", "0"))
            if length <= 0 or length > 2 * 1024 * 1024:
                raise ValueError("invalid content length")
            body = json.loads(self.rfile.read(length).decode("utf-8"))
            if not isinstance(body, dict) or body.get("model") != "offline_fixture":
                raise ValueError("fixture requires model=offline_fixture")
            context, context_status = extract_json_context(body)
            if context is None:
                raise ValueError(context_status)
            reply, kind = choose_reply(context)
            headers = {key: value for key, value in self.headers.items()}
            with state.lock:
                phase = state.phase
            record = RequestRecord(
                at_utc=utc_iso(utc_now()), monotonic=time.monotonic(), phase=phase,
                resident=resident_id(context, headers), kind=kind,
                model=str(body.get("model")),
                event_hint=str(context.get("observation_id") or context.get("conversation_id") or context.get("persistent_character_id") or ""),
                response=reply, request={"context": context, "headers": headers},
            )
            state.add(record)
            envelope = {
                "id": "offline-" + uuid.uuid4().hex,
                "object": "chat.completion",
                "model": "offline_fixture",
                "choices": [{"index": 0, "finish_reason": "stop", "message": {"role": "assistant", "content": json_line(reply)}}],
                "usage": {"prompt_tokens": 1, "completion_tokens": 1, "total_tokens": 2},
            }
            self.send_json(200, envelope)
        except (ValueError, UnicodeDecodeError, json.JSONDecodeError) as exc:
            state.invalid(str(exc))
            self.send_json(422, {"error": {"message": str(exc)}})


class FixtureServer:
    def __init__(self) -> None:
        self.state = FixtureState()
        self.http = ThreadingHTTPServer(("127.0.0.1", 0), FixtureHandler)
        self.http.fixture_state = self.state  # type: ignore[attr-defined]
        self.thread = threading.Thread(target=self.http.serve_forever, name="offline-thinking-http", daemon=True)

    @property
    def port(self) -> int:
        return int(self.http.server_address[1])

    def start(self) -> None:
        self.thread.start()

    def stop(self) -> None:
        self.http.shutdown()
        self.http.server_close()
        self.thread.join(timeout=3)


def write_config(output: Path, port: int, max_requests: int) -> Path:
    output.mkdir(parents=True, exist_ok=True)
    config = {
        "enabled": True,
        "backend": "openai_compatible",
        "base_url": f"http://127.0.0.1:{port}/v1",
        "api_key": "offline-fixture-no-secret",
        "model": "offline_fixture",
        "timeout_seconds": 10,
        "max_output_tokens": 128,
        "token_limit_field": "max_tokens",
        "response_format": "json_object",
        "max_requests_per_run": max_requests,
        "autonomous_life": True,
        "life_decision_interval_seconds": 6,
    }
    path = output / "api-config.json"
    path.write_text(json.dumps(config, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    return path


def win_quote(value: str) -> str:
    return '"' + value.replace('"', '\\"') + '"'


def build_raw_command(args: argparse.Namespace, config: Path, world: Path, deadline: Optional[str], abslog: Path,
                      history: Path) -> str:
    if not args.ue_exe or not args.project:
        raise ValueError("UE executable and project are required to build a launch command")
    exe = Path(args.ue_exe).resolve()
    project = Path(args.project).resolve()
    tokens = [win_quote(str(exe)), win_quote(str(project)), "-game", "-log", "-HearthOrganicVillage",
              f"-HearthApiConfig={win_quote(str(config))}", f"-HearthWorld={win_quote(str(world))}",
              f"-HearthSimulationSpeed={args.simulation_speed}", f"-HearthApiConcurrency={args.api_concurrency}",
              f"-HearthReviewDurationSeconds={args.duration:g}", f"-HearthHistory={win_quote(str(history))}",
              "-unattended", "-nop4", "-nosplash", "-NullRHI", "-NoSound",
              f"-abslog={win_quote(str(abslog))}"]
    if args.unpaused:
        tokens.append("-HearthUnpaused")
    if deadline:
        tokens.append(f"-HearthApiDeadlineUtc={deadline}")
    # Keep this token raw.  Do not turn it into -ExecCmds=\"...\" with the
    # equals sign inside the quote: UE's FParse::Value expects this exact form.
    tokens.append(f'-ExecCmds="{args.exec_cmds}"')
    return " ".join(tokens)


def hidden_startupinfo() -> Any:
    if sys.platform != "win32":
        return None
    info = subprocess.STARTUPINFO()
    info.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    info.wShowWindow = 0
    return info


def launch_ue(args: argparse.Namespace, raw_command: str, log_path: Path) -> dict[str, Any]:
    creation_flags = 0
    if sys.platform == "win32":
        creation_flags = subprocess.CREATE_NEW_PROCESS_GROUP | subprocess.CREATE_NO_WINDOW
    started_at = utc_now()
    started_monotonic = time.monotonic()
    log_path.parent.mkdir(parents=True, exist_ok=True)
    with log_path.open("w", encoding="utf-8", errors="replace") as log:
        log.write("RAW_COMMAND=" + raw_command + "\n")
        log.flush()
        try:
            process = subprocess.Popen(raw_command, shell=False, startupinfo=hidden_startupinfo(),
                                       creationflags=creation_flags, stdout=subprocess.PIPE,
                                       stderr=subprocess.STDOUT, text=True, encoding="utf-8", errors="replace")
        except OSError as exc:
            return {"started": False, "pid": None, "start_utc": utc_iso(started_at), "end_utc": utc_iso(utc_now()),
                    "duration_seconds": time.monotonic()-started_monotonic, "return_code": None,
                    "timed_out": False, "normal_exit": False, "error": str(exc), "stdout_log": str(log_path)}
        timed_out = False
        try:
            output, _ = process.communicate(timeout=args.duration + args.startup_grace_seconds)
        except subprocess.TimeoutExpired as exc:
            # Terminate only the process created by this Popen call.  No
            # taskkill-by-image-name or broad process enumeration is used.
            timed_out = True
            process.terminate()
            try:
                remaining_output, _ = process.communicate(timeout=10)
            except subprocess.TimeoutExpired:
                process.kill()
                remaining_output, _ = process.communicate(timeout=10)
            # In text mode TimeoutExpired.output may still be bytes, and the
            # second communicate may already contain the complete stream.
            output = remaining_output if remaining_output else exc.output
        if isinstance(output, bytes):
            output = output.decode("utf-8", errors="replace")
        log.write(output or "")
        ended_at = utc_now()
        timeout_error = "duration exceeded; harness terminated this process" if timed_out else None
        return {"started": True, "pid": process.pid, "start_utc": utc_iso(started_at),
                "end_utc": utc_iso(ended_at), "duration_seconds": time.monotonic()-started_monotonic,
                "return_code": process.returncode, "timed_out": timed_out,
                "normal_exit": not timed_out and process.returncode == 0,
                "error": timeout_error, "stdout_log": str(log_path)}


def safe_world_path(output: Path, name: str) -> Path:
    world_dir = (output / "world").resolve()
    world_dir.mkdir(parents=True, exist_ok=True)
    path = (world_dir / f"{name}.json").resolve()
    if world_dir not in path.parents:
        raise ValueError("world path escaped the isolated output directory")
    return path


def read_json_file(path: Optional[Path]) -> Optional[Any]:
    if not path or not path.is_file():
        return None
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError):
        return None


def read_world_snapshot(world_path: Path) -> Optional[dict[str, Any]]:
    """Read one immutable post-run world envelope and its decoded payload."""
    envelope = read_json_file(world_path)
    if not isinstance(envelope, dict) or not isinstance(envelope.get("payload"), str):
        return None
    try:
        payload = json.loads(envelope["payload"])
    except (UnicodeError, json.JSONDecodeError):
        return None
    if not isinstance(payload, dict):
        return None
    original_id = payload.get("Id")
    canonical_id: Optional[str] = None
    if isinstance(original_id, str):
        try:
            canonical_id = str(uuid.UUID(original_id))
        except ValueError:
            canonical_id = None
    elapsed = payload.get("Elapsed")
    return {"path": str(world_path), "captured_at_utc": utc_iso(utc_now()),
            "envelope": envelope, "payload": payload, "id_original": original_id,
            "id_canonical": canonical_id, "elapsed": elapsed}


def capture_run_evidence(project: Optional[Path], world_path: Path, abslog: Path,
                         stdout_log: Path) -> dict[str, Any]:
    world_snapshot = read_world_snapshot(world_path)
    original_id = world_snapshot.get("id_original") if world_snapshot else None
    sidecar_path = sidecar_for_project(project, original_id if isinstance(original_id, str) else None)
    sidecar_snapshot = read_json_file(sidecar_path)
    log_text = ""
    for path in (abslog, stdout_log):
        if path.is_file():
            try:
                log_text += path.read_text(encoding="utf-8", errors="replace")
            except OSError:
                pass
    return {"captured_at_utc": utc_iso(utc_now()), "world": world_snapshot,
            "sidecar_path": str(sidecar_path) if sidecar_path else None,
            "sidecar": sidecar_snapshot, "review_exit_marker": "HEARTH_REVIEW_EXIT" in log_text}


def run_scenario(args: argparse.Namespace, server: FixtureServer, config: Path, output: Path,
                 scenario: str, world_name: str, deadline: Optional[str] = None,
                 phase_suffix: str = "") -> dict[str, Any]:
    server.state.set_phase(scenario + phase_suffix)
    world = safe_world_path(output, world_name)
    stdout_log = output / "logs" / f"{scenario}{phase_suffix}.stdout.log"
    abslog = output / "logs" / f"{scenario}{phase_suffix}.abslog.log"
    history = output / "history" / f"{scenario}{phase_suffix}.json"
    raw = build_raw_command(args, config, world, deadline, abslog, history) if args.ue_exe and args.project else None
    if args.launch:
        if not args.ue_exe or not args.project:
            raise ValueError("--launch requires --ue-exe and --project")
        result = launch_ue(args, raw, stdout_log)
        result.update({"scenario": scenario + phase_suffix, "raw_command": raw, "world": str(world),
                       "deadline_utc": deadline, "abslog": str(abslog), "history": str(history),
                       "evidence": capture_run_evidence(args.project, world, abslog, stdout_log)})
        return result
    return {"scenario": scenario + phase_suffix, "prepared": True, "raw_command": raw,
            "world": str(world), "deadline_utc": deadline, "abslog": str(abslog), "history": str(history),
            "stdout_log": str(stdout_log)}


def records_json(state: FixtureState) -> list[dict[str, Any]]:
    with state.lock:
        return [{"at_utc": r.at_utc, "monotonic": r.monotonic, "phase": r.phase, "resident": r.resident,
                 "kind": r.kind, "model": r.model, "event_hint": r.event_hint, "response": r.response,
                 "request": r.request} for r in state.records]


def analyze(state: FixtureState, routine_min_seconds: float) -> dict[str, Any]:
    rows = records_json(state)
    by_resident: dict[str, list[dict[str, Any]]] = {}
    for row in rows:
        by_resident.setdefault(row["resident"], []).append(row)
    intervals: list[dict[str, Any]] = []
    routine_fast: list[dict[str, Any]] = []
    for resident, resident_rows in by_resident.items():
        resident_rows.sort(key=lambda row: row["monotonic"])
        for before, after in zip(resident_rows, resident_rows[1:]):
            gap = after["monotonic"] - before["monotonic"]
            intervals.append({"resident": resident, "from": before["kind"], "to": after["kind"], "seconds": gap})
    by_phase_resident: dict[tuple[str, str], list[dict[str, Any]]] = {}
    for row in rows:
        by_phase_resident.setdefault((row["phase"], row["resident"]), []).append(row)
    for (phase, resident), phase_rows in by_phase_resident.items():
        phase_rows.sort(key=lambda row: row["monotonic"])
        for before, after in zip(phase_rows, phase_rows[1:]):
            gap = after["monotonic"] - before["monotonic"]
            if before["kind"] == "life" and after["kind"] == "life" and gap < routine_min_seconds:
                routine_fast.append({"phase": phase, "resident": resident, "from": before["kind"],
                                     "to": after["kind"], "seconds": gap})
    phases = {phase: sum(1 for row in rows if row["phase"] == phase) for phase in sorted({row["phase"] for row in rows})}
    with state.lock:
        received_count = state.received_count
        invalid_requests = list(state.invalid_requests)
    return {"request_count": len(rows), "received_count": received_count,
            "invalid_count": len(invalid_requests), "invalid_requests": invalid_requests,
            "requests_by_phase": phases,
            "residents": {resident: len(items) for resident, items in sorted(by_resident.items())},
            "intervals": intervals, "routine_fast_intervals": routine_fast,
            "records": rows}


def read_world_id(world_path: Path) -> Optional[str]:
    """Read the GUID from the normal HearthWorld envelope without touching UE."""
    snapshot = read_world_snapshot(world_path)
    return snapshot.get("id_canonical") if snapshot else None


def sidecar_for_project(project: Optional[Path], world_id: Optional[str]) -> Optional[Path]:
    if not project or not world_id:
        return None
    project_root = project.resolve().parent if project.suffix.lower() == ".uproject" else project.resolve()
    return project_root / "Saved" / "ThreeHearths" / "World" / f"{world_id}.thinking.json"


def sidecar_is_valid(path: Optional[Path]) -> bool:
    data = read_json_file(path)
    schema = data.get("schema") if isinstance(data, dict) else None
    if not isinstance(schema, (int, float)) or isinstance(schema, bool) or schema != 1:
        return False
    suspended = data.get("suspended_until_utc")
    if not isinstance(suspended, (int, float)) or isinstance(suspended, bool) or not math.isfinite(suspended):
        return False
    residents = data.get("residents")
    if not isinstance(residents, list) or len(residents) > 40:
        return False
    numeric_fields = ("last_api_monotonic", "last_api_utc", "urgent_window_monotonic",
                      "urgent_window_utc", "urgent_count", "local_routine_monotonic",
                      "local_routine_utc", "local_daydream_monotonic", "local_daydream_utc")
    for resident in residents:
        if not isinstance(resident, dict) or not isinstance(resident.get("resident_id"), str) \
                or not isinstance(resident.get("role"), str) or not isinstance(resident.get("seen_event_ids"), list):
            return False
        if any(not isinstance(resident.get(field), (int, float)) or isinstance(resident.get(field), bool)
               or not math.isfinite(resident.get(field))
               for field in numeric_fields):
            return False
        if any(not isinstance(event_id, str) or not event_id or len(event_id) > 512
               for event_id in resident["seen_event_ids"]):
            return False
    for name, maximum in (("pending", 16), ("uncertain_in_flight", 10)):
        values = data.get(name)
        if not isinstance(values, list) or len(values) > maximum:
            return False
        for request in values:
            if not isinstance(request, dict) or not isinstance(request.get("resident_id"), str) \
                    or len(request["resident_id"]) > 256 or not isinstance(request.get("role"), str) \
                    or request["role"] not in {"civilian", "gatekeeper", "royal_guard", "carter"} \
                    or not isinstance(request.get("trigger"), str) \
                    or request["trigger"] not in {"routine", "actual_interaction", "important_event", "daydream"} \
                    or not isinstance(request.get("event_id"), str) or not request["event_id"] \
                    or len(request["event_id"]) > 512:
                return False
            if not isinstance(request.get("coalesce_key", ""), str) or len(request.get("coalesce_key", "")) > 512 \
                    or not isinstance(request.get("context", ""), str) or len(request.get("context", "")) > 65536:
                return False
            deadline = request.get("deadline_utc")
            if deadline is not None and (not isinstance(deadline, (int, float)) or isinstance(deadline, bool)
                                         or not math.isfinite(deadline)):
                return False
    return True


def evaluate_scenarios(args: argparse.Namespace, runs: list[dict[str, Any]], analysis: dict[str, Any]) -> dict[str, Any]:
    """Return explicit gates; no scenario is silently called passed."""
    gates: dict[str, Any] = {}
    for run in runs:
        scenario = run["scenario"].split("-")[0]
        if not run.get("started"):
            gates.setdefault(scenario, []).append({"ok": False, "reason": "UE did not start"})
    if args.launch:
        if args.scenario in ("all", "routine"):
            life_rows = [row for row in analysis["records"] if row["phase"] == "routine" and row["kind"] == "life"]
            routine_runs = [r for r in runs if r["scenario"] == "routine"]
            routine_process_ok = all(r.get("started") and (r.get("return_code") == 0 or r.get("timed_out")) for r in routine_runs)
            routine_fast = [row for row in analysis["routine_fast_intervals"] if row["phase"] == "routine"]
            gates.setdefault("routine", []).append({"ok": bool(life_rows) and not routine_fast
                and analysis["invalid_count"] == 0 and routine_process_ok,
                "reason": "requires valid life HTTP, no same-resident life gap below 1800 seconds, and no UE crash",
                "life_http_count": len(life_rows), "fast_intervals": routine_fast,
                "process_ok": routine_process_ok})
        if args.scenario in ("all", "deadline"):
            deadline_rows = [row for row in analysis["records"] if row["phase"] == "deadline"]
            deadline_run = next((r for r in runs if r["scenario"] == "deadline"), {})
            deadline_arg_ok = bool(deadline_run.get("deadline_utc")) and "-HearthApiDeadlineUtc=" in str(deadline_run.get("raw_command"))
            try:
                deadline_is_past = datetime.fromisoformat(str(deadline_run.get("deadline_utc", "")).replace("Z", "+00:00")) < utc_now()
            except ValueError:
                deadline_is_past = False
            deadline_process_ok = bool(deadline_run.get("started")) and (deadline_run.get("return_code") == 0 or deadline_run.get("timed_out"))
            deadline_evidence = deadline_run.get("evidence") or {}
            deadline_world = deadline_evidence.get("world") if isinstance(deadline_evidence, dict) else None
            deadline_elapsed = deadline_world.get("elapsed") if isinstance(deadline_world, dict) else None
            deadline_elapsed_ok = isinstance(deadline_elapsed, (int, float)) and not isinstance(deadline_elapsed, bool) and deadline_elapsed > 0.0
            gates.setdefault("deadline", []).append({"ok": not deadline_rows and analysis["invalid_count"] == 0
                and deadline_process_ok and deadline_arg_ok and deadline_is_past
                and bool(deadline_evidence.get("review_exit_marker")) and deadline_elapsed_ok,
                "reason": "requires past deadline, zero HTTP, HEARTH_REVIEW_EXIT, and positive saved Elapsed",
                "http_count": len(deadline_rows), "deadline_arg_ok": deadline_arg_ok,
                "deadline_is_past": deadline_is_past, "process_ok": deadline_process_ok,
                "review_exit_marker": bool(deadline_evidence.get("review_exit_marker")),
                "saved_elapsed": deadline_elapsed, "deadline_elapsed_ok": deadline_elapsed_ok,
                "deadline_run": deadline_run})
        if args.scenario in ("all", "restart"):
            restart_runs = [r for r in runs if r["scenario"].startswith("restart")]
            world_ids = [((r.get("evidence") or {}).get("world") or {}).get("id_canonical") for r in restart_runs]
            second_life = [row for row in analysis["records"] if row["phase"] == "restart-2" and row["kind"] == "life"]
            first_restart_http = [row for row in analysis["records"] if row["phase"] == "restart-1"]
            sidecars = [((r.get("evidence") or {}).get("sidecar_path")) for r in restart_runs]
            sidecar_ok = bool(restart_runs and all(sidecar_is_valid(Path(path)) if path else False for path in sidecars))
            normal_exit = all(run.get("normal_exit", False) for run in restart_runs)
            gates.setdefault("restart", []).append({"ok": len(world_ids) == 2 and world_ids[0] == world_ids[1]
                and sidecar_ok and bool(first_restart_http) and not second_life and normal_exit and analysis["invalid_count"] == 0,
                "reason": "requires equal persisted world GUID, valid thinking sidecar, no second-run routine burst, and natural UE exits",
                "world_ids": world_ids, "sidecars": sidecars, "first_run_http": len(first_restart_http),
                "second_run_life_http": len(second_life), "normal_exit": normal_exit})
    return gates


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--scenario", choices=("all", "routine", "deadline", "restart"), default="all")
    parser.add_argument("--launch", action="store_true", help="launch the supplied UE executable")
    parser.add_argument("--prepare-only", action="store_true", help="write config and raw commands without starting the fixture or UE")
    parser.add_argument("--ue-exe", type=Path)
    parser.add_argument("--project", type=Path)
    parser.add_argument("--duration", type=float, default=45.0,
                        help="real wall-clock review duration passed as -HearthReviewDurationSeconds")
    parser.add_argument("--startup-grace-seconds", type=float, default=90.0,
                        help="additional launch/shader startup allowance before harness termination")
    parser.add_argument("--simulation-speed", type=float, default=30.0)
    parser.add_argument("--api-concurrency", type=int, default=4)
    parser.add_argument("--exec-cmds", default="t.MaxFPS 30", help='raw commands inside -ExecCmds="..."')
    parser.add_argument("--unpaused", action="store_true", default=True)
    parser.add_argument("--routine-min-seconds", type=float, default=1800.0)
    parser.add_argument("--max-requests", type=int, default=128)
    parser.add_argument("--report", type=Path)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if args.api_concurrency < 1 or args.api_concurrency > 10:
        raise SystemExit("--api-concurrency must be between 1 and 10")
    if not 10.0 <= args.duration <= 1800.0:
        raise SystemExit("--duration must be between 10 and 1800 seconds")
    if not 30.0 <= args.startup_grace_seconds <= 300.0:
        raise SystemExit("--startup-grace-seconds must be between 30 and 300 seconds")
    if args.routine_min_seconds < 1800.0:
        raise SystemExit("--routine-min-seconds must be at least 1800 seconds")
    if args.max_requests < 1:
        raise SystemExit("--max-requests must be positive")
    if args.simulation_speed <= 0.0:
        raise SystemExit("--simulation-speed must be positive")
    if args.prepare_only:
        args.launch = False
    output = args.output_dir.resolve()
    output.mkdir(parents=True, exist_ok=True)
    if args.prepare_only:
        config = write_config(output, 18791, args.max_requests)
        prep_server = FixtureServer()
        run_tag = utc_now().strftime("%Y%m%d-%H%M%S") + "-" + uuid.uuid4().hex[:8]
        try:
            commands: dict[str, Any] = {}
            for scenario in (("routine", "deadline", "restart") if args.scenario == "all" else (args.scenario,)):
                deadline = utc_iso(utc_now() - timedelta(seconds=2)) if scenario == "deadline" else None
                commands[scenario] = run_scenario(args, prep_server, config, output, scenario,
                                                   f"{scenario}-{run_tag}", deadline)
        finally:
            prep_server.http.server_close()
        report = {"status": "prepared", "config": str(config), "commands": commands,
                  "scope": "local fixture configuration only; UE was not started"}
        report_path = args.report or output / "prepare-report.json"
        report_path.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        print(json_line(report))
        return 0

    server = FixtureServer()
    server.start()
    config = write_config(output, server.port, args.max_requests)
    runs: list[dict[str, Any]] = []
    run_tag = utc_now().strftime("%Y%m%d-%H%M%S") + "-" + uuid.uuid4().hex[:8]
    try:
        scenarios = ("routine", "deadline", "restart") if args.scenario == "all" else (args.scenario,)
        for scenario in scenarios:
            world_name = f"{scenario}-{run_tag}"
            if scenario == "deadline":
                past = utc_iso(utc_now() - timedelta(seconds=2))
                runs.append(run_scenario(args, server, config, output, scenario, world_name, past))
            elif scenario == "restart":
                runs.append(run_scenario(args, server, config, output, scenario, world_name, phase_suffix="-1"))
                if args.launch:
                    runs.append(run_scenario(args, server, config, output, scenario, world_name, phase_suffix="-2"))
            else:
                runs.append(run_scenario(args, server, config, output, scenario, world_name))
    finally:
        server.stop()
    analysis = analyze(server.state, args.routine_min_seconds)
    gates = evaluate_scenarios(args, runs, analysis) if args.launch else {}
    gate_values = [gate for group in gates.values() for gate in group]
    acceptance = "passed" if gate_values and all(gate.get("ok") for gate in gate_values) else ("failed" if gate_values else "not_run")
    report = {"status": "completed" if args.launch else "prepared_fixture_only", "acceptance": acceptance,
              "config": str(config), "runs": runs, "analysis": analysis, "gates": gates,
              "claims": {"real_wall_clock_observation": True, "simulation_time_is_not_used_for_spacing": True,
                         "external_network_used": False, "real_secret_or_ledger_used": False,
                         "forced_termination_is_not_natural_exit": True,
                         "restart_persistence_requires_natural_exit": True}}
    report_path = args.report or output / "thinking-http-report.json"
    report_path.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json_line({"status": report["status"], "acceptance": acceptance, "config": str(config), "report": str(report_path),
                     "request_count": analysis["request_count"], "received_count": analysis["received_count"],
                     "invalid_count": analysis["invalid_count"], "requests_by_phase": analysis["requests_by_phase"]}))
    return 0 if acceptance in ("passed", "not_run") else 2


if __name__ == "__main__":
    raise SystemExit(main())
