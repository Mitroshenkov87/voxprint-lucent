"""Mock Enlarge server for protocol 1.

The product server is not available yet. This process speaks the same hello,
request, progress, done, error, cancel, capabilities, and CLI messages so the
Lucent client and CI can run without it.
"""

from __future__ import annotations

import argparse
import json
import os
import sys
import time
from pathlib import Path
from typing import Any

import imagecodecs
import numpy as np
from PySide6.QtCore import QCoreApplication, QObject
from PySide6.QtNetwork import QLocalServer, QLocalSocket

from voxprint_lucent.enlarge_protocol import (
    ERROR_CODES,
    PROTOCOL,
    ProtocolError,
    dumps,
    enlarged_destination,
    loads,
    output_format_name,
    place_without_overwrite,
    socket_name,
)
from voxprint_lucent.formats import CodecMissing, OutputFormat, PngRejected, write_raster

MOCK_VERSION = "0.0.0"

# Illustrative registry from the agreed spec. photo/max is not installed.
_MODELS: tuple[dict[str, Any], ...] = (
    {
        "id": "nomos2-realplksr",
        "preset": "photo",
        "quality": "fast",
        "scales": [2, 4],
        "installed": True,
    },
    {
        "id": "nomos2-dat2",
        "preset": "photo",
        "quality": "max",
        "scales": [2, 4],
        "installed": False,
    },
    {
        "id": "realesrgan-anime6b",
        "preset": "art",
        "scales": [4],
        "installed": True,
    },
)


def gpu_block() -> dict[str, Any]:
    """``ok``, ``warn``, or ``none``. ``none`` uses ``no_nvidia`` or ``driver_too_old``."""
    status = os.environ.get("LUCENT_ENLARGE_GPU", "none").strip() or "none"
    if status == "ok":
        return {"name": "mock", "cc": "8.9", "status": "ok", "reason": None}
    if status == "warn":
        return {"name": "mock", "cc": "8.6", "status": "warn", "reason": "below_rtx_40"}
    if status == "driver_too_old":
        return {"name": "mock", "cc": "8.9", "status": "none", "reason": "driver_too_old"}
    return {"name": None, "cc": None, "status": "none", "reason": "no_nvidia"}


def capabilities() -> dict[str, Any]:
    """One capabilities object. It exits 0 with or without a GPU."""
    return {
        "protocol": PROTOCOL,
        "app": "enlarge",
        "version": MOCK_VERSION,
        "gpu": gpu_block(),
        "models": [dict(model) for model in _MODELS],
        "presets": ["photo", "art"],
        "max_scale": 4,
        "input_formats": ["avif", "jxl", "webp", "png", "jpeg", "heic", "tiff"],
        "output_formats": ["avif", "jxl", "webp", "jpeg", "heic"],
        "default_output_format": "avif",
    }


