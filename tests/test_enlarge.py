"""Enlarge client against the mock server, and the before/after compare."""

from __future__ import annotations

import hashlib
import json
import stat
import subprocess
import sys
import time
import uuid
from pathlib import Path

import imagecodecs
import numpy as np
import pytest
from PySide6.QtGui import QColor, QImage, QImageReader, QPainter
from PySide6.QtWidgets import QApplication, QPushButton

from bench.hard_images import _animated_webp
from voxprint_lucent.discovery import (
    AppInstall,
    command_from_desktop,
    probe_capabilities,
    store_cached,
)
from voxprint_lucent.enlarge_client import EnlargeClient, EnlargeError, hello_supported
from voxprint_lucent.enlarge_compare import (
    CompareWindow,
    ViewState,
    discard_output,
    paint_split,
    pan_by,
    reveal_command,
    split_cut,
    zoom_at,
)
from voxprint_lucent.enlarge_mock import build_job
from voxprint_lucent.enlarge_mock import main as enlarge_main
from voxprint_lucent.enlarge_protocol import (
    ENLARGE_RELEASES_URL,
    action_enabled,
    action_tooltip,
    action_visible,
    dumps,
    enlarged_destination,
    install_offered,
    loads,
    parse_capabilities,
    pipe_name_for_sid,
)
from voxprint_lucent.enlarge_ui import EnlargeWindow, still_for_enlarge
from voxprint_lucent.formats import PngRejected
from voxprint_lucent.theme_tokens import ACCENT, ROOT_PLAIN
from voxprint_lucent.tools_host import main


def _app() -> QApplication:
    return QApplication.instance() or QApplication([])


def _png(path: Path) -> None:
    image = QImage(2, 2, QImage.Format.Format_RGB32)
    image.fill(0xFFFF0000)
    assert image.save(str(path), "PNG")


def _client(monkeypatch: pytest.MonkeyPatch, tmp_path: Path, gpu: str = "ok") -> EnlargeClient:
    if sys.platform == "win32":
        name = f"lucent-enlarge-{uuid.uuid4().hex}"
    else:
        name = str(tmp_path / "voxprint" / "enlarge.sock")
    monkeypatch.setenv("LUCENT_ENLARGE_SOCKET", name)
    monkeypatch.setenv(
        "LUCENT_ENLARGE_EXE",
        f"{sys.executable} -m voxprint_lucent.enlarge_mock",
    )
    monkeypatch.setenv("LUCENT_ENLARGE_GPU", gpu)
    monkeypatch.setenv("LUCENT_CACHE", str(tmp_path / "cache"))
    monkeypatch.delenv("LUCENT_ENLARGE_MOCK_SERVE_FAIL", raising=False)
    return EnlargeClient()


def test_protocol_keeps_unknown_fields() -> None:
    parsed = loads(dumps({"type": "hello", "future": 1}))
    assert parsed["protocol"] == 1
    assert parsed["future"] == 1
    assert parsed["type"] == "hello"


def test_pipe_name_is_the_sid_hash() -> None:
    digest = hashlib.sha256(b"S-1-5-21-1").hexdigest()[:16]
    assert pipe_name_for_sid("S-1-5-21-1") == f"voxprint-enlarge-{digest}"


def test_destination_never_overwrites(tmp_path: Path) -> None:
    source = tmp_path / "shot.jpg"
    source.write_bytes(b"jpeg")
    taken = tmp_path / "shot.enlarged-2x.avif"
    taken.write_bytes(b"old")
    dest = enlarged_destination(source, 2)
    assert dest.name == "shot.enlarged-2x-2.avif"
    assert dest != source
    assert dest != taken
    assert not dest.exists()


