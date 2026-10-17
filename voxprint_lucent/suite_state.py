"""Suite and app settings documents.

Writers lock ``<file>.lock``, re-read, change only their own keys, and publish
with a temporary file, an fsync, and a replace. Readers reload when the file
changes and keep the last good object when the file is invalid.
"""

from __future__ import annotations

import errno
import json
import os
import sys
import time
import uuid
from pathlib import Path
from typing import Any

from voxprint_lucent.presets import reserved_setting_key

try:
    import fcntl
except ImportError:  # Windows has no fcntl.
    fcntl = None  # type: ignore[assignment]
try:
    import msvcrt
except ImportError:  # POSIX has no msvcrt.
    msvcrt = None  # type: ignore[assignment]

_GOOD: dict[str, dict[str, Any]] = {}
_TOKEN: dict[str, tuple[int, int]] = {}


def data_home() -> Path:
    """Return the suite data root.

    ``VOXPRINT_HOME``, when set and not empty, replaces the platform directory.
    This is the only function that spells that directory.
    """
    override = os.environ.get("VOXPRINT_HOME", "").strip()
    if override:
        return Path(override)
    if sys.platform == "win32":
        local = os.environ.get("LOCALAPPDATA", str(Path.home() / "AppData" / "Local"))
        return Path(local) / "VOXPrint AI Suite"
    xdg = os.environ.get("XDG_DATA_HOME", "").strip()
    base = Path(xdg) if xdg and os.path.isabs(xdg) else Path.home() / ".local" / "share"
    return base / "voxprint-ai-suite"


def suite_settings_path() -> Path:
    """Return ``<data home>/state/suite.json``."""
    return data_home() / "state" / "suite.json"


def app_settings_path(app_id: str = "lucent") -> Path:
    """Return ``<data home>/state/<app-id>/image-settings.json``."""
    return data_home() / "state" / app_id / "image-settings.json"


def read_document(path: Path) -> dict[str, Any]:
    """Return a JSON object, or the last good one when the file is invalid.

    A missing file is an empty object.
    """
    key = str(path.resolve()) if path.exists() else str(path)
    try:
        stat = path.stat()
    except OSError:
        return {}
    token = (stat.st_mtime_ns, stat.st_size)
    if _TOKEN.get(key) == token and key in _GOOD:
        return dict(_GOOD[key])
    try:
        payload = json.loads(path.read_text(encoding="utf-8-sig"))
    except (OSError, UnicodeError, json.JSONDecodeError):
        return dict(_GOOD[key]) if key in _GOOD else {}
    if not isinstance(payload, dict):
        return dict(_GOOD[key]) if key in _GOOD else {}
    _GOOD[key] = payload
    _TOKEN[key] = token
    return dict(payload)


def write_own_keys(path: Path, own_keys: dict[str, Any]) -> None:
    """Lock, re-read, replace ``own_keys``, and publish the whole document.

    Args:
        path: Destination JSON file.
        own_keys: Keys this writer owns. Nested objects replace that key.

    Raises:
        ValueError: A key is reserved.
        TimeoutError: The lock was not acquired.
        OSError: The file could not be published.
    """
    for key in own_keys:
        if reserved_setting_key(key):
            raise ValueError(f"reserved key {key}")
    lock_path = Path(str(path) + ".lock")
    with ExclusiveLock(lock_path):
        current = _reread(path)
        if "schema" not in current:
            current["schema"] = 1
        current.update(own_keys)
        _atomic_write(path, json.dumps(current, indent=2, ensure_ascii=False) + "\n")
        try:
            stat = path.stat()
        except OSError:
            stat = None
        resolved = str(path.resolve()) if path.exists() else str(path)
        _GOOD[resolved] = dict(current)
        if stat is not None:
            _TOKEN[resolved] = (stat.st_mtime_ns, stat.st_size)


def read_expert_mode() -> bool:
    """Return ``ui.expert_mode``. Missing or invalid storage is false."""
    stored = read_document(suite_settings_path()).get("ui.expert_mode")
    return stored if isinstance(stored, bool) else False


