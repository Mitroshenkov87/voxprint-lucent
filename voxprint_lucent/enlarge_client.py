"""Lucent's Enlarge client: discovery, capabilities, and the headless CLI.

Lucent never opens Enlarge's socket. It runs ``voxprint-enlarge`` and reads
JSON lines. Point ``LUCENT_ENLARGE_EXE`` at
``python -m voxprint_lucent.enlarge_mock`` to exercise this client.
"""

from __future__ import annotations

import json
import os
import shlex
import shutil
import subprocess
from dataclasses import dataclass
from pathlib import Path
from typing import Any

from voxprint_lucent.discovery import AppInstall, find_app, probe_capabilities
from voxprint_lucent.enlarge_protocol import (
    ENLARGE_RELEASES_URL,
    PROTOCOL,
    Capabilities,
    ProtocolError,
    action_enabled,
    action_tooltip,
    install_offered,
    loads,
    output_format_name,
    parse_capabilities,
)
from voxprint_lucent.formats import PngRejected


class EnlargeError(RuntimeError):
    """Enlarge refused the job or the hand-off failed."""
    def __init__(self, code: str, message: str, retryable: bool) -> None:
        """Store the machine-readable code, message, and whether a retry makes sense."""
        super().__init__(message)
        self.code = code
        self.retryable = retryable


@dataclass(frozen=True)
class EnlargeResult:
    """A finished Enlarge job: output path, size, format, and timing."""
    output: Path
    width: int
    height: int
    format: str
    milliseconds: int


class EnlargeClient:
    """Talks to an installed Enlarge program on its command line."""
    def __init__(self, command: list[str] | None = None) -> None:
        """Use `command` when discovery should not choose the executable."""
        if command is None:
            self.install = discover_install()
        else:
            self.install = AppInstall(list(command), "", "voxprint-enlarge")
        self.command = list(self.install.command) if self.install is not None else []
        self._cli: subprocess.Popen[str] | None = None

    def probe(self) -> Capabilities | None:
        """Cached ``--capabilities --json``. None when Enlarge is absent or the probe fails."""
        if not self.command:
            return None
        data = probe_capabilities(self.install, "enlarge") if self.install is not None else None
        if data is None:
            return None
        return parse_capabilities(data)

    def enabled(self) -> bool:
        """Return whether Enlarge can accept a job on this machine."""
        return action_enabled(self.probe())

    def install_offered(self) -> bool:
        """Return whether the UI should offer to install Enlarge."""
        return install_offered(self.probe())

    def releases_url(self) -> str:
        """Return the page where the user can download Enlarge."""
        return ENLARGE_RELEASES_URL

    def tooltip(self) -> str:
        """Return the Enlarge action's tooltip for the current machine."""
        return action_tooltip(self.probe())

    def enlarge(
        self,
        source: Path,
        *,
        scale: int | None = 2,
        long_side: int | None = None,
        preset: str = "photo",
        quality: int | None = None,
        quality_tier: str = "fast",
        model: str | None = None,
        format_name: str = "avif",
        output: Path | None = None,
        lossless: bool = False,
        lock_timeout: float | None = None,
        parent_lock_pid: int | None = None,
        dry_run: bool = False,
        on_event: Any = None,
    ) -> EnlargeResult:
        """Hand the file to ``voxprint-enlarge`` and read its JSON lines.

        The process never opens a window. ``--dry-run`` plans the file and
        does not write it. GPU, format, and missing-model failures still apply.
        """
        self._reject_png(format_name)
        if not self.command:
            raise EnlargeError("io_error", "Enlarge is not installed.", False)
        chosen = output_format_name(format_name)
        command = [
            *self.command,
            "--input",
            str(source.resolve()),
            "--format",
            chosen.value,
            "--json-progress",
        ]
        if long_side is not None:
            command.extend(["--long-side", str(long_side)])
        elif scale is not None:
            command.extend(["--scale", str(scale)])
        if model:
            command.extend(["--model", model])
        else:
            command.extend(["--preset", preset, "--quality-tier", quality_tier])
        if quality is not None:
            command.extend(["--quality", str(quality)])
        if output is not None:
            command.extend(["--output", str(output)])
        if lossless:
            command.append("--lossless")
        if lock_timeout is not None:
            command.extend(["--lock-timeout", str(lock_timeout)])
        if parent_lock_pid is not None:
            command.extend(["--parent-lock-pid", str(parent_lock_pid)])
        if dry_run:
            command.append("--dry-run")
        proc = subprocess.Popen(command, stdout=subprocess.PIPE, text=True)
        self._cli = proc
        assert proc.stdout is not None
        result: EnlargeResult | None = None
        failure: EnlargeError | None = None
        for line in proc.stdout:
            if not line.strip():
                continue
            try:
                message = loads(line)
            except (ProtocolError, json.JSONDecodeError) as exc:
                failure = EnlargeError("internal", str(exc), False)
                break
            self._emit(on_event, message)
            kind = message.get("type")
            if kind == "done":
                result = _result_from(message)
            elif kind == "error":
                failure = _error_from(message)
        code = proc.wait()
        self._cli = None
        if failure is not None:
            raise failure
        if result is None or code != 0:
            raise EnlargeError("internal", f"Enlarge CLI exited {code}.", False)
        return result

    def cancel(self) -> None:
        """Stop the running CLI hand-off."""
        if self._cli is not None and self._cli.poll() is None:
            self._cli.terminate()

    def shutdown(self) -> None:
        """Stop a local Enlarge process this client started."""
        self.cancel()
        self._cli = None

    @staticmethod
    def _emit(on_event: Any, message: dict[str, Any]) -> None:
        if on_event is not None:
            on_event(message)

    @staticmethod
    def _reject_png(format_name: str) -> None:
        try:
            output_format_name(format_name)
        except PngRejected:
            raise
        except ValueError as exc:
            raise EnlargeError("unsupported_format", str(exc), False) from exc


