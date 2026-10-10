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
    socket_name,
)
from voxprint_lucent.formats import (
    MODERN_OUTPUT_FORMATS,
    CodecMissing,
    OutputFormat,
    PngRejected,
    unique_path,
    write_raster,
)

_GPU_REASONS = {
    "none": "No usable GPU is available.",
    "ok": "",
    "warn": "Below RTX 40. Enlarge can run, with a warning.",
}


def capabilities() -> dict[str, Any]:
    """One capabilities object. The default GPU status is ``none`` with a reason."""
    status = os.environ.get("LUCENT_ENLARGE_GPU", "none").strip() or "none"
    if status not in _GPU_REASONS:
        status = "none"
    return {
        "protocol": PROTOCOL,
        "name": "voxprint-enlarge",
        "version": "0.0.0-mock",
        "protocol_min": PROTOCOL,
        "protocol_max": PROTOCOL,
        "gpu": {"status": status, "reason": _GPU_REASONS[status]},
        "formats": ["avif", "jxl", "webp"],
        "scales": [2, 4],
        "presets": ["photo"],
    }


def build_job(request: dict[str, Any], *, hold_cancel: bool) -> tuple[list[dict[str, Any]], bool]:
    """Messages to send now, and whether the socket should wait for ``cancel``."""
    job_id = request.get("id")
    ident = job_id if isinstance(job_id, str) else ""

    def message(body: dict[str, Any]) -> dict[str, Any]:
        return {"id": ident, **body}

    raw_input = request.get("input")
    if not isinstance(raw_input, str) or not raw_input:
        return [_error(message, "bad_input", "enlarge.request needs an input path.", False)], False
    source = Path(raw_input)
    if not source.is_file():
        return [_error(message, "bad_input", f"No such image: {source.name}", False)], False
    if "missing-model" in source.name:
        return [
            message({"type": "accepted", "queue_pos": 0}),
            _error(message, "model_missing", "The enlarge model is not installed.", False),
        ], False
    if "cancel-me" in source.name:
        waiting = [
            message({"type": "accepted", "queue_pos": 0}),
            message({"type": "progress", "pct": 10, "stage": "load", "eta_s": 1}),
        ]
        if hold_cancel:
            return waiting, True
        return [*waiting, _error(message, "cancelled", "Cancelled.", False)], False
    scale = request.get("scale", 2)
    if not isinstance(scale, int) or isinstance(scale, bool) or scale not in {2, 4}:
        return [_error(message, "bad_input", "scale must be 2 or 4.", False)], False
    preset = request.get("preset", "photo")
    if not isinstance(preset, str) or not preset.strip():
        return [_error(message, "bad_input", "preset must be a name.", False)], False
    format_name = request.get("format", "avif")
    if not isinstance(format_name, str):
        return [_error(message, "unsupported_format", "format must be a name.", False)], False
    try:
        fmt = output_format_name(format_name)
    except PngRejected as exc:
        return [_error(message, "unsupported_format", str(exc), False)], False
    except ValueError as exc:
        return [_error(message, "unsupported_format", str(exc), False)], False
    if fmt not in MODERN_OUTPUT_FORMATS:
        return [
            _error(message, "unsupported_format", "Enlarge writes AVIF, JPEG XL, or WebP.", False)
        ], False
    try:
        started = time.perf_counter()
        array = _load_rgb(source)
        scaled = np.repeat(np.repeat(array, scale, axis=0), scale, axis=1)
        scaled = np.ascontiguousarray(scaled)
        dest = _destination(request, source, scale, fmt)
        write_raster(scaled, dest, fmt, lossless=fmt is not OutputFormat.AVIF)
    except PngRejected as exc:
        return [_error(message, "unsupported_format", str(exc), False)], False
    except (OSError, CodecMissing, ValueError) as exc:
        return [_error(message, "io_error", str(exc), True)], False
    height, width = int(scaled.shape[0]), int(scaled.shape[1])
    elapsed = max(0, int((time.perf_counter() - started) * 1000))
    return [
        message({"type": "accepted", "queue_pos": 0}),
        message({"type": "progress", "pct": 40, "stage": "load"}),
        message({"type": "progress", "pct": 90, "stage": "upscale", "eta_s": 0}),
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
    """Listen on the protocol socket until the process is stopped."""
    if os.environ.get("LUCENT_ENLARGE_MOCK_SERVE_FAIL") == "1":
        print("mock serve disabled", file=sys.stderr)
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
    parser.add_argument("--format", default="avif")
    parser.add_argument("--output", default="")
    parser.add_argument("--output-dir", default="")
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
            "usage: voxprint-enlarge --serve --hidden | --capabilities --json | --json-progress",
            file=sys.stderr,
        )
        return 2
    request = {
        "id": "cli",
        "input": args.input,
        "scale": args.scale,
        "preset": args.preset,
        "format": args.format,
        "output": args.output,
        "output_dir": args.output_dir,
        "open_ui": False,
    }
    messages, _held = build_job(request, hold_cancel=False)
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
                    "server": "voxprint-enlarge-mock",
                    "version": "0.0.0-mock",
                    "protocol_min": PROTOCOL,
                    "protocol_max": PROTOCOL,
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
        if dest.resolve() == source.resolve():
            raise OSError("Refusing to overwrite the source.")
        if dest.exists():
            dest = unique_path(dest)
        return dest
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
