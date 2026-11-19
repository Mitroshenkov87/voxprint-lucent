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
ENLARGE_RELEASES_URL = "https://github.com/Mitroshenkov87/voxprint-enlarge/releases"

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


# Exit status of the headless CLI. ``done`` is 0.
EXIT_FOR_CODE = {
    "internal": 1,
    "bad_input": 2,
    "unsupported_format": 3,
    "gpu_unsupported": 4,
    "gpu_busy": 5,
    "oom": 6,
    "model_missing": 7,
    "io_error": 8,
    "cancelled": 130,
}


def exit_for_code(code: str) -> int:
    """Process status for an Enlarge error code. Unknown codes are internal."""
    return EXIT_FOR_CODE.get(code, 1)


def dumps(message: dict[str, Any]) -> bytes:
    """One UTF-8 JSON line. ``protocol`` is 1. ``op`` repeats ``type``."""
    body = dict(message)
    body["protocol"] = PROTOCOL
    if "type" in body:
        body.setdefault("op", body["type"])
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
    """First free path. A collision becomes ``name-2.ext``, then ``-3``."""
    number = 0
    while True:
        candidate = path if number == 0 else path.with_name(f"{path.stem}-{number}{path.suffix}")
        number = 2 if number == 0 else number + 1
        if candidate.exists() or candidate.resolve() == source.resolve():
            continue
        return candidate


def enlarged_destination(
    source: Path,
    scale: int,
    fmt: OutputFormat | None = None,
    directory: Path | None = None,
) -> Path:
    """``<name>.enlarged-<scale>x.<ext>`` beside the source. A collision starts at ``-2``."""
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
    """GPU status reported by Enlarge."""
    status: str
    reason: str | None


@dataclass(frozen=True)
class Capabilities:
    """What the installed Enlarge build can do."""
    version: str
    gpu: GpuInfo
    formats: tuple[str, ...]
    raw: dict[str, Any]


def dedupe_models(data: dict[str, Any]) -> dict[str, Any]:
    """Drop later ``models`` entries that repeat an id. The first one stays.

    Enlarge can list the same id twice. Lucent shows the list it was given,
    so the duplicate is removed here before anything displays ``raw``.
    """
    models = data.get("models")
    if not isinstance(models, list):
        return data
    seen: set[str] = set()
    kept: list[Any] = []
    for item in models:
        if isinstance(item, dict):
            model_id = item.get("id")
            if isinstance(model_id, str) and model_id:
                if model_id in seen:
                    continue
                seen.add(model_id)
        kept.append(item)
    if len(kept) == len(models):
        return data
    cleaned = dict(data)
    cleaned["models"] = kept
    return cleaned


def parse_capabilities(data: dict[str, Any]) -> Capabilities:
    """Build a Capabilities value from an Enlarge JSON object."""
    data = dedupe_models(data)
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


def action_enabled(caps: Capabilities | None) -> bool:
    """Enable Enlarge when it is installed and reports a usable GPU."""
    if caps is None:
        return False
    return caps.gpu.status != "none"


def action_visible(caps: Capabilities | None) -> bool:
    """Same rule as ``action_enabled``. The button stays on screen either way."""
    return action_enabled(caps)


def install_offered(caps: Capabilities | None) -> bool:
    """An Install Enlarge button is offered only when the program is absent."""
    return caps is None


def action_tooltip(caps: Capabilities | None) -> str:
    """Return the tooltip for the Enlarge action."""
    if caps is None:
        return "Enlarge is not installed."
    if caps.gpu.status == "none":
        return caps.gpu.reason or "no_nvidia"
    if caps.gpu.status == "warn":
        return caps.gpu.reason or "warn"
    return "Enlarge"

