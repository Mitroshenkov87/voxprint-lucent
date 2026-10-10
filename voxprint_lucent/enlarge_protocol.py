"""Enlarge protocol 1 messages, discovery names, and the show/hide rule.

This is the client contract from PLAN-v1 section 4, plus ``model_missing`` and a
capabilities ``gpu`` object. The shared ``suite/IPC-ENLARGE.md`` file was not
readable from this checkout (the project-notes repository returned 404).
"""

from __future__ import annotations

import hashlib
import json
import os
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any

from voxprint_lucent.formats import (
    OutputFormat,
    extension_for,
    parse_output_format,
    unique_path,
)

PROTOCOL = 1

ERROR_CODES = frozenset(
    {
        "gpu_unsupported",
        "gpu_busy",
        "oom",
        "bad_input",
        "unsupported_format",
        "io_error",
        "cancelled",
        "internal",
        "model_missing",
    }
)


class ProtocolError(ValueError):
    """A line was not a protocol 1 object."""


def dumps(message: dict[str, Any]) -> bytes:
    """One UTF-8 JSON line. ``protocol`` is always 1, even if the caller set it."""
    body = dict(message)
    body["protocol"] = PROTOCOL
    return (json.dumps(body, separators=(",", ":")) + "\n").encode("utf-8")


def loads(line: bytes | str) -> dict[str, Any]:
    """Parse one JSON line. Unknown fields stay in the dict for the caller to ignore."""
    text = line.decode("utf-8") if isinstance(line, bytes) else line
    data = json.loads(text)
    if not isinstance(data, dict):
        raise ProtocolError("Enlarge message must be a JSON object")
    if data.get("protocol") != PROTOCOL:
        raise ProtocolError(f"Enlarge protocol {data.get('protocol')} is not {PROTOCOL}")
    return data


def pipe_name_for_sid(sid: str) -> str:
    """Qt server name. Windows exposes it as ``\\\\.\\pipe\\<name>``."""
    digest = hashlib.sha256(sid.encode("utf-8")).hexdigest()[:16]
    return f"voxprint-enlarge-{digest}"


def default_socket_name() -> str:
    """The socket Enlarge listens on when nothing has registered another name."""
    if sys.platform == "win32":
        return pipe_name_for_sid(_windows_user_sid())
    runtime = os.environ.get("XDG_RUNTIME_DIR") or f"/tmp/voxprint-{os.getuid()}"
    return str(Path(runtime) / "voxprint" / "enlarge.sock")


def socket_name() -> str:
    """Test and launch override, otherwise the standard socket."""
    override = os.environ.get("LUCENT_ENLARGE_SOCKET", "").strip()
    if override:
        return override
    return default_socket_name()


def enlarged_destination(
    source: Path,
    scale: int,
    fmt: OutputFormat | None = None,
    directory: Path | None = None,
) -> Path:
    """``<name>.enlarged-<scale>x.avif`` beside the source. Never the source, never PNG."""
    chosen = OutputFormat.AVIF if fmt is None else fmt
    folder = source.parent if directory is None else directory
    ext = extension_for(chosen)
    candidate = folder / f"{source.stem}.enlarged-{scale}x{ext}"
    if candidate.resolve() == source.resolve():
        candidate = folder / f"{source.stem}.enlarged-{scale}x.copy{ext}"
    return unique_path(candidate)


def output_format_name(name: str) -> OutputFormat:
    """AVIF when the request leaves the format empty. PNG is rejected by the parser."""
    if not name.strip():
        return OutputFormat.AVIF
    return parse_output_format(name)


@dataclass(frozen=True)
class GpuInfo:
    status: str
    reason: str


@dataclass(frozen=True)
class Capabilities:
    version: str
    gpu: GpuInfo
    formats: tuple[str, ...]
    raw: dict[str, Any]


def parse_capabilities(data: dict[str, Any]) -> Capabilities:
    gpu = data.get("gpu")
    status = "none"
    reason = "No usable GPU is available."
    if isinstance(gpu, dict):
        raw_status = gpu.get("status")
        if isinstance(raw_status, str) and raw_status:
            status = raw_status
        raw_reason = gpu.get("reason")
        if isinstance(raw_reason, str):
            reason = raw_reason
    formats = data.get("formats")
    if isinstance(formats, list):
        names = tuple(item for item in formats if isinstance(item, str))
    else:
        names = ()
    version = data.get("version")
    return Capabilities(
        version if isinstance(version, str) else "",
        GpuInfo(status, reason),
        names,
        data,
    )


def action_visible(caps: Capabilities | None) -> bool:
    """Hide Enlarge when the program is absent or it reports no usable GPU."""
    if caps is None:
        return False
    return caps.gpu.status != "none"


def action_tooltip(caps: Capabilities | None) -> str:
    if caps is None:
        return "Enlarge is not installed."
    if caps.gpu.status == "none":
        return caps.gpu.reason or "No usable GPU is available."
    if caps.gpu.status == "warn":
        return caps.gpu.reason
    return "Enlarge"


def _windows_user_sid() -> str:
    # ctypes.wintypes is Windows-only. A top-level import fails on Linux.
    import ctypes
    from ctypes import wintypes

    advapi32 = ctypes.WinDLL("advapi32", use_last_error=True)
    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    advapi32.ConvertSidToStringSidW.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_wchar_p)]
    advapi32.ConvertSidToStringSidW.restype = wintypes.BOOL
    kernel32.LocalFree.argtypes = [ctypes.c_void_p]
    kernel32.LocalFree.restype = ctypes.c_void_p

    class _SidAndAttributes(ctypes.Structure):
        _fields_ = [("Sid", ctypes.c_void_p), ("Attributes", wintypes.DWORD)]

    class _TokenUser(ctypes.Structure):
        _fields_ = [("User", _SidAndAttributes)]

    token = wintypes.HANDLE()
    if not advapi32.OpenProcessToken(kernel32.GetCurrentProcess(), 0x0008, ctypes.byref(token)):
        return str(os.getlogin())
    try:
        length = wintypes.DWORD()
        advapi32.GetTokenInformation(token, 1, None, 0, ctypes.byref(length))
        buf = ctypes.create_string_buffer(length.value)
        if not advapi32.GetTokenInformation(token, 1, buf, length, ctypes.byref(length)):
            return str(os.getlogin())
        user = ctypes.cast(buf, ctypes.POINTER(_TokenUser)).contents
        sid = ctypes.c_wchar_p()
        if not advapi32.ConvertSidToStringSidW(user.User.Sid, ctypes.byref(sid)):
            return str(os.getlogin())
        try:
            return sid.value or str(os.getlogin())
        finally:
            kernel32.LocalFree(sid)
    finally:
        kernel32.CloseHandle(token)
