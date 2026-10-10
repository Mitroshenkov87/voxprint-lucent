"""How Lucent finds another Voxprint app.

This is Lucent's copy of suite/APP-DISCOVERY.md. A shared voxprint-common
package replaces it once that repository exists. Enlarge-specific messages
stay in enlarge_protocol.py.
"""

from __future__ import annotations

import hashlib
import json
import os
import shlex
import shutil
import subprocess
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Any

_CACHE_SECONDS = 24 * 60 * 60


@dataclass(frozen=True)
class AppInstall:
    """One installed app the discovery rules were willing to use."""

    command: list[str]
    version: str
    ipc_base: str


def pipe_name(ipc_base: str, sid: str) -> str:
    """Qt server name. Windows exposes it as ``\\\\.\\pipe\\<name>``."""
    digest = hashlib.sha256(sid.encode("utf-8")).hexdigest()[:16]
    return f"{ipc_base}-{digest}"


def socket_path(app: str, ipc_base: str) -> str:
    """Standard socket for ``app``. ``LUCENT_ENLARGE_SOCKET`` is not applied here."""
    if sys.platform == "win32":
        return pipe_name(ipc_base, _windows_user_sid())
    runtime = os.environ.get("XDG_RUNTIME_DIR", "").strip()
    if not runtime:
        raise OSError("XDG_RUNTIME_DIR is not set")
    return str(Path(runtime) / "voxprint" / f"{app}.sock")


def find_app(pascal: str, kebab: str) -> AppInstall | None:
    """Registry or desktop entry, then App Paths, then ``voxprint-<kebab>`` on PATH.

    The executable must exist. Flatpak is not planned.
    """
    if sys.platform == "win32":
        found = _from_windows_registry(pascal, kebab)
    else:
        found = _from_desktop_dirs(pascal, kebab)
    if found is not None:
        return found
    program = f"voxprint-{kebab}.exe" if sys.platform == "win32" else f"voxprint-{kebab}"
    which = shutil.which(program)
    if which is None and sys.platform == "win32":
        which = shutil.which(f"voxprint-{kebab}")
    if which is None:
        return None
    return AppInstall([which], "", f"voxprint-{kebab}")


def command_from_desktop(text: str) -> list[str] | None:
    """``Exec=`` when ``X-Voxprint-Protocol`` is 1. ``%`` placeholders are dropped."""
    parsed = _desktop_fields(text)
    if parsed is None:
        return None
    return parsed[0]


def install_from_values(
    *,
    install_path: str,
    exe: str,
    protocol: object,
    version: str,
    ipc_name: str,
    kebab: str,
) -> AppInstall | None:
    """Turn registry or desktop fields into a command, after the exe exists."""
    if not _protocol_ok(protocol) or not exe:
        return None
    path = Path(exe)
    if not path.is_absolute() and install_path:
        path = Path(install_path) / path
    resolved = str(path) if path.is_file() else shutil.which(exe)
    if not resolved:
        return None
    base = ipc_name.strip() or f"voxprint-{kebab}"
    return AppInstall([resolved], version.strip(), base)


