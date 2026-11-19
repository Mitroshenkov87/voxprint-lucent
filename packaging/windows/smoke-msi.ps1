# Smoke-test the Lucent MSI on a Windows runner without a display.
#   pwsh -File packaging/windows/smoke-msi.ps1 -Msi dist\voxprint-lucent-setup-<version>-win-x64.msi
# Installs silently, opens WebP, AVIF, JPEG XL and JPEG offscreen (FIRST_PIXEL),
# keeps the viewer window up for a few seconds, checks the selftest, the Python
# helper, the suite registry row and the HKLM discovery values, then uninstalls.
param([Parameter(Mandatory = $true)][string]$Msi)
$ErrorActionPreference = 'Stop'

function Invoke-Viewer {
    param([string]$Exe, [string]$Arguments, [int]$TimeoutMs = 60000)
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $Exe
    $psi.Arguments = $Arguments
    $psi.UseShellExecute = $false
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $proc = [System.Diagnostics.Process]::Start($psi)
    $out = $proc.StandardOutput.ReadToEndAsync()
    $err = $proc.StandardError.ReadToEndAsync()
    if (-not $proc.WaitForExit($TimeoutMs)) {
        try { $proc.Kill() } catch { }
        throw "TIMEOUT after $TimeoutMs ms: $Exe $Arguments"
    }
    return @{ Code = $proc.ExitCode; Out = [string]$out.Result; Err = [string]$err.Result }
}

$msiPath = (Resolve-Path $Msi).Path
$log = Join-Path $env:RUNNER_TEMP 'lucent-install.log'
$proc = Start-Process msiexec.exe -ArgumentList @('/i', "`"$msiPath`"", '/qn', '/norestart', '/l*v', "`"$log`"") -Wait -PassThru
if ($proc.ExitCode -ne 0) {
    Get-Content $log -Tail 80
    throw "msiexec install exit $($proc.ExitCode)"
}
Write-Host 'ok: silent install'

$App = Join-Path $env:ProgramFiles 'VOXPrint AI Suite\Lucent'
$Exe = Join-Path $App 'voxprint-lucent.exe'
$Py = Join-Path $App 'python\python.exe'
foreach ($path in @($Exe, (Join-Path $App 'voxprint-lucent-cli.exe'), $Py, (Join-Path $App 'imageformats\kimg_avif.dll'), (Join-Path $App 'imageformats\kimg_jxl.dll'))) {
    if (-not (Test-Path -LiteralPath $path)) { throw "missing after install: $path" }
}
$reg = Get-ItemProperty -LiteralPath 'HKLM:\Software\VOXPrint\Lucent'
if ($reg.Exe -ne 'voxprint-lucent.exe') { throw 'HKLM Software\VOXPrint\Lucent Exe is wrong' }
Write-Host "ok: installed $($reg.Version) build $($reg.Build) at $App"

$Images = Join-Path $env:RUNNER_TEMP 'lucent-smoke'
New-Item -ItemType Directory -Force -Path $Images | Out-Null
$script = @"
from pathlib import Path
import imagecodecs, numpy as np
root = Path(r'$Images')
img = np.zeros((96, 128, 3), np.uint8)
img[:, :, 0] = 200
img[:, 64:, 1] = 180
root.joinpath('sample.webp').write_bytes(bytes(imagecodecs.webp_encode(img, level=80)))
root.joinpath('sample.avif').write_bytes(bytes(imagecodecs.avif_encode(img, level=60, speed=8)))
root.joinpath('sample.jxl').write_bytes(bytes(imagecodecs.jpegxl_encode(img, level=90, effort=3)))
root.joinpath('sample.jpg').write_bytes(bytes(imagecodecs.jpeg_encode(img, level=90)))
"@
& $Py -c $script
if ($LASTEXITCODE -ne 0) { throw 'sample generation failed' }

$env:QT_QPA_PLATFORM = 'offscreen'
foreach ($name in @('sample.webp', 'sample.avif', 'sample.jxl', 'sample.jpg')) {
    $file = Join-Path $Images $name
    $r = Invoke-Viewer $Exe ('"{0}" --benchmark' -f $file)
    if ($r.Code -ne 0 -or $r.Out -notmatch '(?m)^FIRST_PIXEL\s*$') {
        throw "FAIL: $name exit $($r.Code) stdout=[$($r.Out)] stderr=[$($r.Err)]"
    }
    Write-Host "ok: $name FIRST_PIXEL"
}

foreach ($name in @('sample.avif', 'sample.jxl')) {
    $p = Start-Process -FilePath $Exe -ArgumentList ('"{0}"' -f (Join-Path $Images $name)) -PassThru
    Start-Sleep -Seconds 6
    if ($p.HasExited) { throw "FAIL: viewer window for $name exited with $($p.ExitCode)" }
    Stop-Process -Id $p.Id -Force
    Write-Host "ok: viewer window stayed up with $name"
}

$r = Invoke-Viewer $Exe '--selftest --json'
$self = $r.Out | ConvertFrom-Json
foreach ($name in @('jpeg', 'webp', 'avif', 'jpegxl')) {
    $check = $self.checks | Where-Object { $_.name -eq $name } | Select-Object -First 1
    if (-not $check -or -not $check.ok) { throw "FAIL: selftest $name not ok: $($r.Out)" }
}
Write-Host 'ok: selftest jpeg webp avif jpegxl'

$r = Invoke-Viewer $Exe ('--dry-run "{0}"' -f (Join-Path $Images 'sample.webp'))
if ($r.Out -notmatch '"installed":false') { throw "FAIL: dry-run without Enlarge: $($r.Out)" }
Write-Host 'ok: Enlarge absent, viewer still works'

& $Py -m voxprint_lucent.tools_host --ping
if ($LASTEXITCODE -ne 0) { throw 'FAIL: tools helper ping' }
Write-Host 'ok: tools helper'

$registry = Join-Path $env:LOCALAPPDATA 'VOXPrint AI Suite\state\installed-apps.json'
if (-not (Test-Path -LiteralPath $registry)) { throw "FAIL: $registry was not written by the installer" }
$rows = @((Get-Content -LiteralPath $registry -Raw | ConvertFrom-Json).apps | Where-Object { $_.id -eq 'lucent' })
if ($rows.Count -ne 1) { throw "FAIL: lucent row missing in $registry" }
Write-Host "ok: registered in installed-apps.json ($($rows[0].path))"

$proc = Start-Process msiexec.exe -ArgumentList @('/x', "`"$msiPath`"", '/qn', '/norestart') -Wait -PassThru
if ($proc.ExitCode -ne 0) { throw "msiexec uninstall exit $($proc.ExitCode)" }
if (Test-Path -LiteralPath $Exe) { throw 'FAIL: viewer still present after uninstall' }
Write-Host 'ok: silent uninstall'
Write-Host 'SMOKE PASSED'