def build_job(
    request: dict[str, Any], *, hold_cancel: bool, dry_run: bool = False
) -> tuple[list[dict[str, Any]], bool]:
    """Messages to send now, and whether the socket should wait for ``cancel``.

    ``dry_run`` skips the GPU refusal and does not write the output file. Input,
    model, scale, and format checks still apply. ``done.output`` is the path
    that would have been written.
    """
    job_id = request.get("id")
    ident = job_id if isinstance(job_id, str) else ""

    def message(body: dict[str, Any]) -> dict[str, Any]:
        return {"id": ident, **body}

    gpu = gpu_block()
    if gpu["status"] == "none" and not dry_run:
        reason = gpu["reason"] or "no_nvidia"
        return [_error(message, "gpu_unsupported", f"No usable GPU ({reason}).", False)], False
    raw_input = request.get("input")
    if not isinstance(raw_input, str) or not raw_input:
        return [_error(message, "bad_input", "enlarge.request needs an input path.", False)], False
    source = Path(raw_input)
    if not source.is_absolute():
        return [_error(message, "bad_input", "input must be an absolute path.", False)], False
    if not source.is_file():
        return [_error(message, "bad_input", f"No such image: {source.name}", False)], False
    if "missing-model" in source.name:
        return [
            message({"type": "accepted", "queue_pos": 0}),
            _error(message, "model_missing", "The enlarge model is not installed.", True),
        ], False
    if "cancel-me" in source.name:
        waiting = [
            message({"type": "accepted", "queue_pos": 0}),
            message({"type": "progress", "pct": 10, "stage": "loading", "eta_s": 1}),
        ]
        if hold_cancel:
            return waiting, True
        return [*waiting, _error(message, "cancelled", "Cancelled.", False)], False
    chosen = _select_model(request)
    if isinstance(chosen, str):
        code, text, retryable = _model_error(chosen)
        return [_error(message, code, text, retryable)], False
    scale = request.get("scale", 2)
    if not isinstance(scale, int) or isinstance(scale, bool) or scale not in chosen["scales"]:
        return [
            _error(message, "bad_input", "scale is not available for this model.", False)
        ], False
    format_name = request.get("format") or "avif"
    if not isinstance(format_name, str):
        return [_error(message, "unsupported_format", "format must be a name.", False)], False
    try:
        fmt = output_format_name(format_name)
    except PngRejected as exc:
        return [_error(message, "unsupported_format", str(exc), False)], False
    except ValueError as exc:
        return [_error(message, "unsupported_format", str(exc), False)], False
    try:
        started = time.perf_counter()
        array = _load_rgb(source)
        dest = _destination(request, source, scale, fmt)
        if dry_run:
            height, width = int(array.shape[0]) * scale, int(array.shape[1]) * scale
            elapsed = max(0, int((time.perf_counter() - started) * 1000))
            return [
                message({"type": "accepted", "queue_pos": 0}),
                message({"type": "progress", "pct": 40, "stage": "loading"}),
                message({"type": "progress", "pct": 90, "stage": "tiles", "eta_s": 0}),
                message(
                    {
                        "type": "done",
                        "output": str(dest),
                        "width": width,
                        "height": height,
                        "format": fmt.value,
                        "ms": elapsed,
                    }
                ),
            ], False
        scaled = np.repeat(np.repeat(array, scale, axis=0), scale, axis=1)
        scaled = np.ascontiguousarray(scaled)
        write_raster(scaled, dest, fmt, lossless=fmt is not OutputFormat.AVIF)
    except PngRejected as exc:
        return [_error(message, "unsupported_format", str(exc), False)], False
    except CodecMissing as exc:
        return [_error(message, "unsupported_format", str(exc), False)], False
    except (OSError, ValueError) as exc:
        return [_error(message, "io_error", str(exc), False)], False
    height, width = int(scaled.shape[0]), int(scaled.shape[1])
    elapsed = max(0, int((time.perf_counter() - started) * 1000))
    return [
        message({"type": "accepted", "queue_pos": 0}),
        message({"type": "progress", "pct": 40, "stage": "loading"}),
        message({"type": "progress", "pct": 90, "stage": "tiles", "eta_s": 0}),
        message(
            {
                "type": "done",
                "output": str(dest),
                "width": width,
                "height": height,
                "format": fmt.value,
                "ms": elapsed,
            }
        ),
    ], False


