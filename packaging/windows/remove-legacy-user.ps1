# Manual fallback. The installed viewer does this once, detached, on first launch.
# Uninstall the v0.1.0-rc6 per-user MSI. Settings under %LOCALAPPDATA%\Voxprint stay.
$ErrorActionPreference = 'Stop'
$root = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall'
if (-not (Test-Path -LiteralPath $root)) { exit 0 }
Get-ChildItem -LiteralPath $root -ErrorAction SilentlyContinue | ForEach-Object {
    $props = Get-ItemProperty -LiteralPath $_.PSPath -ErrorAction SilentlyContinue
    if (-not $props) { return }
    if ([string]$props.DisplayName -ne 'Voxprint AI Lucent') { return }
    $id = $_.PSChildName
    if ($id -notmatch '^\{[0-9A-Fa-f-]+\}$') { return }
    $proc = Start-Process -FilePath "$env:SystemRoot\System32\msiexec.exe" -ArgumentList @('/x', $id, '/qn', '/norestart') -Wait -PassThru
    if ($proc.ExitCode -ne 0 -and $proc.ExitCode -ne 1605) {
        Write-Error "msiexec /x $id exited $($proc.ExitCode)"
    }
}
