"""The release MSI must register Lucent the way the suite discovery spec says."""

from __future__ import annotations

from pathlib import Path

WXS = Path("packaging/windows/Package.release.wxs")
RUN_M0 = Path("bench/Run-M0.ps1")
VM_TEST = Path("scripts/vm_test.ps1")


def test_release_files_use_lower_case_names() -> None:
    windows = Path("packaging/windows/build-release.ps1").read_text(encoding="utf-8")
    assert "voxprint-lucent-setup-$VersionName-win-x64.msi" in windows
    assert "voxprint-lucent-$VersionName-win-x64.zip" in windows
    assert "Voxprint" not in windows
    linux = Path("packaging/linux/build-release.sh").read_text(encoding="utf-8")
    assert "voxprint-lucent_${FILE_VERSION}_amd64.deb" in linux
    workflow = Path(".github/workflows/release.yml").read_text(encoding="utf-8")
    assert "dist/voxprint-lucent-*-win-x64.zip" in workflow
    assert "voxprint-lucent-setup-<version>-win-x64.msi" in VM_TEST.read_text(encoding="utf-8")
    assert "voxprint-lucent-<version>-win-x64.zip" in RUN_M0.read_text(encoding="utf-8")


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


def test_v01_gates_are_recorded() -> None:
    m0 = RUN_M0.read_text(encoding="utf-8")
    assert "$ColdBudgetMs = 450.0" in m0
    assert "$BudgetMs = 250.0" in m0
    assert "$FullResBudgetMs = 350.0" in m0
    smoke = Path("packaging/linux/build-cpp-smoke.sh").read_text(encoding="utf-8")
    assert "WARM_FIRST_PIXEL_MS=250" in smoke
    assert "cold first pixel <= 450 ms" in smoke
    assert "GITHUB_STEP_SUMMARY" in smoke
    assert "report only" in smoke
    timing = smoke.split("WARM_FIRST_PIXEL_MS=250", 1)[1].split("# After the timed launch", 1)[0]
    assert "exit 1" in timing
    over = timing.split('-gt "$WARM_FIRST_PIXEL_MS"', 1)[1]
    assert "exit 1" not in over


def test_cold_task_unregisters_on_failure() -> None:
    text = RUN_M0.read_text(encoding="utf-8")
    assert "DeleteExpiredTaskAfter" in text
    assert "outcome=deferred-helper" in text
    assert "outcome=failed" in text
    assert "schtasks.exe" in text
    assert "outcome=left-registered" not in text


def test_viewer_spawns_legacy_uninstall_without_waiting() -> None:
    text = Path("cpp/src/main.cpp").read_text(encoding="utf-8")
    assert "spawnLegacyPerUserUninstall" in text
    assert "msiexec.exe /x " in text
    assert "Voxprint AI Lucent" in text
    assert "legacy-uninstall.log" in text
    assert "DETACHED_PROCESS" in text
    assert "CREATE_NO_WINDOW" in text
    assert ".detach()" in text
    assert "WaitForSingleObject" not in text
    interactive = text.split("lucent::Browser browser", 1)[1]
    before_show, after_show = interactive.split("browser.show();", 1)
    assert "spawnLegacyPerUserUninstall" not in before_show
    assert "spawnLegacyPerUserUninstall();" in after_show
    benchmark = text.split("if (options.benchmark)", 1)[1].split("lucent::Browser browser", 1)[0]
    assert "spawnLegacyPerUserUninstall" not in benchmark


def test_vm_test_finds_an_msi_and_times_out() -> None:
    text = VM_TEST.read_text(encoding="utf-8")
    assert r"VOXPrint AI Suite\Lucent" in text
    assert r"HKLM:\Software\VOXPrint\Lucent" in text
    assert r"Voxprint\Lucent" in text
    assert "WaitForExit" in text
    assert "TIMEOUT after" in text
    assert "images\\smoke.jpg" in text
    assert "packaging\\windows\\smoke.jpg" in text
