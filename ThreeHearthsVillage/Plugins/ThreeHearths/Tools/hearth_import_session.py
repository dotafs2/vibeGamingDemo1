"""Engine-independent lifecycle for an optional PIE asset preview."""
import math
import time


class ImportSession:
    def __init__(self, backend, timeout_seconds=60.0, clock=time.monotonic):
        if not math.isfinite(timeout_seconds) or timeout_seconds <= 0:
            raise ValueError("timeout_seconds must be positive and finite")
        self.backend = backend
        self.clock = clock
        self.timeout = timeout_seconds
        self.started = self.previous_tick = clock()
        self.state = "loading"
        self.result = None
        self.report = {
            "editor_only": True, "state": "loading", "outcome": None,
            "ticks_while_loading": 0, "max_tick_gap_seconds": 0.0,
        }

    def begin(self):
        try:
            if not self.backend.is_current():
                self.stop("PIE session is no longer current")
            elif not self.backend.begin(self.on_complete):
                self.fallback("Interchange did not start the import")
        except Exception as exc:
            self.fallback(str(exc))
        return self

    def on_complete(self, objects):
        # A late callback must never replace a fallback or affect another PIE run.
        if self.state == "loading" and self.clock() - self.started < self.timeout:
            self.result = list(objects)

    def tick(self):
        if self.state == "stopped":
            return False
        try:
            if not self.backend.is_current():
                self.stop("PIE ended or the village restarted")
                return False
            if self.state == "loading":
                now = self.clock()
                self.report["ticks_while_loading"] += 1
                self.report["max_tick_gap_seconds"] = max(
                    self.report["max_tick_gap_seconds"], now - self.previous_tick)
                self.previous_tick = now
                if now - self.started >= self.timeout:
                    self.fallback("Import timed out; original model retained")
                elif self.result is not None:
                    details = self.backend.apply(self.result)
                    self.result = None
                    self.report.update(details)
                    self.finish("ready", "Imported model displayed")
        except Exception as exc:
            self.fallback(str(exc))
        return True

    def finish(self, outcome, reason):
        self.state = outcome
        self.report.update(state=outcome, outcome=outcome, reason=reason,
                           import_wall_seconds=self.clock() - self.started)
        self.backend.write_report(dict(self.report))

    def fallback(self, reason):
        self.result = None
        try:
            self.backend.restore()
        except Exception as exc:
            self.finish("error", f"{reason}; could not restore preview: {exc}")
            return
        self.finish("fallback", reason)

    def stop(self, reason="Stopped by operator"):
        if self.state == "stopped":
            return
        self.state = "stopped"
        self.result = None
        try:
            self.backend.restore()
        except Exception as exc:
            self.report["cleanup_error"] = str(exc)
        self.report.update(state="stopped", cleanup_reason=reason)
        self.backend.write_report(dict(self.report))
