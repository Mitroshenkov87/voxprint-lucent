"""The release MSI must register Lucent the way the suite discovery spec says."""

from __future__ import annotations

from pathlib import Path

WXS = Path("packaging/windows/Package.release.wxs")


def test_release_msi_writes_app_discovery_values() -> None:
    """HKCU Lucent values and App Paths are both in the shortcut component."""
    text = WXS.read_text(encoding="utf-8")
    assert 'Name="installed"' not in text
    for name in ("InstallPath", "Exe", "Version", "IpcName", "ProtocolVersion"):
        assert f'Name="{name}"' in text
    assert 'Key="Software\\Voxprint\\Lucent"' in text
    assert 'Value="[INSTALLFOLDER]voxprint-lucent.exe"' in text
    assert 'Value="voxprint-lucent"' in text
    assert 'Name="ProtocolVersion"' in text
    assert 'Type="integer"' in text
    assert 'Value="1"' in text
    app_paths = r"Software\Microsoft\Windows\CurrentVersion\App Paths\voxprint-lucent.exe"
    assert app_paths in text
    assert 'Name="Path"' in text