def test_gpu_none_hides_and_warn_stays() -> None:
    hidden = parse_capabilities({"gpu": {"status": "none", "reason": "no_nvidia"}})
    assert action_visible(hidden) is False
    assert action_tooltip(hidden) == "no_nvidia"
    old = parse_capabilities({"gpu": {"status": "none", "reason": "driver_too_old"}})
    assert action_visible(old) is False
    assert action_tooltip(old) == "driver_too_old"
    warned = parse_capabilities({"gpu": {"status": "warn", "reason": "below_rtx_40"}})
    assert action_visible(warned) is True
    assert action_tooltip(warned) == "below_rtx_40"
    ready = parse_capabilities({"gpu": {"status": "ok", "reason": None}})
    assert action_visible(ready) is True
    assert action_tooltip(ready) == "Enlarge"
    assert action_visible(None) is False
    assert action_enabled(None) is False
    assert action_enabled(hidden) is False
    assert action_enabled(warned) is True
    assert install_offered(None) is True
    assert install_offered(hidden) is False
    assert install_offered(ready) is False
    assert hello_supported(
        {"type": "hello", "server": "enlarge", "protocols": [1]}
    )
    assert hello_supported({"type": "hello", "server": "enlarge", "protocol_min": 1}) is False


def test_desktop_exec_requires_protocol_1(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    text = "[Desktop Entry]\nExec=/opt/enlarge/voxprint-enlarge %f\nX-Voxprint-Protocol=1\n"
    assert command_from_desktop(text) == ["/opt/enlarge/voxprint-enlarge"]
    assert command_from_desktop("Exec=/opt/enlarge/voxprint-enlarge\n") is None
    applications = tmp_path / "applications"
    applications.mkdir()
    (applications / "ai.voxprint.Enlarge.desktop").write_text(text, encoding="utf-8")
    monkeypatch.setenv("XDG_DATA_HOME", str(tmp_path))
    monkeypatch.setenv("XDG_DATA_DIRS", str(tmp_path / "no-share"))
    monkeypatch.delenv("LUCENT_ENLARGE_EXE", raising=False)
    monkeypatch.setenv("PATH", str(tmp_path))
    assert EnlargeClient().command == []
    exe = tmp_path / "voxprint-enlarge"
    exe.write_text("#!/bin/sh\nexit 0\n", encoding="utf-8")
    exe.chmod(stat.S_IRWXU)
    desktop = (
        "[Desktop Entry]\n"
        f"Exec={exe} %f\n"
        "X-Voxprint-Protocol=1\n"
        "X-Voxprint-Ipc=voxprint-enlarge\n"
        "X-Voxprint-Version=0.1.0\n"
    )
    (applications / "ai.voxprint.Enlarge.desktop").write_text(desktop, encoding="utf-8")
    client = EnlargeClient()
    assert client.command == [str(exe)]
    assert client.install is not None
    assert client.install.version == "0.1.0"
    assert client.install.ipc_base == "voxprint-enlarge"


def test_capabilities_json_reports_no_gpu(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.delenv("LUCENT_ENLARGE_GPU", raising=False)
    proc = subprocess.run(
        [sys.executable, "-m", "voxprint_lucent.enlarge_mock", "--capabilities", "--json"],
        capture_output=True,
        text=True,
        check=False,
    )
    assert proc.returncode == 0, proc.stderr
    data = json.loads(proc.stdout)
    assert data["protocol"] == 1
    assert data["app"] == "enlarge"
    assert data["gpu"]["status"] == "none"
    assert data["gpu"]["reason"] == "no_nvidia"
    assert data["gpu"]["name"] is None
    assert data["default_output_format"] == "avif"
    assert "protocol_min" not in data
    dat2 = next(item for item in data["models"] if item["id"] == "nomos2-dat2")
    assert dat2["installed"] is False


def test_capabilities_drop_duplicate_model_ids() -> None:
    parsed = parse_capabilities(
        {
            "version": "0.2.0-rc.3",
            "gpu": {"status": "ok"},
            "models": [
                {"id": "realesrgan-x4plus-anime6b", "installed": True, "quality": "fast"},
                {"id": "realesrgan-x4plus-anime6b", "installed": True, "quality": "max"},
                {"id": "4x-nomos2-hq-dat2", "installed": True},
            ],
        }
    )
    ids = [item["id"] for item in parsed.raw["models"]]
    assert ids == ["realesrgan-x4plus-anime6b", "4x-nomos2-hq-dat2"]
    assert parsed.raw["models"][0]["quality"] == "fast"


def test_cli_json_progress_writes_avif(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setenv("LUCENT_ENLARGE_GPU", "ok")
    _app()
    source = tmp_path / "dot.png"
    _png(source)
    proc = subprocess.run(
        [
            sys.executable,
            "-m",
            "voxprint_lucent.enlarge_mock",
            "--input",
            str(source),
            "--scale",
            "2",
            "--preset",
            "photo",
            "--format",
            "avif",
            "--json-progress",
        ],
        capture_output=True,
        text=True,
        check=False,
    )
    assert proc.returncode == 0, proc.stderr
    messages = [json.loads(line) for line in proc.stdout.splitlines() if line.strip()]
    assert [item["type"] for item in messages] == ["accepted", "progress", "progress", "done"]
    assert messages[1]["stage"] == "loading"
    assert messages[2]["stage"] == "tiles"
    done = messages[-1]
    assert done["format"] == "avif"
    assert done["width"] == 4
    assert done["height"] == 4
    assert Path(done["output"]).is_file()
    assert Path(done["output"]).suffix == ".avif"


def test_cli_model_missing_is_an_error(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setenv("LUCENT_ENLARGE_GPU", "ok")
    source = tmp_path / "missing-model.png"
    source.write_bytes(b"pending")
    proc = subprocess.run(
        [
            sys.executable,
            "-m",
            "voxprint_lucent.enlarge_mock",
            "--input",
            str(source),
            "--scale",
            "2",
            "--preset",
            "photo",
            "--json-progress",
        ],
        capture_output=True,
        text=True,
        check=False,
    )
    assert proc.returncode == 7
    messages = [json.loads(line) for line in proc.stdout.splitlines() if line.strip()]
    assert messages[-1]["code"] == "model_missing"
    assert messages[-1]["retryable"] is True


def test_existing_output_is_not_overwritten(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    monkeypatch.setenv("LUCENT_ENLARGE_GPU", "ok")
    _app()
    source = tmp_path / "dot.png"
    _png(source)
    taken = tmp_path / "out.avif"
    taken.write_bytes(b"keep")
    messages, hold = build_job(
        {
            "id": "job",
            "input": str(source),
            "scale": 2,
            "preset": "photo",
            "format": "avif",
            "output": str(taken),
        },
        hold_cancel=False,
    )
    assert hold is False
    written = Path(str(messages[-1]["output"]))
    assert written.name == "out-2.avif"
    assert written != taken
    assert written.is_file()
    assert taken.read_bytes() == b"keep"


def test_unknown_request_field_is_ignored(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    monkeypatch.setenv("LUCENT_ENLARGE_GPU", "ok")
    _app()
    source = tmp_path / "dot.png"
    _png(source)
    messages, hold = build_job(
        {
            "id": "job",
            "input": str(source),
            "scale": 2,
            "preset": "photo",
            "format": "avif",
            "future": True,
        },
        hold_cancel=False,
    )
    assert hold is False
    assert messages[-1]["type"] == "done"


def test_enlarge_uses_the_cli(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    _app()
    source = tmp_path / "dot.png"
    _png(source)
    client = _client(monkeypatch, tmp_path)
    seen: list[str] = []
    launched: list[list[str]] = []
    real_popen = subprocess.Popen

    def spy(*args: object, **kwargs: object) -> subprocess.Popen[str]:
        command = args[0] if args else kwargs.get("args")
        assert isinstance(command, list)
        launched.append([str(part) for part in command])
        return real_popen(*args, **kwargs)  # type: ignore[arg-type]

    monkeypatch.setattr(subprocess, "Popen", spy)
    try:
        result = client.enlarge(source, on_event=lambda message: seen.append(str(message["type"])))
    finally:
        client.shutdown()
    assert launched
    assert "--json-progress" in launched[0]
    assert "--quality-tier" in launched[0]
    assert "--scale" in launched[0]
    assert "--open-ui" not in launched[0]
    assert "--output-dir" not in launched[0]
    assert "--serve" not in launched[0]
    assert "--dry-run" not in launched[0]
    assert seen[0] == "accepted"
    assert "done" in seen
    assert result.format == "avif"
    assert result.width == 4
    assert result.height == 4
    assert result.output.is_file()
    assert result.output.resolve() != source.resolve()


def test_cli_model_missing(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    _app()
    source = tmp_path / "missing-model.png"
    source.write_bytes(b"pending")
    client = _client(monkeypatch, tmp_path)
    try:
        with pytest.raises(EnlargeError) as caught:
            client.enlarge(source)
    finally:
        client.shutdown()
    assert caught.value.code == "model_missing"
    assert caught.value.retryable is True


def test_cli_cancel_file(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    _app()
    source = tmp_path / "cancel-me.png"
    source.write_bytes(b"pending")
    client = _client(monkeypatch, tmp_path)
    seen: list[str] = []
    try:
        with pytest.raises(EnlargeError) as caught:
            client.enlarge(source, on_event=lambda message: seen.append(str(message["type"])))
    finally:
        client.shutdown()
    assert caught.value.code == "cancelled"
    assert "accepted" in seen
    assert "progress" in seen


def test_hand_off_uses_the_published_cli(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    _app()
    source = tmp_path / "dot.png"
    _png(source)
    client = _client(monkeypatch, tmp_path)
    launched: list[list[str]] = []
    real_popen = subprocess.Popen

    def spy(*args: object, **kwargs: object) -> subprocess.Popen[str]:
        command = args[0] if args else kwargs.get("args")
        assert isinstance(command, list)
        launched.append([str(part) for part in command])
        return real_popen(*args, **kwargs)  # type: ignore[arg-type]

    monkeypatch.setattr(subprocess, "Popen", spy)
    try:
        result = client.enlarge(source, scale=None)
    finally:
        client.shutdown()
    command = launched[0]
    assert "--open-ui" not in command
    assert "--scale" not in command
    assert "--long-side" not in command
    assert "--json-progress" in command
    assert result.output.name.startswith("dot.enlarged-4x.")
    assert result.output.is_file()


def test_dry_run_does_not_write(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setenv("LUCENT_ENLARGE_GPU", "ok")
    source = tmp_path / "dot.png"
    _png(source)
    dest = tmp_path / "would.avif"
    proc = subprocess.run(
        [
            sys.executable,
            "-m",
            "voxprint_lucent.enlarge_mock",
            "--input",
            str(source),
            "--scale",
            "2",
            "--preset",
            "photo",
            "--format",
            "avif",
            "--output",
            str(dest),
            "--json-progress",
            "--dry-run",
        ],
        capture_output=True,
        text=True,
        check=False,
    )
    assert proc.returncode == 0, proc.stderr
    messages = [json.loads(line) for line in proc.stdout.splitlines() if line.strip()]
    assert [item["type"] for item in messages] == ["accepted", "done"]
    assert messages[-1]["ms"] == 0
    assert messages[-1]["op"] == "done"
    assert messages[-1]["output"] == str(dest)
    assert not dest.exists()


def test_serve_socket_is_user_only(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    if sys.platform == "win32":
        pytest.skip("unix socket mode")
    monkeypatch.setenv("LUCENT_ENLARGE_GPU", "ok")
    name = str(tmp_path / "voxprint" / "enlarge.sock")
    monkeypatch.setenv("LUCENT_ENLARGE_SOCKET", name)
    proc = subprocess.Popen(
        [sys.executable, "-m", "voxprint_lucent.enlarge_mock", "--serve", "--hidden"]
    )
    try:
        deadline = time.monotonic() + 3
        while time.monotonic() < deadline and not Path(name).exists():
            time.sleep(0.05)
        assert Path(name).exists()
        mode = stat.S_IMODE(Path(name).stat().st_mode)
        parent = stat.S_IMODE(Path(name).parent.stat().st_mode)
        assert mode == 0o600
        assert parent == 0o700
    finally:
        proc.terminate()
        proc.wait(timeout=2)


def test_png_output_is_rejected(tmp_path: Path) -> None:
    _app()
    source = tmp_path / "dot.png"
    _png(source)
    client = EnlargeClient(command=[sys.executable, "-m", "voxprint_lucent.enlarge_mock"])
    with pytest.raises(PngRejected):
        client.enlarge(source, format_name="png")


def test_split_shows_before_then_after() -> None:
    _app()
    before = QImage(40, 20, QImage.Format.Format_RGB32)
    before.fill(QColor("red"))
    after = QImage(40, 20, QImage.Format.Format_RGB32)
    after.fill(QColor("blue"))
    canvas = QImage(40, 20, QImage.Format.Format_RGB32)
    painter = QPainter(canvas)
    paint_split(painter, 40, 20, before, after, ViewState(1.0, 0.0, 0.0), 0.5)
    painter.end()
    assert split_cut(40, 0.5) == 20
    assert canvas.pixelColor(4, 10).red() > 200
    assert canvas.pixelColor(36, 10).blue() > 200
    line = canvas.pixelColor(20, 10)
    accent = QColor(ACCENT)
    assert (line.red(), line.green(), line.blue()) == (accent.red(), accent.green(), accent.blue())
    empty = QImage(40, 20, QImage.Format.Format_RGB32)
    painter = QPainter(empty)
    paint_split(painter, 40, 20, QImage(), QImage(), ViewState(1.0, 0.0, 0.0), 0.0)
    painter.end()
    background = QColor(ROOT_PLAIN)
    corner = empty.pixelColor(10, 10)
    assert (corner.red(), corner.green(), corner.blue()) == (
        background.red(),
        background.green(),
        background.blue(),
    )


def test_zoom_keeps_the_cursor_point() -> None:
    state = zoom_at(ViewState(1.0, 0.0, 0.0), 10.0, 4.0, 2.0)
    assert state.zoom == pytest.approx(2.0)
    assert (10.0 - state.pan_x) / state.zoom == pytest.approx(10.0)
    assert (4.0 - state.pan_y) / state.zoom == pytest.approx(4.0)
    moved = pan_by(state, 3.0, -2.0)
    assert moved.pan_x == pytest.approx(state.pan_x + 3.0)
    assert moved.pan_y == pytest.approx(state.pan_y - 2.0)


def test_keep_leaves_the_file_and_discard_deletes_only_the_result(tmp_path: Path) -> None:
    _app()
    source = tmp_path / "dot.png"
    _png(source)
    output = tmp_path / "dot.enlarged-2x.png"
    _png(output)
    compare = CompareWindow(source, output)
    assert compare.keep() is True
    assert output.is_file()
    assert compare.discard() is True
    assert not output.exists()
    assert source.is_file()
    assert discard_output(source, source) is False
    assert source.is_file()


def test_reveal_command_targets_the_result(tmp_path: Path) -> None:
    path = tmp_path / "dot.enlarged-2x.avif"
    command = reveal_command(path)
    if sys.platform == "win32":
        assert command[0] == "explorer"
        assert str(path) in command[1]
    else:
        assert command == ["xdg-open", str(path.parent)]


def test_window_hides_enlarge_without_a_gpu(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    _app()
    source = tmp_path / "dot.png"
    _png(source)
    _client(monkeypatch, tmp_path, gpu="none")
    window = EnlargeWindow(source)
    try:
        button = window.findChild(QPushButton, "quick-enlarge")
        assert button is not None
        assert button.isEnabled() is False
        assert window.findChild(QPushButton, "install-enlarge") is None
        assert window.status_text() == "no_nvidia"
    finally:
        window.shutdown()


def test_missing_enlarge_offers_install(tmp_path: Path) -> None:
    _app()
    source = tmp_path / "dot.png"
    _png(source)
    window = EnlargeWindow(source, EnlargeClient(command=[]))
    try:
        button = window.findChild(QPushButton, "quick-enlarge")
        assert button is not None
        assert button.isEnabled() is False
        install = window.findChild(QPushButton, "install-enlarge")
        assert install is not None
        assert install.text() == "Install Enlarge"
        assert window.status_text() == "Enlarge is not installed."
        assert ENLARGE_RELEASES_URL.endswith("/voxprint-enlarge/releases")
    finally:
        window.shutdown()


def test_warn_keeps_the_button(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    _app()
    source = tmp_path / "dot.png"
    _png(source)
    _client(monkeypatch, tmp_path, gpu="warn")
    window = EnlargeWindow(source)
    try:
        button = window.findChild(QPushButton, "quick-enlarge")
        assert button is not None
        assert button.toolTip() == "warn"
    finally:
        window.shutdown()


def test_quick_enlarge_compare_keep_and_discard(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    _app()
    source = tmp_path / "dot.png"
    _png(source)
    _client(monkeypatch, tmp_path, gpu="ok")
    window = EnlargeWindow(source)
    try:
        compare = window.run_quick()
        assert compare.output.is_file()
        assert compare.output.resolve() != source.resolve()
        compare._view.setFixedSize(40, 20)
        assert compare.composed().width() == 40
        assert compare.keep() is True
        assert compare.output.is_file()
        assert compare.discard() is True
        assert not compare.output.exists()
        assert source.is_file()
    finally:
        window.shutdown()


def test_scale_preset_and_jpeg_name(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    _app()
    monkeypatch.setenv("LUCENT_ENLARGE_GPU", "ok")
    relative, _hold = build_job(
        {"id": "job", "input": "relative.png", "scale": 2, "preset": "photo"},
        hold_cancel=False,
    )
    assert relative[-1]["code"] == "bad_input"
    source = tmp_path / "dot.png"
    _png(source)
    art, _hold = build_job(
        {"id": "job", "input": str(source), "scale": 2, "preset": "art", "format": "avif"},
        hold_cancel=False,
    )
    assert art[-1]["code"] == "bad_input"
    missing, _hold = build_job(
        {
            "id": "job",
            "input": str(source),
            "scale": 2,
            "preset": "photo",
            "quality": "max",
            "format": "avif",
            "open_ui": True,
        },
        hold_cancel=False,
    )
    assert missing[-1]["code"] == "model_missing"
    assert missing[-1]["retryable"] is True
    done, _hold = build_job(
        {
            "id": "job",
            "input": str(source),
            "scale": 2,
            "preset": "photo",
            "quality": "fast",
            "format": "jpeg",
        },
        hold_cancel=False,
    )
    assert done[-1]["type"] == "done"
    assert done[-1]["format"] == "jpeg"
    assert Path(str(done[-1]["output"])).name == "dot.enlarged-2x.jpg"


def test_serve_refuses_without_a_gpu(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.delenv("LUCENT_ENLARGE_GPU", raising=False)
    monkeypatch.delenv("LUCENT_ENLARGE_MOCK_SERVE_FAIL", raising=False)
    assert enlarge_main(["--serve", "--hidden"]) == 1


def test_capabilities_cache_follows_version(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    monkeypatch.setenv("LUCENT_CACHE", str(tmp_path))
    monkeypatch.setenv("LUCENT_ENLARGE_GPU", "warn")
    store_cached(
        "enlarge",
        "0.0.0",
        {
            "protocol": 1,
            "version": "0.0.0",
            "gpu": {"status": "ok", "reason": None},
            "cached": True,
        },
    )
    command = [sys.executable, "-m", "voxprint_lucent.enlarge_mock"]
    cached = probe_capabilities(AppInstall(command, "0.0.0", "voxprint-enlarge"), "enlarge")
    assert cached is not None
    assert cached.get("cached") is True
    fresh = probe_capabilities(AppInstall(command, "9.9.9", "voxprint-enlarge"), "enlarge")
    assert fresh is not None
    assert fresh.get("cached") is not True
    assert fresh["gpu"]["status"] == "warn"
    assert fresh["gpu"]["reason"] is None


def test_published_exit_codes(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setenv("LUCENT_ENLARGE_GPU", "ok")
    source = tmp_path / "dot.png"
    _png(source)
    both = subprocess.run(
        [
            sys.executable,
            "-m",
            "voxprint_lucent.enlarge_mock",
            "--input",
            str(source),
            "--scale",
            "2",
            "--long-side",
            "20",
            "--preset",
            "photo",
            "--json-progress",
        ],
        capture_output=True,
        text=True,
        check=False,
    )
    assert both.returncode == 2
    assert json.loads(both.stdout.splitlines()[-1])["code"] == "bad_input"
    missing_json = subprocess.run(
        [sys.executable, "-m", "voxprint_lucent.enlarge_mock", "--capabilities"],
        capture_output=True,
        text=True,
        check=False,
    )
    assert missing_json.returncode == 2
    monkeypatch.delenv("LUCENT_ENLARGE_GPU", raising=False)
    dry = subprocess.run(
        [
            sys.executable,
            "-m",
            "voxprint_lucent.enlarge_mock",
            "--input",
            str(source),
            "--scale",
            "2",
            "--preset",
            "photo",
            "--json-progress",
            "--dry-run",
        ],
        capture_output=True,
        text=True,
        check=False,
    )
    assert dry.returncode == 4
    assert json.loads(dry.stdout.splitlines()[-1])["code"] == "gpu_unsupported"


def test_long_side_uses_the_model_scale(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setenv("LUCENT_ENLARGE_GPU", "ok")
    _app()
    source = tmp_path / "dot.png"
    _png(source)
    proc = subprocess.run(
        [
            sys.executable,
            "-m",
            "voxprint_lucent.enlarge_mock",
            "upscale",
            "--input",
            str(source),
            "--long-side",
            "6",
            "--preset",
            "photo",
            "--quality-tier",
            "fast",
            "--quality",
            "fast",
            "--json-progress",
        ],
        capture_output=True,
        text=True,
        check=False,
    )
    assert proc.returncode == 0, proc.stderr
    done = json.loads(proc.stdout.splitlines()[-1])
    assert done["width"] == 6
    assert done["height"] == 6
    assert Path(done["output"]).name == "dot.enlarged-4x.avif"


def test_tools_host_enlarge_needs_an_image(capsys: pytest.CaptureFixture[str]) -> None:
    assert main(["--enlarge"]) == 2
    assert "usage:" in capsys.readouterr().out


def test_animated_webp_dry_run_uses_the_first_frame(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    image = np.zeros((8, 8, 3), dtype=np.uint8)
    image[:, :, 0] = 255
    path = tmp_path / "animated.webp"
    path.write_bytes(_animated_webp(image))
    monkeypatch.setenv("LUCENT_ENLARGE_GPU", "ok")
    code = enlarge_main(
        [
            "--input",
            str(path.resolve()),
            "--scale",
            "2",
            "--preset",
            "photo",
            "--quality-tier",
            "fast",
            "--format",
            "avif",
            "--json-progress",
            "--dry-run",
        ]
    )
    assert code == 0


def test_multi_frame_file_is_sent_as_a_still(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    image = np.zeros((8, 8, 3), dtype=np.uint8)
    image[..., 0] = 255
    other = np.zeros_like(image)
    other[..., 2] = 255
    path = tmp_path / "clip.gif"
    path.write_bytes(bytes(imagecodecs.gif_encode(np.stack((image, other)))))
    assert QImageReader(str(path)).imageCount() > 1
    monkeypatch.chdir(tmp_path)
    still = still_for_enlarge(Path("clip.gif"), 0)
    assert still.is_absolute()
    assert still.suffix == ".png"
    assert still.is_file()
    assert QImageReader(str(still)).imageCount() <= 1
