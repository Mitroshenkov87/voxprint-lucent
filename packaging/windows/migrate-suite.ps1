# Move the suite data home, remove the old per-user Lucent install, and
# optionally install PowerShell 7. Runs from the MSI before any app Python.
# Windows PowerShell 5.1 can run this file. Later scripts prefer pwsh 7 and
# fall back to 5.1 when it is absent.
param(
    [string]$InstallPwsh = '0',
    [string]$Language = 'en'
)

$ErrorActionPreference = 'Stop'

function Move-SuiteHome {
    $local = $env:LOCALAPPDATA
    if (-not $local) { return }
    $source = Join-Path $local 'Voxprint'
    $dest = Join-Path $local 'VOXPrint AI Suite'
    if (-not (Test-Path -LiteralPath $source)) { return }
    if (-not (Test-Path -LiteralPath $dest)) {
        New-Item -ItemType Directory -Force -Path $dest | Out-Null
        Get-ChildItem -LiteralPath $source -Force | Where-Object { $_.Name -ne 'Lucent' } | ForEach-Object {
            $target = Join-Path $dest $_.Name
            if (-not (Test-Path -LiteralPath $target)) {
                Move-Item -LiteralPath $_.FullName -Destination $target
            }
        }
    }
}

function Remove-LegacyLucent {
    $root = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall'
    if (-not (Test-Path -LiteralPath $root)) { return }
    Get-ChildItem -LiteralPath $root -ErrorAction SilentlyContinue | ForEach-Object {
        $props = Get-ItemProperty -LiteralPath $_.PSPath -ErrorAction SilentlyContinue
        if (-not $props) { return }
        $name = [string]$props.DisplayName
        $location = [string]$props.InstallLocation
        $oldName = $name -eq 'Voxprint AI Lucent'
        $oldDir = $location -and ($location.TrimEnd('\') -match '\\Voxprint\\Lucent$')
        if (-not $oldName -and -not $oldDir) { return }
        $id = $_.PSChildName
        if ($id -notmatch '^\{[0-9A-Fa-f-]+\}$') { return }
        $proc = Start-Process -FilePath "$env:SystemRoot\System32\msiexec.exe" -ArgumentList @('/x', $id, '/qn', '/norestart') -Wait -PassThru
        if ($proc.ExitCode -ne 0 -and $proc.ExitCode -ne 1605 -and $proc.ExitCode -ne 3010) {
            Write-Error "The old Lucent install could not be removed (msiexec $($proc.ExitCode)). Settings were left in place."
        }
    }
}

function Set-SuiteLanguage([string]$Code) {
    $allowed = @('en', 'de', 'uk', 'lv')
    if ($allowed -notcontains $Code) { $Code = 'en' }
    $local = $env:LOCALAPPDATA
    if (-not $local) { return }
    $dest = Join-Path $local 'VOXPrint AI Suite'
    $state = Join-Path $dest 'state'
    New-Item -ItemType Directory -Force -Path $state | Out-Null
    $file = Join-Path $state 'suite.json'
    $doc = @{}
    if (Test-Path -LiteralPath $file) {
        try {
            $parsed = Get-Content -LiteralPath $file -Raw -Encoding UTF8 | ConvertFrom-Json
            if ($parsed) {
                $parsed.PSObject.Properties | ForEach-Object { $doc[$_.Name] = $_.Value }
            }
        } catch {
            $doc = @{}
        }
    }
    $doc['ui_language'] = $Code
    ($doc | ConvertTo-Json -Depth 8) | Set-Content -LiteralPath $file -Encoding UTF8
}

function Install-PowerShell7 {
    $existing = Get-Command pwsh -ErrorAction SilentlyContinue
    if ($existing) {
        $version = & pwsh -NoProfile -Command '$PSVersionTable.PSVersion.Major'
        if ([int]$version -ge 7) { return }
    }
    $msi = Join-Path $env:TEMP 'PowerShell-7.6.9-win-x64.msi'
    $url = 'https://github.com/PowerShell/PowerShell/releases/download/v7.6.9/PowerShell-7.6.9-win-x64.msi'
    try {
        Invoke-WebRequest -Uri $url -OutFile $msi -UseBasicParsing
        $proc = Start-Process -FilePath "$env:SystemRoot\System32\msiexec.exe" -ArgumentList @('/i', $msi, '/qn', '/norestart') -Wait -PassThru
        if ($proc.ExitCode -ne 0 -and $proc.ExitCode -ne 3010) {
            Write-Warning "PowerShell 7 did not install (msiexec $($proc.ExitCode)). Scripts will use Windows PowerShell 5.1."
        }
    } catch {
        Write-Warning "PowerShell 7 could not be downloaded. Scripts will use Windows PowerShell 5.1."
    }
}

Move-SuiteHome
Remove-LegacyLucent
if ($Language) { Set-SuiteLanguage $Language }
if ($InstallPwsh -eq '1') { Install-PowerShell7 }
exit 0
