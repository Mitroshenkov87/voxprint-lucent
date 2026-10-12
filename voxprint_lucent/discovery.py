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
    """Registry or desktop entry, App Paths, the shared manifest, the installer path, then PATH.

    The executable must exist. Flatpak is not planned. On Windows, HKLM
    ``Software\\VOXPrint\\<App>`` is first, then HKCU, then App Paths, then
    ``Program Files\\VOXPrint AI Suite``. The older Enlarge installer writes
    ``VoxprintEnlarge.exe``.
    """
    if sys.platform == "win32":
        found = _from_windows_registry(pascal, kebab)
    else:
        found = _from_desktop_dirs(pascal, kebab)
    if found is not None:
        return found
    found = _from_shared_manifest(pascal, kebab)
    if found is not None:
        return found
    found = _from_known_install(kebab)
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
    """Return a cached capabilities document when the version is still current."""
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
    """Write a capabilities document for this app version."""
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


def _shared_dir() -> Path:
    """``VOXPRINT_HOME/shared``, else the suite folder for this platform."""
    env = os.environ.get("VOXPRINT_HOME", "").strip()
    if env:
        return Path(env) / "shared"
    if sys.platform == "win32":
        local = os.environ.get("LOCALAPPDATA", "").strip()
        root = Path(local) if local else Path.home() / "AppData" / "Local"
        return root / "Voxprint" / "shared"
    data = os.environ.get("XDG_DATA_HOME", "").strip()
    base = Path(data) if data else Path.home() / ".local" / "share"
    return base / "voxprint" / "shared"


def _known_install_exe(kebab: str) -> Path:
    """Path the per-user installer uses. Enlarge's Windows file is ``VoxprintEnlarge.exe``."""
    home = _shared_dir().parent
    if sys.platform == "win32" and kebab == "enlarge":
        return home / "Enlarge" / "VoxprintEnlarge.exe"
    if sys.platform == "win32":
        folder = "".join(part.capitalize() for part in kebab.split("-"))
        return home / folder / f"voxprint-{kebab}.exe"
    return home / kebab / f"voxprint-{kebab}"


def _install_from_app_entry(entry: object, kebab: str) -> AppInstall | None:
    if isinstance(entry, str):
        return install_from_values(
            install_path="",
            exe=entry,
            protocol=1,
            version="",
            ipc_name=f"voxprint-{kebab}",
            kebab=kebab,
        )
    if not isinstance(entry, dict):
        return None
    exe = entry.get("exe", entry.get("Exe", ""))
    protocol = entry.get("protocol", entry.get("ProtocolVersion", 1))
    version = entry.get("version", entry.get("Version", ""))
    ipc_name = entry.get("ipc", entry.get("IpcName", ""))
    install_path = entry.get("install_path", entry.get("InstallPath", ""))
    return install_from_values(
        install_path=str(install_path or ""),
        exe=str(exe or ""),
        protocol=protocol,
        version=str(version or ""),
        ipc_name=str(ipc_name or ""),
        kebab=kebab,
    )


def _manifest_holds(data: dict[str, Any], kebab: str) -> bool:
    components = data.get("components")
    if not isinstance(components, dict):
        return False
    for value in components.values():
        if not isinstance(value, dict):
            continue
        holders = value.get("holders")
        if isinstance(holders, list) and kebab in {str(item) for item in holders}:
            return True
    return False


def _from_shared_manifest(pascal: str, kebab: str) -> AppInstall | None:
    """``manifest.json`` in the shared suite folder. Schema 1, as Enlarge writes it.

    An ``apps`` entry wins. Otherwise a holder named ``kebab`` selects the
    installer path when that file exists.
    """
    path = _shared_dir() / "manifest.json"
    try:
        data = json.loads(path.read_text(encoding="utf-8-sig"))
    except (OSError, json.JSONDecodeError):
        return None
    if not isinstance(data, dict):
        return None
    apps = data.get("apps")
    if isinstance(apps, dict):
        entry = apps.get(kebab)
        if entry is None:
            entry = apps.get(pascal)
        found = _install_from_app_entry(entry, kebab)
        if found is not None:
            return found
    if _manifest_holds(data, kebab):
        return _from_known_install(kebab)
    return None


def _from_known_install(kebab: str) -> AppInstall | None:
    known = _known_install_exe(kebab)
    if not known.is_file():
        return None
    return AppInstall([str(known.resolve())], "", f"voxprint-{kebab}")


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


def _read_app_key(hive: int, subkey: str, kebab: str) -> AppInstall | None:
    # winreg is Windows-only. A top-level import fails on Linux.
    import winreg

    try:
        key = winreg.OpenKey(hive, subkey)
    except OSError:
        return None
    with key:
        install_path, _ = _query(key, "InstallPath")
        exe, _ = _query(key, "Exe")
        version, _ = _query(key, "Version")
        ipc_name, _ = _query(key, "IpcName")
        try:
            protocol, _ = winreg.QueryValueEx(key, "ProtocolVersion")
        except OSError:
            protocol = 0
    return install_from_values(
        install_path=str(install_path),
        exe=str(exe),
        protocol=protocol,
        version=str(version),
        ipc_name=str(ipc_name),
        kebab=kebab,
    )


def _from_windows_registry(pascal: str, kebab: str) -> AppInstall | None:
    # winreg is Windows-only. A top-level import fails on Linux.
    import winreg

    for hive, subkey in (
        (winreg.HKEY_LOCAL_MACHINE, rf"Software\VOXPrint\{pascal}"),
        (winreg.HKEY_CURRENT_USER, rf"Software\VOXPrint\{pascal}"),
        (winreg.HKEY_CURRENT_USER, rf"Software\Voxprint\{pascal}"),
    ):
        found = _read_app_key(hive, subkey, kebab)
        if found is not None:
            return found
    app_paths = rf"Software\Microsoft\Windows\CurrentVersion\App Paths\voxprint-{kebab}.exe"
    for hive in (winreg.HKEY_LOCAL_MACHINE, winreg.HKEY_CURRENT_USER):
        try:
            with winreg.OpenKey(hive, app_paths) as key:
                exe, _ = winreg.QueryValueEx(key, "")
        except OSError:
            continue
        found = install_from_values(
            install_path="",
            exe=str(exe),
            protocol=1,
            version="",
            ipc_name=f"voxprint-{kebab}",
            kebab=kebab,
        )
        if found is not None:
            return found
    names = [f"voxprint-{kebab}.exe"]
    if kebab == "enlarge":
        names.append("VoxprintEnlarge.exe")
    program_files = os.environ.get("ProgramFiles", r"C:\Program Files")
    local = os.environ.get("LOCALAPPDATA", "").strip()
    candidates = [Path(program_files) / "VOXPrint AI Suite" / pascal / name for name in names]
    if local:
        candidates.extend(Path(local) / "Programs" / "Voxprint" / pascal / name for name in names)
    for candidate in candidates:
        if candidate.is_file():
            return AppInstall([str(candidate.resolve())], "", f"voxprint-{kebab}")
    return None


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
