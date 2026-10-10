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
    exit_for_code,
    loads,
    output_format_name,
    place_without_overwrite,
    socket_name,
)
from voxprint_lucent.formats import CodecMissing, OutputFormat, PngRejected, write_raster

MOCK_VERSION = "0.0.0"

# Stand-in registry. photo/max is not on disk, so that tier returns model_missing.
_MODELS: tuple[dict[str, Any], ...] = (
    {
        "id": "4x-nomos2-realplksr-dysample",
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
        "quality": "fast",
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
        return {"name": "mock", "cc": "8.6", "status": "warn", "reason": None}
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
        "input_formats": ["avif", "bmp", "jpeg", "jxl", "png", "tiff", "webp"],
        "output_formats": ["avif", "jxl", "webp", "jpeg"],
        "default_output_format": "avif",
    }


def build_job(
    request: dict[str, Any], *, hold_cancel: bool, dry_run: bool = False
) -> tuple[list[dict[str, Any]], bool]:
    """Messages to send now, and whether the socket should wait for ``cancel``.

    A missing GPU is ``gpu_unsupported`` even when ``dry_run`` is set.
    ``dry_run`` then returns ``accepted`` and ``done`` with ``ms`` 0 and does
    not write the file. ``done.output`` is the path a real run would use.
    """
    job_id = request.get("id")
    ident = job_id if isinstance(job_id, str) else ""

    def message(body: dict[str, Any]) -> dict[str, Any]:
        return {"id": ident, **body}

    gpu = gpu_block()
    if gpu["status"] == "none":
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
        scale, size_error = _resolve_scale(request, array_shape=array.shape)
        if size_error is not None:
            return [_error(message, "bad_input", size_error, False)], False
        if scale not in chosen["scales"]:
            return [
                _error(message, "bad_input", "scale is not available for this model.", False)
            ], False
        if request.get("lossless") and fmt is not OutputFormat.JXL:
            return [_error(message, "bad_input", "--lossless is JPEG XL only.", False)], False
        width, height = _planned_size(array.shape, scale, request.get("long_side"))
        dest = _destination(request, source, scale, fmt)
        if dry_run:
            return [
                message({"type": "accepted", "queue_pos": 0}),
                message(
                    {
                        "type": "done",
                        "output": str(dest),
                        "width": width,
                        "height": height,
                        "format": fmt.value,
                        "ms": 0,
                    }
                ),
            ], False
        scaled = np.repeat(np.repeat(array, scale, axis=0), scale, axis=1)
        scaled = _fit_long_side(np.ascontiguousarray(scaled), width, height)
        write_raster(scaled, dest, fmt, lossless=bool(request.get("lossless")))
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


def _emit(messages: list[dict[str, Any]]) -> int:
    for message in messages:
        sys.stdout.buffer.write(dumps(message))
    sys.stdout.flush()
    failed = next((message for message in messages if message.get("type") == "error"), None)
    if failed is None:
        return 0
    code = failed.get("code")
    return exit_for_code(code if isinstance(code, str) else "internal")


def main(argv: list[str] | None = None) -> int:
    """Run the stand-in Enlarge CLI. Flags match voxprint-enlarge v0.2.0-rc.1."""
    raw = list(sys.argv[1:] if argv is None else argv)
    if raw and raw[0] == "upscale":
        raw = raw[1:]
    parser = argparse.ArgumentParser(prog="voxprint-enlarge")
    parser.add_argument("--serve", action="store_true")
    parser.add_argument("--hidden", action="store_true")
    parser.add_argument("--capabilities", action="store_true")
    parser.add_argument("--json", action="store_true")
    parser.add_argument("--json-progress", action="store_true")
    parser.add_argument("--input", default="")
    parser.add_argument("--output", "--out", default="")
    parser.add_argument("--scale", type=int, default=None)
    parser.add_argument("--long-side", type=int, default=None)
    parser.add_argument("--format", default="avif")
    parser.add_argument("--lossless", action="store_true")
    parser.add_argument("--quality", default=None)
    parser.add_argument("--quality-tier", dest="quality_tier", default=None)
    parser.add_argument("--preset", default=None)
    parser.add_argument("--model", default="")
    parser.add_argument("--lock-timeout", type=float, default=None)
    parser.add_argument("--parent-lock-pid", type=int, default=None)
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args(raw)
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
            "usage: voxprint-enlarge --input FILE (--scale 2|4 | --long-side PX) "
            "--json-progress",
            file=sys.stderr,
        )
        return 2
    tier, encode_quality, quality_error = _parse_quality(args.quality, args.quality_tier)
    if quality_error is not None:
        return _emit(
            [
                {
                    "id": "cli",
                    "type": "error",
                    "code": "bad_input",
                    "message": quality_error,
                    "retryable": False,
                }
            ]
        )
    request = {
        "id": "cli",
        "input": args.input,
        "scale": args.scale,
        "long_side": args.long_side,
        "preset": args.preset,
        "quality": tier,
        "quality_tier": args.quality_tier,
        "encode_quality": encode_quality,
        "model": args.model or None,
        "format": args.format,
        "output": args.output,
        "lossless": args.lossless,
        "lock_timeout": args.lock_timeout,
        "parent_lock_pid": args.parent_lock_pid,
    }
    messages, _held = build_job(request, hold_cancel=False, dry_run=args.dry_run)
    return _emit(messages)


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


