#Requires -Version 5.1
<#
.SYNOPSIS
    Non-interactive Lucent check for Windows.

.DESCRIPTION
    Exit 0 pass, 1 a check failed, 2 setup, 3 no GPU.
    Viewing does not need a GPU. Exit 3 only when -RequireGpu is set and no
    NVIDIA GPU is present. Otherwise a missing GPU is recorded and a passing
    run still exits 0.
    -Smoke is the short pass and is the default. Budget: about 10 minutes.
#>
[CmdletBinding()]
param(
    [switch]$Json,
    [switch]$Smoke,
    [switch]$RequireGpu,
    [string]$Viewer,
    [string]$Image
)

$ErrorActionPreference = 'Stop'
$Utf8NoBom = New-Object System.Text.UTF8Encoding $false
try { [Console]::InputEncoding = $Utf8NoBom } catch { }
try { [Console]::OutputEncoding = $Utf8NoBom } catch { }
$OutputEncoding = $Utf8NoBom

$Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
if (Test-Path -LiteralPath (Join-Path $PSScriptRoot '..\BUILD.json')) {
    $Root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
}

function Write-Result([bool]$Ok, [int]$Code, [string]$Detail, [bool]$GpuPresent) {
    if ($Json) {
        $payload = [ordered]@{
            ok = $Ok
            smoke = $true
            exit = $Code
            detail = $Detail
            gpu = [ordered]@{
                present = $GpuPresent
                required = [bool]$RequireGpu
                code = $(if ($GpuPresent) { 0 } else { 3 })
            }
        }
        $text = ($payload | ConvertTo-Json -Compress -Depth 5)
        [Console]::Out.WriteLine($text)
    } else {
        Write-Host "vm_test: $Detail"
    }
}

$gpuPresent = $false
$nvsmi = Get-Command nvidia-smi -ErrorAction SilentlyContinue
if ($nvsmi) {
    & nvidia-smi -L *> $null
    if ($LASTEXITCODE -eq 0) { $gpuPresent = $true }
}

function Test-ViewerFile([string]$Path) {
    return ($Path -and (Test-Path -LiteralPath $Path))
}