def probe_capabilities(install: AppInstall, app: str) -> dict[str, Any] | None:
    """``--capabilities --json``, cached for a day and refreshed when Version changes."""
    cached = load_cached(app, install.version)
    if cached is not None:
        return cached
    try:
        proc = subprocess.run(
            [*install.command, "--capabilities", "--json"],
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
    version = install.version or data.get("version")
    store_cached(app, version if isinstance(version, str) else "", data)
    return data


def load_cached(app: str, registered_version: str) -> dict[str, Any] | None:
    path = _cache_file(app)
    if not path.is_file():
        return None
    try:
        stored = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return None
    if not isinstance(stored, dict):
        return None
    fetched = stored.get("fetched_at")
    body = stored.get("body")
    if not isinstance(fetched, int | float) or not isinstance(body, dict):
        return None
    if time.time() - float(fetched) > _CACHE_SECONDS:
        return None
    cached_version = stored.get("version")
    if registered_version and cached_version != registered_version:
        return None
    return body


def store_cached(app: str, version: str, body: dict[str, Any]) -> None:
    path = _cache_file(app)
    path.parent.mkdir(parents=True, exist_ok=True)
    payload = {"fetched_at": time.time(), "version": version, "body": body}
    path.write_text(json.dumps(payload), encoding="utf-8")


def _cache_file(app: str) -> Path:
    override = os.environ.get("LUCENT_CACHE", "").strip()
    if override:
        root = Path(override)
    else:
        xdg = os.environ.get("XDG_CACHE_HOME", "").strip()
        root = Path(xdg) if xdg else Path.home() / ".cache"
    return root / "voxprint" / f"{app}-capabilities.json"


def _protocol_ok(value: object) -> bool:
    return value == 1 or value == "1"


def _desktop_fields(text: str) -> tuple[list[str], str, str] | None:
    protocol = ""
    version = ""
    ipc = ""
    exec_line = ""
    for line in text.splitlines():
        if line.startswith("X-Voxprint-Protocol="):
            protocol = line.split("=", 1)[1].strip()
        elif line.startswith("X-Voxprint-Version="):
            version = line.split("=", 1)[1].strip()
        elif line.startswith("X-Voxprint-Ipc="):
            ipc = line.split("=", 1)[1].strip()
        elif line.startswith("Exec="):
            exec_line = line.split("=", 1)[1].strip()
    if protocol != "1" or not exec_line:
        return None
    command = [part for part in shlex.split(exec_line) if not part.startswith("%")]
    if not command:
        return None
    return command, version, ipc


def _from_desktop_dirs(pascal: str, kebab: str) -> AppInstall | None:
    roots: list[Path] = []
    data_home = os.environ.get("XDG_DATA_HOME", "").strip()
    roots.append(Path(data_home) if data_home else Path.home() / ".local" / "share")
    data_dirs = os.environ.get("XDG_DATA_DIRS", "/usr/local/share:/usr/share")
    roots.extend(Path(item) for item in data_dirs.split(":") if item)
    name = f"ai.voxprint.{pascal}.desktop"
    for root in roots:
        path = root / "applications" / name
        if not path.is_file():
            continue
        parsed = _desktop_fields(path.read_text(encoding="utf-8"))
        if parsed is None:
            continue
        command, version, ipc = parsed
        if shutil.which(command[0]) is None and not Path(command[0]).is_file():
            continue
        return AppInstall(command, version, ipc or f"voxprint-{kebab}")
    return None


def _from_windows_registry(pascal: str, kebab: str) -> AppInstall | None:
    # winreg is Windows-only. A top-level import fails on Linux.
    import winreg

    try:
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER, rf"Software\Voxprint\{pascal}") as key:
            install_path, _ = _query(key, "InstallPath")
            exe, _ = _query(key, "Exe")
            version, _ = _query(key, "Version")
            ipc_name, _ = _query(key, "IpcName")
            protocol, _ = winreg.QueryValueEx(key, "ProtocolVersion")
    except OSError:
        install_path = exe = version = ipc_name = ""
        protocol = 0
    else:
        found = install_from_values(
            install_path=str(install_path),
            exe=str(exe),
            protocol=protocol,
            version=str(version),
            ipc_name=str(ipc_name),
            kebab=kebab,
        )
        if found is not None:
            return found
    try:
        app_paths = rf"Software\Microsoft\Windows\CurrentVersion\App Paths\voxprint-{kebab}.exe"
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER, app_paths) as key:
            exe, _ = winreg.QueryValueEx(key, "")
    except OSError:
        return None
    return install_from_values(
        install_path="",
        exe=str(exe),
        protocol=1,
        version="",
        ipc_name=f"voxprint-{kebab}",
        kebab=kebab,
    )


def _query(key: object, name: str) -> tuple[object, int]:
    import winreg

    try:
        return winreg.QueryValueEx(key, name)
    except OSError:
        return "", 0


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