def serve() -> int:
    """Listen on the protocol socket. ``--serve`` refuses to run without a usable GPU."""
    if gpu_block()["status"] == "none" or os.environ.get("LUCENT_ENLARGE_MOCK_SERVE_FAIL") == "1":
        print("voxprint-enlarge --serve needs a usable GPU.", file=sys.stderr)
        return 1
    name = socket_name()
    app = QCoreApplication.instance() or QCoreApplication([])
    try:
        _prepare_socket(name)
        server = _Server(name)
    except RuntimeError as exc:
        print(str(exc), file=sys.stderr)
        return 1
    _ = server
    return int(app.exec())


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="voxprint-enlarge")
    parser.add_argument("--serve", action="store_true")
    parser.add_argument("--hidden", action="store_true")
    parser.add_argument("--capabilities", action="store_true")
    parser.add_argument("--json", action="store_true")
    parser.add_argument("--json-progress", action="store_true")
    parser.add_argument("--input", default="")
    parser.add_argument("--scale", type=int, default=2)
    parser.add_argument("--preset", default="photo")
    parser.add_argument("--quality", default="fast")
    parser.add_argument("--model", default="")
    parser.add_argument("--format", default="avif")
    parser.add_argument("--output", default="")
    parser.add_argument("--output-dir", default="")
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--open-ui", action="store_true")
    args = parser.parse_args(argv)
    if args.capabilities:
        if not args.json:
            print("usage: voxprint-enlarge --capabilities --json", file=sys.stderr)
            return 2
        json.dump(capabilities(), sys.stdout)
        sys.stdout.write("\n")
        return 0
    if args.serve:
        return serve()
    if not args.json_progress:
        print(
            "usage: voxprint-enlarge --serve --hidden | --capabilities --json | "
            "--json-progress [--dry-run] [--open-ui]",
            file=sys.stderr,
        )
        return 2
    request = {
        "id": "cli",
        "input": args.input,
        "scale": args.scale,
        "preset": args.preset,
        "quality": args.quality,
        "model": args.model or None,
        "format": args.format,
        "output": args.output,
        "output_dir": args.output_dir,
        "open_ui": args.open_ui,
    }
    messages, _held = build_job(request, hold_cancel=False, dry_run=args.dry_run)
    for message in messages:
        sys.stdout.buffer.write(dumps(message))
    sys.stdout.flush()
    failed = any(message.get("type") == "error" for message in messages)
    return 1 if failed else 0


class _Server(QObject):
    def __init__(self, name: str) -> None:
        super().__init__()
        self._server = QLocalServer(self)
        QLocalServer.removeServer(name)
        self._server.setSocketOptions(QLocalServer.SocketOption.UserAccessOption)
        if not self._server.listen(name):
            raise RuntimeError(self._server.errorString())
        _restrict(self._server.fullServerName())
        self._sessions: list[_Session] = []
        self._server.newConnection.connect(self._accept)

    def _accept(self) -> None:
        sock = self._server.nextPendingConnection()
        if sock is None:
            return
        session = _Session(sock, self)
        self._sessions.append(session)
        sock.readyRead.connect(session.on_ready)
        sock.disconnected.connect(lambda session=session: self._drop(session))

    def _drop(self, session: _Session) -> None:
        if session in self._sessions:
            self._sessions.remove(session)
        session.deleteLater()


class _Session(QObject):
    def __init__(self, sock: QLocalSocket, parent: QObject) -> None:
        super().__init__(parent)
        self._sock = sock
        self._buf = bytearray()
        self._held: str | None = None

    def on_ready(self) -> None:
        self._buf.extend(bytes(self._sock.readAll()))
        while b"\n" in self._buf:
            line, _, rest = self._buf.partition(b"\n")
            self._buf = bytearray(rest)
            if not line.strip():
                continue
            self._dispatch(line)

    def _dispatch(self, line: bytes) -> None:
        try:
            message = loads(line)
        except (ProtocolError, json.JSONDecodeError, UnicodeError) as exc:
            self._send(
                {
                    "type": "error",
                    "code": "bad_input",
                    "message": str(exc),
                    "retryable": False,
                }
            )
            return
        kind = message.get("type")
        if kind == "hello":
            self._send(
                {
                    "type": "hello",
                    "server": "enlarge",
                    "version": MOCK_VERSION,
                    "protocols": [PROTOCOL],
                }
            )
            return
        if kind == "cancel":
            job_id = message.get("id")
            if self._held and job_id == self._held:
                self._held = None
                self._send(
                    {
                        "id": job_id,
                        "type": "error",
                        "code": "cancelled",
                        "message": "Cancelled.",
                        "retryable": False,
                    }
                )
                return
            self._send(
                {
                    "id": job_id if isinstance(job_id, str) else "",
                    "type": "error",
                    "code": "internal",
                    "message": "No job is waiting for cancel.",
                    "retryable": False,
                }
            )
            return
        if kind != "enlarge.request":
            self._send(
                {
                    "type": "error",
                    "code": "internal",
                    "message": f"Unknown message {kind}.",
                    "retryable": False,
                }
            )
            return
        messages, hold = build_job(message, hold_cancel=True)
        if hold:
            ident = message.get("id")
            self._held = ident if isinstance(ident, str) else ""
        for item in messages:
            code = item.get("code")
            if item.get("type") == "error" and code not in ERROR_CODES:
                item = {**item, "code": "internal"}
            self._send(item)

    def _send(self, message: dict[str, Any]) -> None:
        self._sock.write(dumps(message))
        self._sock.flush()
        self._sock.waitForBytesWritten(1000)


