#Requires -Version 5.1
<#
.SYNOPSIS
    Cheap headless checks for an unpacked measurement zip.

.DESCRIPTION
    Writes one results\checks.json. Checks --version, Enlarge
    --capabilities --json, the Enlarge --dry-run hand-off, and
    --benchmark on each file in images\hard.
#>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$Utf8NoBom = New-Object System.Text.UTF8Encoding $false
try { [Console]::InputEncoding = $Utf8NoBom } catch { }
try { [Console]::OutputEncoding = $Utf8NoBom } catch { }
$OutputEncoding = $Utf8NoBom
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$Exe = Join-Path $Root 'voxprint-lucent.exe'
$Python = Join-Path $Root 'python\python.exe'
$Images = Join-Path $Root 'images\hard'
$OutDir = Join-Path $Root 'results'
if (-not (Test-Path -LiteralPath $Exe)) { throw "Missing $Exe" }
if (-not (Test-Path -LiteralPath $Python)) { throw "Missing $Python" }
if (-not (Test-Path -LiteralPath $Images)) { throw "Missing $Images" }
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$env:QT_QPA_PLATFORM = 'offscreen'
$env:LUCENT_ENLARGE_GPU = 'ok'
$env:LUCENT_ENLARGE_EXE = '"' + $Python + '" -m voxprint_lucent.enlarge_mock'

function Invoke-Capture {
    param(
        [string]$FileName,
        [string[]]$ArgumentList,
        [int]$TimeoutMs = 180000
    )
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $FileName
    $quoted = @(foreach ($part in $ArgumentList) {
        if ($part -match '[\s"]') { '"' + ($part.Replace('"', '\"')) + '"' } else { $part }
    })
    $psi.Arguments = ($quoted -join ' ')
    $psi.WorkingDirectory = $Root
    $psi.UseShellExecute = $false
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.StandardOutputEncoding = $Utf8NoBom
    $psi.StandardErrorEncoding = $Utf8NoBom
    $proc = New-Object System.Diagnostics.Process
    $proc.StartInfo = $psi
    [void]$proc.Start()
    $stdoutTask = $proc.StandardOutput.ReadToEndAsync()
    $stderrTask = $proc.StandardError.ReadToEndAsync()
    $finished = $proc.WaitForExit($TimeoutMs)
    if (-not $finished) {
        try { $proc.Kill() } catch { }
        return @{ exit = 1; stdout = ''; stderr = 'timed out' }
    }
    return @{
        exit = [int]$proc.ExitCode
        stdout = [string]$stdoutTask.Result
        stderr = [string]$stderrTask.Result
    }
}

$failures = New-Object System.Collections.Generic.List[string]
$version = Invoke-Capture $Exe @('--version') 30000
$label = $version.stdout
if ($version.exit -ne 0 -or $label -notmatch '0\.1\.0 RC' -or $label -notmatch '"Orah"') {
    $failures.Add('version')
}
$capabilities = Invoke-Capture $Python @('-m', 'voxprint_lucent.enlarge_mock', '--capabilities', '--json') 60000
if ($capabilities.exit -ne 0 -or $capabilities.stdout -notmatch '"protocol"\s*:\s*1') {
    $failures.Add('capabilities')
}
# animated.webp sorts first and is a multi-frame file. The dry-run needs a still.
$sample = Get-ChildItem -LiteralPath $Images -File |
    Where-Object { $_.Name -notlike 'animated.*' } |
    Sort-Object Name |
    Select-Object -First 1
if (-not $sample) { throw "No files in $Images" }
$dry = Invoke-Capture $Exe @('--dry-run', $sample.FullName) 60000
if ($dry.exit -ne 0 -or $dry.stdout -notmatch '"dry_run"\s*:\s*true') {
    $failures.Add('dry-run')
}
$opened = @()
Get-ChildItem -LiteralPath $Images -File | Sort-Object Name | ForEach-Object {
    $run = Invoke-Capture $Exe @($_.FullName, '--benchmark')
    $ok = $run.exit -eq 0 -and $run.stdout -match '(?m)^FIRST_PIXEL\s*$'
    if (-not $ok) { $failures.Add($_.Name) }
    $opened += [ordered]@{
        name = $_.Name
        exit = $run.exit
        first_pixel = [bool]$ok
    }
}
$report = [ordered]@{
    version = $version.stdout.Trim()
    capabilities_exit = $capabilities.exit
    dry_run_exit = $dry.exit
    dry_run_image = $sample.Name
    images = $opened
    failures = @($failures)
}
$destination = Join-Path $OutDir 'checks.json'
[System.IO.File]::WriteAllText($destination, (($report | ConvertTo-Json -Depth 5) + "`n"), $Utf8NoBom)
Write-Host "Wrote $destination"
if ($failures.Count -gt 0) {
    throw ("Checks failed: " + ($failures -join ', '))
}
