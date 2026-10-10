#!/usr/bin/env python3
"""Download the Qt 6.12.0 MSVC libraries the Windows harness links against.

aqtinstall 3.3.0 asks for
online/qtsdkrepository/windows_x86/desktop/qt6_6120/qt6_6120/Updates.xml.
That file is not on the feed. The Windows desktop packages live in
qt6_6120_msvc2022_64. Linux desktop still uses qt6_6120/qt6_6120, so the
Ubuntu smoke job can keep using aqtinstall.
"""

from __future__ import annotations

import sys
import urllib.request
import xml.etree.ElementTree as ET
from pathlib import Path

import py7zr

HOST = "https://download.qt.io"
FOLDER = "online/qtsdkrepository/windows_x86/desktop/qt6_6120/qt6_6120_msvc2022_64"
WANTED_PREFIXES = ("qtbase-", "qtimageformats-")


def _download(url: str, dest: Path) -> None:
    print(f"GET {url}")
    with urllib.request.urlopen(url, timeout=180) as response, dest.open("wb") as handle:
        while True:
            chunk = response.read(1024 * 1024)
            if not chunk:
                break
            handle.write(chunk)


def main(argv: list[str]) -> int:
    if len(argv) != 2:
        print("usage: fetch_qt.py DEST_PREFIX", file=sys.stderr)
        return 2
    dest = Path(argv[1])
    dest.mkdir(parents=True, exist_ok=True)
    cache = dest.parent / "qt-archives"
    cache.mkdir(parents=True, exist_ok=True)
    xml_path = cache / "Updates.xml"
    _download(f"{HOST}/{FOLDER}/Updates.xml", xml_path)
    root = ET.fromstring(xml_path.read_bytes())
    extracted = 0
    for package in root.iter("PackageUpdate"):
        name = package.findtext("Name") or ""
        if not name.endswith("win64_msvc2022_64") or "debug" in name:
            continue
        version = package.findtext("Version") or ""
        archives = package.findtext("DownloadableArchives") or ""
        for archive in [part.strip() for part in archives.split(",") if part.strip()]:
            if not archive.startswith(WANTED_PREFIXES) or "debug" in archive:
                continue
            blob = cache / archive
            if not blob.is_file() or blob.stat().st_size == 0:
                _download(f"{HOST}/{FOLDER}/{name}/{version}{archive}", blob)
            print(f"extract {archive} -> {dest}")
            with py7zr.SevenZipFile(blob, mode="r") as archive_file:
                archive_file.extractall(path=dest)
            extracted += 1
    if extracted != 2:
        print(f"expected qtbase and qtimageformats, extracted {extracted}", file=sys.stderr)
        return 1
    required = (
        dest / "bin" / "windeployqt.exe",
        dest / "bin" / "Qt6Widgets.dll",
        dest / "plugins" / "imageformats" / "qwebp.dll",
        dest / "plugins" / "platforms" / "qwindows.dll",
    )
    missing = [str(path) for path in required if not path.is_file()]
    if missing:
        print("Qt layout is incomplete:\n" + "\n".join(missing), file=sys.stderr)
        return 1
    print(f"Qt 6.12.0 ready at {dest}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
