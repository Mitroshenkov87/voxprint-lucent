"""Lucent's Enlarge client: capability probe, socket hand-off, and CLI fallback.

The real Enlarge server is not running yet. Point ``LUCENT_ENLARGE_EXE`` at
``python -m voxprint_lucent.enlarge_mock`` to exercise this client.
"""

from __future__ import annotations

import json
import os
import shlex
import shutil
import subprocess
import sys
import time
import uuid
from dataclasses import dataclass
from pathlib import Path
from typing import Any

from PySide6.QtNetwork import QLocalSocket

from voxprint_lucent import __version__
from voxprint_lucent.enlarge_protocol import (
    Capabilities,
    ProtocolError,
    action_tooltip,
    action_visible,
    dumps,
    enlarged_destination,
    loads,
    output_format_name,
    parse_capabilities,
    socket_name,
)
from voxprint_lucent.formats import PngRejected

_SOCKET_WAIT_S = 3.0
_MESSAGE_WAIT_S = 5.0


class EnlargeError(RuntimeError):
    def __init__(self, code: str, message: str, retryable: bool) -> None:
        super().__init__(message)
        self.code = code
        self.retryable = retryable


@dataclass(frozen=True)
class EnlargeResult:
    output: Path
    width: int
    height: int
    format: str
    milliseconds: int


class EnlargeClient:
    def __init__(
        self,
        command: list[str] | None = None,
        socket: str | None = None,
    ) -> None:
        self.command = discover_command() if command is None else command
        self.socket = socket_name() if socket is None else socket
        self._spawned: subprocess.Popen[bytes] | None = None
        self._cli: subprocess.Popen[str] | None = None
        self._sock: QLocalSocket | None = None
        self._job_id: str | None = None
        self._buf = bytearray()

    def probe(self) -> Capabilities | None:
        """``--capabilities --json``. None when Enlarge is absent or the probe fails."""
        if not self.command:
            return None
        try:
            proc = subprocess.run(
                [*self.command, "--capabilities", "--json"],
                capture_output=True,
                text=True,
                timeout=10,
                check=False,
            )
        except (OSError, subprocess.TimeoutExpired):
            return None
        if proc.returncode != 0 or not proc.stdout.strip():
            return None
        try:
            data = json.loads(proc.stdout)
        except json.JSONDecodeError:
            return None
        if not isinstance(data, dict):
            return None
        return parse_capabilities(data)

    def visible(self) -> bool:
        return action_visible(self.probe())

    def tooltip(self) -> str:
        return action_tooltip(self.probe())

    def enlarge(
        self,
        source: Path,
        *,
        scale: int = 2,
        preset: str = "photo",
        format_name: str = "avif",
        output: Path | None = None,
        output_dir: Path | None = None,
        open_ui: bool = False,
        on_event: Any = None,
        transport: str = "auto",
    ) -> EnlargeResult:
        """Quick enlarge (``open_ui`` false) or Open in Enlarge (``open_ui`` true)."""
        if transport not in {"auto", "socket", "cli"}:
            raise ValueError(f"Unknown transport: {transport}")
        self._reject_png(format_name)
        if transport == "cli":
            return self._enlarge_cli(
                source,
                scale=scale,
                preset=preset,
                format_name=format_name,
                output=output,
                on_event=on_event,
            )
        try:
            return self._enlarge_socket(
                source,
                scale=scale,
                preset=preset,
                format_name=format_name,
                output=output,
                output_dir=output_dir,
                open_ui=open_ui,
                on_event=on_event,
            )
        except EnlargeError as exc:
            if transport == "socket" or exc.code != "io_error" or not self.command:
                raise
            return self._enlarge_cli(
                source,
                scale=scale,
                preset=preset,
                format_name=format_name,
                output=output,
                on_event=on_event,
            )

    def cancel(self) -> None:
        """Ask the server to cancel the current job, or stop a CLI fallback."""
        if self._sock is not None and self._job_id:
            self._write(self._sock, {"type": "cancel", "id": self._job_id})
        if self._cli is not None and self._cli.poll() is None:
            self._cli.terminate()

    def shutdown(self) -> None:
        """Drop the socket and stop a server this client started."""
        if self._sock is not None:
            self._sock.disconnectFromServer()
            self._sock = None
        if self._cli is not None and self._cli.poll() is None:
            self._cli.terminate()
            self._cli = None
        spawned = self._spawned
        self._spawned = None
        if spawned is not None and spawned.poll() is None:
            spawned.terminate()
            try:
                spawned.wait(timeout=2)
            except subprocess.TimeoutExpired:
                spawned.kill()

    def _enlarge_socket(
        self,
        source: Path,
        *,
        scale: int,
        preset: str,
        format_name: str,
        output: Path | None,
        output_dir: Path | None,
        open_ui: bool,
        on_event: Any,
    ) -> EnlargeResult:
        sock = self._connect()
        self._sock = sock
        self._buf = bytearray()
        self._job_id = uuid.uuid4().hex
        self._write(
            sock,
            {"type": "hello", "client": "voxprint-lucent", "version": __version__},
        )
        hello = self._read(sock)
        self._emit(on_event, hello)
        if hello.get("type") != "hello":
            raise EnlargeError("internal", "Enlarge did not answer hello.", True)
        request = {
            "type": "enlarge.request",
            "id": self._job_id,
            "input": str(source),
            "scale": scale,
            "preset": preset,
            "format": format_name,
            "output": "" if output is None else str(output),
            "output_dir": "" if output_dir is None else str(output_dir),
            "open_ui": open_ui,
        }
        self._write(sock, request)
        while True:
            message = self._read(sock)
            self._emit(on_event, message)
            kind = message.get("type")
            if kind == "done":
                return _result_from(message)
            if kind == "error":
                raise _error_from(message)
            if kind not in {"accepted", "progress"}:
                raise EnlargeError("internal", f"Unexpected Enlarge message {kind}.", True)

    def _enlarge_cli(
        self,
        source: Path,
        *,
        scale: int,
        preset: str,
        format_name: str,
        output: Path | None,
        on_event: Any,
    ) -> EnlargeResult:
        if not self.command:
            raise EnlargeError("io_error", "Enlarge is not installed.", False)
        chosen = output_format_name(format_name)
        dest = output or enlarged_destination(source, scale, chosen)
        command = [
            *self.command,
            "--input",
            str(source),
            "--scale",
            str(scale),
            "--preset",
            preset,
            "--format",
            chosen.value,
            "--output",
            str(dest),
            "--json-progress",
        ]
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
                failure = EnlargeError("internal", str(exc), True)
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
            raise EnlargeError("internal", f"Enlarge CLI exited {code}.", True)
        return result

    def _connect(self) -> QLocalSocket:
        live = _try_connect(self.socket, 300)
        if live is not None:
            return live
        if not self.command:
            raise EnlargeError("io_error", "Enlarge is not installed.", False)
        if self._spawned is None or self._spawned.poll() is not None:
            try:
                self._spawned = subprocess.Popen([*self.command, "--serve", "--hidden"])
            except OSError as exc:
                raise EnlargeError("io_error", str(exc), True) from exc
        deadline = time.monotonic() + _SOCKET_WAIT_S
        while time.monotonic() < deadline:
            live = _try_connect(self.socket, 200)
            if live is not None:
                return live
            if self._spawned.poll() is not None:
                break
        raise EnlargeError("io_error", "Enlarge did not open a socket within 3 s.", True)

    def _read(self, sock: QLocalSocket) -> dict[str, Any]:
        deadline = time.monotonic() + _MESSAGE_WAIT_S
        while time.monotonic() < deadline:
            if b"\n" in self._buf:
                line, _, rest = self._buf.partition(b"\n")
                self._buf = bytearray(rest)
                if not line.strip():
                    continue
                try:
                    return loads(line)
                except (ProtocolError, json.JSONDecodeError) as exc:
                    raise EnlargeError("internal", str(exc), True) from exc
            remaining = int((deadline - time.monotonic()) * 1000)
            if remaining <= 0:
                break
            if not sock.waitForReadyRead(min(remaining, 200)):
                continue
            self._buf.extend(bytes(sock.readAll()))
        raise EnlargeError("io_error", "Timed out waiting for Enlarge.", True)

    def _write(self, sock: QLocalSocket, message: dict[str, Any]) -> None:
        payload = dumps(message)
        written = sock.write(payload)
        if written < 0 or not sock.waitForBytesWritten(1000):
            raise EnlargeError("io_error", "Could not write to Enlarge.", True)

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