def _parse_quality(
    quality: str | None, quality_tier: str | None
) -> tuple[str, int, str | None]:
    tier = "fast"
    if quality_tier is not None:
        if quality_tier not in {"fast", "max"}:
            return tier, 60, "--quality-tier must be fast or max."
        tier = quality_tier
    if quality is None:
        return tier, 60, None
    if quality in {"fast", "max"}:
        if quality_tier is None:
            tier = quality
        return tier, 60, None
    try:
        encode = int(quality)
    except ValueError:
        return tier, 60, "--quality must be 1-100, fast, or max."
    if encode < 1 or encode > 100:
        return tier, 60, "--quality must be 1-100, fast, or max."
    return tier, encode, None


def _resolve_scale(
    request: dict[str, Any], array_shape: tuple[int, ...] | None
) -> tuple[int, str | None]:
    scale = request.get("scale", None)
    long_side = request.get("long_side", None)
    if scale is not None and long_side is not None:
        return 0, "--scale and --long-side cannot be combined."
    if long_side is not None:
        if not isinstance(long_side, int) or isinstance(long_side, bool) or long_side < 1:
            return 0, "--long-side must be a positive pixel count."
        if array_shape is None:
            return 4, None
        height, width = int(array_shape[0]), int(array_shape[1])
        long = max(width, height)
        if 4 * long < long_side:
            return 0, "The long side is beyond 4x."
        return (4 if 2 * long < long_side else 2), None
    if scale is None:
        return 4, None
    if not isinstance(scale, int) or isinstance(scale, bool) or scale not in (2, 4):
        return 0, "scale must be 2 or 4."
    return scale, None


def _planned_size(
    shape: tuple[int, ...], scale: int, long_side: object
) -> tuple[int, int]:
    height, width = int(shape[0]), int(shape[1])
    if not isinstance(long_side, int) or isinstance(long_side, bool):
        return width * scale, height * scale
    long = max(width, height)
    short = min(width, height)
    target_short = max(1, round(short * long_side / long))
    if width >= height:
        return long_side, target_short
    return target_short, long_side


def _fit_long_side(array: np.ndarray[Any, np.dtype[np.uint8]], width: int, height: int):
    if array.shape[0] == height and array.shape[1] == width:
        return array
    ys = np.linspace(0, array.shape[0] - 1, height).round().astype(np.intp)
    xs = np.linspace(0, array.shape[1] - 1, width).round().astype(np.intp)
    return np.ascontiguousarray(array[ys][:, xs])


def _select_model(request: dict[str, Any]) -> dict[str, Any] | str:
    model_id = request.get("model")
    if isinstance(model_id, str) and model_id.strip():
        found = next((model for model in _MODELS if model["id"] == model_id), None)
        if found is None:
            return "unknown_model"
        if not found["installed"]:
            return "model_missing"
        return found
    preset = request.get("preset")
    quality = request.get("quality") if request.get("quality") in {"fast", "max"} else "fast"
    tier = request.get("quality_tier")
    if tier in {"fast", "max"}:
        quality = tier
    if not isinstance(preset, str) or preset not in {"photo", "art"}:
        return "bad_preset"
    if preset == "art":
        found = next(model for model in _MODELS if model["preset"] == "art")
    elif quality == "max":
        found = next(model for model in _MODELS if model["id"] == "nomos2-dat2")
    else:
        found = next(model for model in _MODELS if model["id"] == "4x-nomos2-realplksr-dysample")
    if not found["installed"]:
        return "model_missing"
    return found


def _model_error(token: str) -> tuple[str, str, bool]:
    if token == "model_missing":
        return "model_missing", "The enlarge model is not installed.", True
    if token == "unknown_model":
        return "bad_input", "Unknown model id.", False
    if token == "bad_quality":
        return "bad_input", "--quality-tier must be fast or max.", False
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
    return enlarged_destination(source, scale, fmt, None)


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
