"""Enlarge protocol 1 messages and the Lucent show/hide rule.

The agreed text is docs/IPC-ENLARGE.md. Discovery and socket names follow
docs/APP-DISCOVERY.md.
"""

from __future__ import annotations

import json
import os
from dataclasses import dataclass
from pathlib import Path
from typing import Any

from voxprint_lucent.discovery import pipe_name, socket_path
from voxprint_lucent.formats import OutputFormat, parse_output_format

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


_OUTPUT_EXT = {
    OutputFormat.AVIF: ".avif",
    OutputFormat.JXL: ".jxl",
    OutputFormat.WEBP: ".webp",
    OutputFormat.JPEG: ".jpg",
    OutputFormat.HEIC: ".heic",
}


def pipe_name_for_sid(sid: str) -> str:
    """Qt server name ``voxprint-enlarge-<sid hash>``. Windows adds ``\\\\.\\pipe\\``."""
    return pipe_name("voxprint-enlarge", sid)


def socket_name() -> str:
    """``LUCENT_ENLARGE_SOCKET``, otherwise the suite socket for Enlarge."""
    override = os.environ.get("LUCENT_ENLARGE_SOCKET", "").strip()
    if override:
        return override
    return socket_path("enlarge", "voxprint-enlarge")


def place_without_overwrite(path: Path, source: Path) -> Path:
    """First free path. A collision becomes ``name-1.ext``, then ``-2``, and so on."""
    number = 0
    while True:
        if number == 0:
            candidate = path
        else:
            candidate = path.with_name(f"{path.stem}-{number}{path.suffix}")
        number += 1
        if candidate.exists():
            continue
        if candidate.resolve() == source.resolve():
            continue
        return candidate


def enlarged_destination(
    source: Path,
    scale: int,
    fmt: OutputFormat | None = None,
    directory: Path | None = None,
) -> Path:
    """``<name>.enlarged-<scale>x.avif`` beside the source. Never the source, never PNG."""
    chosen = OutputFormat.AVIF if fmt is None else fmt
    folder = source.parent if directory is None else directory
    ext = _OUTPUT_EXT[chosen]
    candidate = folder / f"{source.stem}.enlarged-{scale}x{ext}"
    return place_without_overwrite(candidate, source)


def output_format_name(name: str) -> OutputFormat:
    """AVIF when the request leaves the format empty. PNG is rejected by the parser."""
    if not name.strip():
        return OutputFormat.AVIF
    return parse_output_format(name)


@dataclass(frozen=True)
class GpuInfo:
    status: str
    reason: str | None


@dataclass(frozen=True)
class Capabilities:
    version: str
    gpu: GpuInfo
    formats: tuple[str, ...]
    raw: dict[str, Any]


def parse_capabilities(data: dict[str, Any]) -> Capabilities:
    gpu = data.get("gpu")
    status = "none"
    reason: str | None = "no_nvidia"
    if isinstance(gpu, dict):
        raw_status = gpu.get("status")
        if isinstance(raw_status, str) and raw_status:
            status = raw_status
        if "reason" in gpu:
            raw_reason = gpu.get("reason")
            reason = raw_reason if isinstance(raw_reason, str) else None
        elif status == "ok":
            reason = None
    formats = data.get("output_formats")
    if not isinstance(formats, list):
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
        return caps.gpu.reason or "no_nvidia"
    if caps.gpu.status == "warn":
        return caps.gpu.reason or "warn"
    return "Enlarge"