def discover_install() -> AppInstall | None:
    """``LUCENT_ENLARGE_EXE`` is a test hook. Installed apps follow APP-DISCOVERY.md.

    The executable must exist. Flatpak is not probed.
    """
    raw = os.environ.get("LUCENT_ENLARGE_EXE", "").strip()
    if raw:
        parts = shlex.split(raw)
        if not parts:
            return None
        head = parts[0]
        if not Path(head).is_file() and shutil.which(head) is None:
            return None
        version = os.environ.get("LUCENT_ENLARGE_VERSION", "").strip()
        return AppInstall(parts, version, "voxprint-enlarge")
    return find_app("Enlarge", "enlarge")


def discover_command() -> list[str] | None:
    """Return the argv of an installed Enlarge, or `None`."""
    install = discover_install()
    if install is None:
        return None
    return list(install.command)


def hello_supported(message: dict[str, Any]) -> bool:
    """Server hello must name Enlarge and list protocol 1."""
    protocols = message.get("protocols")
    return (
        message.get("type") == "hello"
        and message.get("server") == "enlarge"
        and isinstance(protocols, list)
        and PROTOCOL in protocols
    )


def _result_from(message: dict[str, Any]) -> EnlargeResult:
    output = message.get("output")
    if not isinstance(output, str) or not output:
        raise EnlargeError("internal", "Enlarge done message has no output.", False)
    return EnlargeResult(
        Path(output),
        int(message.get("width", 0)),
        int(message.get("height", 0)),
        str(message.get("format", "")),
        int(message.get("ms", 0)),
    )


def _error_from(message: dict[str, Any]) -> EnlargeError:
    code = message.get("code")
    text = message.get("message")
    return EnlargeError(
        code if isinstance(code, str) else "internal",
        text if isinstance(text, str) else "Enlarge failed.",
        bool(message.get("retryable", False)),
    )