def _select_model(request: dict[str, Any]) -> dict[str, Any] | str:
    model_id = request.get("model")
    if isinstance(model_id, str) and model_id.strip():
        found = next((model for model in _MODELS if model["id"] == model_id), None)
        if found is None:
            return "unknown_model"
        if not found["installed"]:
            return "model_missing"
        return found
    preset = request.get("preset") or "photo"
    quality = request.get("quality") or "fast"
    if not isinstance(preset, str) or preset not in {"photo", "art"}:
        return "bad_preset"
    if not isinstance(quality, str) or quality not in {"fast", "max"}:
        return "bad_quality"
    if preset == "art":
        found = next(model for model in _MODELS if model["id"] == "realesrgan-anime6b")
    elif quality == "max":
        found = next(model for model in _MODELS if model["id"] == "nomos2-dat2")
    else:
        found = next(model for model in _MODELS if model["id"] == "nomos2-realplksr")
    if not found["installed"]:
        return "model_missing"
    return found


def _model_error(token: str) -> tuple[str, str, bool]:
    if token == "model_missing":
        return "model_missing", "The enlarge model is not installed.", True
    if token == "unknown_model":
        return "bad_input", "Unknown model id.", False
    if token == "bad_quality":
        return "bad_input", "quality must be fast or max.", False
    return "bad_input", "preset must be photo or art.", False


def _error(
    message: Any,
    code: str,
    text: str,
    retryable: bool,
) -> dict[str, Any]:
    return message(
        {"type": "error", "code": code, "message": text, "retryable": retryable}
    )


def _destination(request: dict[str, Any], source: Path, scale: int, fmt: OutputFormat) -> Path:
    raw_output = request.get("output")
    if isinstance(raw_output, str) and raw_output:
        dest = Path(raw_output)
        if dest.suffix.lower() == ".png":
            raise PngRejected("PNG is not an output format")
        return place_without_overwrite(dest, source)
    raw_dir = request.get("output_dir")
    directory = Path(raw_dir) if isinstance(raw_dir, str) and raw_dir else None
    return enlarged_destination(source, scale, fmt, directory)


def _load_rgb(path: Path) -> np.ndarray[Any, np.dtype[np.uint8]]:
    data = path.read_bytes()
    suffix = path.suffix.lower()
    if suffix in {".jpg", ".jpeg", ".jpe"}:
        decoded = imagecodecs.jpeg_decode(data)
    elif suffix == ".png":
        decoded = imagecodecs.png_decode(data)
    elif suffix == ".webp":
        decoded = imagecodecs.webp_decode(data)
    elif suffix == ".avif":
        decoded = imagecodecs.avif_decode(data)
    elif suffix == ".jxl":
        decoded = imagecodecs.jpegxl_decode(data)
    else:
        raise ValueError(f"Unsupported input format: {suffix or path.name}")
    array = np.asarray(decoded)
    if array.ndim == 2:
        array = np.repeat(array[:, :, None], 3, axis=2)
    if array.ndim != 3 or array.shape[2] < 3:
        raise ValueError(f"Unsupported pixel shape {array.shape}")
    rgb = np.ascontiguousarray(array[:, :, :3])
    if rgb.dtype != np.uint8:
        rgb = np.clip(rgb, 0, 255).astype(np.uint8)
    return rgb


def _prepare_socket(name: str) -> None:
    if "/" not in name and "\\" not in name:
        return
    path = Path(name)
    path.parent.mkdir(parents=True, exist_ok=True)
    if sys.platform != "win32":
        os.chmod(path.parent, 0o700)
    if path.exists():
        path.unlink()


def _restrict(full_name: str) -> None:
    if sys.platform == "win32" or not full_name:
        return
    path = Path(full_name)
    if path.exists():
        os.chmod(path, 0o600)


if __name__ == "__main__":
    raise SystemExit(main())
