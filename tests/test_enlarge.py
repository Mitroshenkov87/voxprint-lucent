"""Enlarge client against the mock server, and the before/after compare."""

from __future__ import annotations

import hashlib
import json
import stat
import subprocess
import sys
import uuid
from pathlib import Path

import pytest
from PySide6.QtGui import QColor, QImage, QPainter
from PySide6.QtWidgets import QApplication, QPushButton

from voxprint_lucent.enlarge_client import EnlargeClient, EnlargeError, command_from_desktop
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
from voxprint_lucent.enlarge_protocol import (
    action_tooltip,
    action_visible,
    dumps,
    enlarged_destination,
    loads,
    parse_capabilities,
    pipe_name_for_sid,
)
from voxprint_lucent.enlarge_ui import EnlargeWindow
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
    assert dest.suffix == ".avif"
    assert dest != source
    assert dest != taken
    assert not dest.exists()


def test_gpu_none_hides_and_warn_stays() -> None:
    hidden = parse_capabilities(
        {"gpu": {"status": "none", "reason": "No usable GPU is available."}}
    )
    assert action_visible(hidden) is False
    assert "GPU" in action_tooltip(hidden)
    warned = parse_capabilities({"gpu": {"status": "warn", "reason": "Below RTX 40"}})
    assert action_visible(warned) is True
    assert "RTX" in action_tooltip(warned)
    assert action_visible(None) is False


def test_desktop_exec_requires_protocol_1(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    text = "[Desktop Entry]\nExec=/opt/enlarge/voxprint-enlarge %f\nX-Voxprint-Protocol=1\n"
    assert command_from_desktop(text) == ["/opt/enlarge/voxprint-enlarge"]
    assert command_from_desktop("Exec=/opt/enlarge/voxprint-enlarge\n") is None
    applications = tmp_path / "applications"
    applications.mkdir()
    (applications / "ai.voxprint.Enlarge.desktop").write_text(text, encoding="utf-8")
    monkeypatch.setenv("XDG_DATA_HOME", str(tmp_path))
    monkeypatch.delenv("LUCENT_ENLARGE_EXE", raising=False)
    monkeypatch.setenv("PATH", str(tmp_path))
    assert EnlargeClient().command == ["/opt/enlarge/voxprint-enlarge"]


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
    assert data["gpu"]["status"] == "none"
    assert data["gpu"]["reason"]


def test_cli_json_progress_writes_avif(tmp_path: Path) -> None:
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
    done = messages[-1]
    assert done["format"] == "avif"
    assert done["width"] == 4
    assert done["height"] == 4
    assert Path(done["output"]).is_file()
    assert Path(done["output"]).suffix == ".avif"


def test_cli_model_missing_is_an_error(tmp_path: Path) -> None:
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
    assert proc.returncode != 0
    messages = [json.loads(line) for line in proc.stdout.splitlines() if line.strip()]
    assert messages[-1]["code"] == "model_missing"
    assert messages[-1]["retryable"] is False


def test_existing_output_is_not_overwritten(tmp_path: Path) -> None:
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
    assert written != taken
    assert written.is_file()
    assert taken.read_bytes() == b"keep"


def test_unknown_request_field_is_ignored(tmp_path: Path) -> None:
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


def test_socket_hello_and_done(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    _app()
    source = tmp_path / "dot.png"
    _png(source)
    client = _client(monkeypatch, tmp_path)
    seen: list[str] = []
    try:
        result = client.enlarge(
            source,
            transport="socket",
            on_event=lambda message: seen.append(str(message["type"])),
        )
        if sys.platform != "win32":
            mode = stat.S_IMODE(Path(client.socket).stat().st_mode)
            parent = stat.S_IMODE(Path(client.socket).parent.stat().st_mode)
            assert mode == 0o600
            assert parent == 0o700
    finally:
        client.shutdown()
    assert seen[0] == "hello"
    assert "accepted" in seen
    assert "done" in seen
    assert result.format == "avif"
    assert result.width == 4
    assert result.height == 4
    assert result.output.is_file()
    assert result.output.resolve() != source.resolve()


def test_socket_model_missing(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    _app()
    source = tmp_path / "missing-model.png"
    source.write_bytes(b"pending")
    client = _client(monkeypatch, tmp_path)
    try:
        with pytest.raises(EnlargeError) as caught:
            client.enlarge(source, transport="socket")
    finally:
        client.shutdown()
    assert caught.value.code == "model_missing"
    assert caught.value.retryable is False


def test_socket_cancel(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    _app()
    source = tmp_path / "cancel-me.png"
    source.write_bytes(b"pending")
    client = _client(monkeypatch, tmp_path)
    seen: list[str] = []

    def on_event(message: dict[str, object]) -> None:
        kind = str(message.get("type"))
        seen.append(kind)
        if kind == "progress":
            client.cancel()

    try:
        with pytest.raises(EnlargeError) as caught:
            client.enlarge(source, transport="socket", on_event=on_event)
    finally:
        client.shutdown()
    assert caught.value.code == "cancelled"
    assert "accepted" in seen
    assert "progress" in seen


def test_open_in_enlarge_still_returns_a_file(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    _app()
    source = tmp_path / "dot.png"
    _png(source)
    client = _client(monkeypatch, tmp_path)
    try:
        result = client.enlarge(source, open_ui=True, transport="socket")
    finally:
        client.shutdown()
    assert result.output.is_file()


def test_cli_fallback_when_serve_fails(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    _app()
    source = tmp_path / "dot.png"
    _png(source)
    client = _client(monkeypatch, tmp_path)
    monkeypatch.setenv("LUCENT_ENLARGE_MOCK_SERVE_FAIL", "1")
    try:
        result = client.enlarge(source, transport="auto")
    finally:
        client.shutdown()
    assert result.output.is_file()
    assert result.format == "avif"


def test_png_output_is_rejected(tmp_path: Path) -> None:
    _app()
    source = tmp_path / "dot.png"
    _png(source)
    client = EnlargeClient(command=[sys.executable, "-m", "voxprint_lucent.enlarge_mock"])
    with pytest.raises(PngRejected):
        client.enlarge(source, format_name="png", transport="cli")


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
        assert window.findChild(QPushButton, "quick-enlarge") is None
        assert "GPU" in window.status_text()
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
        assert "RTX" in button.toolTip()
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


def test_tools_host_enlarge_needs_an_image(capsys: pytest.CaptureFixture[str]) -> None:
    assert main(["--enlarge"]) == 2
    assert "usage:" in capsys.readouterr().out