if (-not $Viewer) {
    $candidates = @(
        (Join-Path $Root 'cpp\build-ci\voxprint-lucent.exe'),
        (Join-Path $PSScriptRoot 'voxprint-lucent.exe'),
        (Join-Path (Split-Path -Parent $PSScriptRoot) 'voxprint-lucent.exe')
    )
    if ($env:ProgramFiles) {
        $candidates += (Join-Path $env:ProgramFiles 'VOXPrint AI Suite\Lucent\voxprint-lucent.exe')
    }
    if ($env:LOCALAPPDATA) {
        $candidates += (Join-Path $env:LOCALAPPDATA 'Voxprint\Lucent\voxprint-lucent.exe')
    }
    foreach ($reg in @(
            'HKLM:\Software\VOXPrint\Lucent',
            'HKCU:\Software\VOXPrint\Lucent',
            'HKCU:\Software\Voxprint\Lucent'
        )) {
        try {
            $props = Get-ItemProperty -LiteralPath $reg -ErrorAction Stop
            $install = [string]$props.InstallPath
            $exe = [string]$props.Exe
            if ($install -and $exe) {
                if ([System.IO.Path]::IsPathRooted($exe)) { $candidates += $exe }
                else { $candidates += (Join-Path $install.TrimEnd('\') $exe) }
            } elseif ($install) {
                $candidates += (Join-Path $install.TrimEnd('\') 'voxprint-lucent.exe')
            }
        } catch {
        }
    }
    foreach ($candidate in $candidates) {
        if (Test-ViewerFile $candidate) { $Viewer = $candidate; break }
    }
}
if (-not $Viewer -or -not (Test-Path -LiteralPath $Viewer)) {
    Write-Result $false 2 'viewer missing' $gpuPresent
    exit 2
}
if ($RequireGpu -and -not $gpuPresent) {
    Write-Result $false 3 'no GPU' $gpuPresent
    exit 3
}

$buildPath = Join-Path $PSScriptRoot 'BUILD.json'
if (-not (Test-Path -LiteralPath $buildPath)) {
    $buildPath = Join-Path $Root 'BUILD.json'
}
if (-not (Test-Path -LiteralPath $buildPath)) {
    $buildPath = Join-Path (Split-Path -Parent $Viewer) 'BUILD.json'
}
if (-not (Test-Path -LiteralPath $buildPath)) {
    Write-Result $false 2 'BUILD.json missing' $gpuPresent
    exit 2
}
$build = Get-Content -LiteralPath $buildPath -Raw -Encoding UTF8 | ConvertFrom-Json
$number = [string]$build.number
$codename = [string]$build.codename
if (-not $number -or -not $codename) {
    Write-Result $false 2 'BUILD.json has no number or codename' $gpuPresent
    exit 2
}

if (-not $Image) {
    $imageCandidates = @(
        (Join-Path (Split-Path -Parent $Viewer) 'images\smoke.jpg'),
        (Join-Path $PSScriptRoot 'images\smoke.jpg'),
        (Join-Path $Root 'images\smoke.jpg'),
        (Join-Path $Root 'packaging\windows\smoke.jpg'),
        (Join-Path $PSScriptRoot 'smoke.jpg')
    )
    foreach ($candidate in $imageCandidates) {
        if (Test-Path -LiteralPath $candidate) { $Image = $candidate; break }
    }
}
if (-not $Image -or -not (Test-Path -LiteralPath $Image)) {
    Write-Result $false 2 'smoke image missing' $gpuPresent
    exit 2
}

function Invoke-Viewer([string[]]$ArgumentList) {
    $timeoutMs = 120000
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $Viewer
    $quoted = @(foreach ($part in $ArgumentList) {
        if ($part -match '[\s"]') { '"' + ($part.Replace('"', '\"')) + '"' } else { $part }
    })
    $psi.Arguments = ($quoted -join ' ')
    $psi.UseShellExecute = $false
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.StandardOutputEncoding = $Utf8NoBom
    $psi.StandardErrorEncoding = $Utf8NoBom
    $proc = New-Object System.Diagnostics.Process
    $proc.StartInfo = $psi
    [void]$proc.Start()
    $outTask = $proc.StandardOutput.ReadToEndAsync()
    $errTask = $proc.StandardError.ReadToEndAsync()
    if (-not $proc.WaitForExit($timeoutMs)) {
        try { $proc.Kill() } catch { }
        try { $proc.WaitForExit(5000) | Out-Null } catch { }
        $joined = ($ArgumentList -join ' ')
        return "TIMEOUT after ${timeoutMs} ms: $joined"
    }
    try { $null = $errTask.Result } catch { }
    return [string]$outTask.Result
}

$version = Invoke-Viewer @('--version')
if ($version -match '^TIMEOUT') {
    Write-Result $false 1 $version $gpuPresent
    exit 1
}
if ($version -notmatch [regex]::Escape("build $number") -or $version -notmatch [regex]::Escape($codename)) {
    Write-Result $false 1 'version' $gpuPresent
    exit 1
}
$selftest = Invoke-Viewer @('--selftest', '--json')
if ($selftest -match '^TIMEOUT') {
    Write-Result $false 1 $selftest $gpuPresent
    exit 1
}
if ($selftest -notmatch '"ok"\s*:\s*true') {
    Write-Result $false 1 'selftest' $gpuPresent
    exit 1
}
$dry = Invoke-Viewer @('--dry-run', $Image)
if ($dry -match '^TIMEOUT') {
    Write-Result $false 1 $dry $gpuPresent
    exit 1
}
if ($dry -notmatch '"dry_run"\s*:\s*true') {
    Write-Result $false 1 'dry-run' $gpuPresent
    exit 1
}
$bench = Invoke-Viewer @($Image, '--benchmark')
if ($bench -match '^TIMEOUT') {
    Write-Result $false 1 $bench $gpuPresent
    exit 1
}
if ($bench -notmatch '(?m)^FIRST_PIXEL\s*$') {
    Write-Result $false 1 'first pixel' $gpuPresent
    exit 1
}
Write-Result $true 0 'pass' $gpuPresent
exit 0