def write_expert_mode(enabled: bool) -> bool:
    """Store Expert mode without touching any other suite key.

    Raises:
        ValueError: ``enabled`` is not a bool.
    """
    if not isinstance(enabled, bool):
        raise ValueError(f"ui.expert_mode must be true or false, got {enabled!r}")
    write_own_keys(suite_settings_path(), {"ui.expert_mode": enabled})
    return enabled


def ui_language() -> str:
    """Return the suite language tag, or ``en`` when it is missing."""
    stored = read_document(suite_settings_path()).get("ui_language")
    if isinstance(stored, str) and stored:
        return stored.split("-", 1)[0].lower()
    return "en"


def _reread(path: Path) -> dict[str, Any]:
    try:
        payload = json.loads(path.read_text(encoding="utf-8-sig"))
    except (OSError, UnicodeError, json.JSONDecodeError):
        key = str(path.resolve()) if path.exists() else str(path)
        return dict(_GOOD.get(key, {}))
    if isinstance(payload, dict):
        return payload
    key = str(path.resolve()) if path.exists() else str(path)
    return dict(_GOOD.get(key, {}))


def _atomic_write(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(f".{path.name}.{os.getpid()}.{uuid.uuid4().hex}.tmp")
    try:
        with temporary.open("wb") as handle:
            handle.write(text.encode("utf-8"))
            handle.flush()
            os.fsync(handle.fileno())
        os.replace(temporary, path)
        _fsync_dir(path.parent)
    finally:
        if temporary.exists():
            temporary.unlink()


def _fsync_dir(directory: Path) -> None:
    if os.name != "posix":
        return
    descriptor = os.open(directory, os.O_RDONLY)
    try:
        os.fsync(descriptor)
    finally:
        os.close(descriptor)


class ExclusiveLock:
    """Hold ``<file>.lock`` until the write finishes. The file stays."""

    def __init__(self, path: Path, timeout: float = 30.0) -> None:
        """Remember the lock path and how long to wait."""
        self._path = path
        self._timeout = timeout
        self._descriptor = -1

    def __enter__(self) -> ExclusiveLock:
        """Acquire the exclusive lock."""
        path = self._path
        path.parent.mkdir(parents=True, exist_ok=True)
        deadline = time.monotonic() + self._timeout
        self._descriptor = os.open(str(path), os.O_CREAT | os.O_RDWR, 0o644)
        if os.lseek(self._descriptor, 0, os.SEEK_END) == 0:
            os.write(self._descriptor, b"\0")
        os.lseek(self._descriptor, 0, os.SEEK_SET)
        while True:
            try:
                _lock(self._descriptor)
                break
            except BlockingIOError:
                if time.monotonic() >= deadline:
                    os.close(self._descriptor)
                    self._descriptor = -1
                    raise TimeoutError(f"timed out waiting for lock {path}") from None
                time.sleep(0.05)
        return self

    def __exit__(self, *_exc: object) -> None:
        """Release the lock and leave the lock file in place."""
        if self._descriptor >= 0:
            _unlock(self._descriptor)
            os.close(self._descriptor)
            self._descriptor = -1


def _lock(descriptor: int) -> None:
    if msvcrt is not None and os.name == "nt":
        os.lseek(descriptor, 0, os.SEEK_SET)
        try:
            msvcrt.locking(descriptor, msvcrt.LK_NBLCK, 1)
        except OSError as exc:
            raise BlockingIOError(str(exc)) from exc
        return
    if fcntl is None:
        raise OSError("this platform has no file lock")
    try:
        fcntl.flock(descriptor, fcntl.LOCK_EX | fcntl.LOCK_NB)
    except OSError as exc:
        if exc.errno in (errno.EACCES, errno.EAGAIN):
            raise BlockingIOError(str(exc)) from exc
        raise


def _unlock(descriptor: int) -> None:
    if msvcrt is not None and os.name == "nt":
        os.lseek(descriptor, 0, os.SEEK_SET)
        msvcrt.locking(descriptor, msvcrt.LK_UNLCK, 1)
        return
    if fcntl is None:
        return
    fcntl.flock(descriptor, fcntl.LOCK_UN)