def discover_command() -> list[str] | None:
    """Env override, then PATH, then the desktop file or the HKCU registration."""
    raw = os.environ.get("LUCENT_ENLARGE_EXE", "").strip()
    if raw:
        return shlex.split(raw)
    found = shutil.which("voxprint-enlarge")
    if found:
        return [found]
    if sys.platform == "win32":
        return _command_from_registry()
    return _command_from_desktop_dirs()


def command_from_desktop(text: str) -> list[str] | None:
    """Exec line from an ``ai.voxprint.Enlarge.desktop`` file at protocol 1."""
    protocol = ""
    exec_line = ""
    for line in text.splitlines():
        if line.startswith("X-Voxprint-Protocol="):
            protocol = line.split("=", 1)[1].strip()
        elif line.startswith("Exec="):
            exec_line = line.split("=", 1)[1].strip()
    if protocol != "1" or not exec_line:
        return None
    return [part for part in shlex.split(exec_line) if not part.startswith("%")]


def _command_from_desktop_dirs() -> list[str] | None:
    roots: list[Path] = []
    data_home = os.environ.get("XDG_DATA_HOME", "").strip()
    roots.append(Path(data_home) if data_home else Path.home() / ".local" / "share")
    data_dirs = os.environ.get("XDG_DATA_DIRS", "/usr/local/share:/usr/share")
    roots.extend(Path(item) for item in data_dirs.split(":") if item)
    for root in roots:
        path = root / "applications" / "ai.voxprint.Enlarge.desktop"
        if not path.is_file():
            continue
        command = command_from_desktop(path.read_text(encoding="utf-8"))
        if command:
            return command
    return None


def _command_from_registry() -> list[str] | None:
    # winreg is Windows-only. A top-level import fails on Linux.
    import winreg

    try:
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER, r"Software\Voxprint\Enlarge") as key:
            exe, _ = winreg.QueryValueEx(key, "Exe")
            protocol, _ = winreg.QueryValueEx(key, "ProtocolVersion")
    except OSError:
        return None
    if str(protocol) != "1" or not exe:
        return None
    return [str(exe)]


def _try_connect(name: str, timeout_ms: int) -> QLocalSocket | None:
    sock = QLocalSocket()
    sock.connectToServer(name)
    if sock.waitForConnected(timeout_ms):
        return sock
    sock.abort()
    return None


def _result_from(message: dict[str, Any]) -> EnlargeResult:
    output = message.get("output")
    if not isinstance(output, str) or not output:
        raise EnlargeError("internal", "Enlarge done message has no output.", True)
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

