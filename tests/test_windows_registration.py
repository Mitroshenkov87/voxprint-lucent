"""The release MSI must register Lucent the way the suite discovery spec says."""

from __future__ import annotations

from pathlib import Path

WXS = Path("packaging/windows/Package.release.wxs")
RUN_M0 = Path("bench/Run-M0.ps1")
VM_TEST = Path("scripts/vm_test.ps1")


def test_release_msi_writes_app_discovery_values() -> None:
    """HKLM Lucent values, both App Paths keys, and the per-machine layout."""
    text = WXS.read_text(encoding="utf-8")
    assert 'Scope="perMachine"' in text
    assert 'Manufacturer="VOXPrint AI Suite"' in text
    assert 'Name="VOXPrint AI Lucent"' in text
    assert 'Id="ProgramFiles64Folder"' in text
    assert 'Name="VOXPrint AI Suite"' in text
    assert 'Name="installed"' not in text
    for name in ("InstallPath", "Exe", "CliExe", "Version", "Build", "IpcName", "ProtocolVersion"):
        assert f'Name="{name}"' in text
    assert 'Root="HKLM"' in text
    assert 'Key="Software\\VOXPrint\\Lucent"' in text
    assert 'Value="voxprint-lucent.exe"' in text
    assert 'Value="voxprint-lucent-cli.exe"' in text
    assert 'Value="voxprint-lucent"' in text
    assert 'Type="integer"' in text
    assert 'Value="1"' in text
    gui = r"Software\Microsoft\Windows\CurrentVersion\App Paths\voxprint-lucent.exe"
    cli = r"Software\Microsoft\Windows\CurrentVersion\App Paths\voxprint-lucent-cli.exe"
    assert gui in text
    assert cli in text
    assert 'Name="Path"' in text


def test_cold_task_unregisters_on_failure() -> None:
    text = RUN_M0.read_text(encoding="utf-8")
    assert "DeleteExpiredTaskAfter" in text
    assert "outcome=deferred-helper" in text
    assert "outcome=failed" in text
    assert "schtasks.exe" in text
    assert "outcome=left-registered" not in text


def test_vm_test_finds_an_msi_and_times_out() -> None:
    text = VM_TEST.read_text(encoding="utf-8")
    assert r"VOXPrint AI Suite\Lucent" in text
    assert r"HKLM:\Software\VOXPrint\Lucent" in text
    assert r"Voxprint\Lucent" in text
    assert "WaitForExit" in text
    assert "TIMEOUT after" in text
    assert "images\\smoke.jpg" in text
    assert "packaging\\windows\\smoke.jpg" in text
