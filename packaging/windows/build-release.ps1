# Build the per-user MSI from a harness stage that is already on disk.
# Run packaging/windows/build-m0-harness.ps1 first.
$ErrorActionPreference = 'Stop'

$Root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$Stage = Join-Path $Root 'dist\m0-harness'
$Exe = Join-Path $Stage 'voxprint-lucent.exe'
if (-not (Test-Path -LiteralPath $Exe)) {
    throw "Missing $Exe. Run packaging/windows/build-m0-harness.ps1 first."
}

$Tag = $env:LUCENT_RELEASE_TAG
if (-not $Tag) { $Tag = 'v0.1.0-rc1' }
$VersionName = $Tag -replace '^v', ''

$Out = Join-Path $Root 'dist'
$Payload = Join-Path $Out 'release-payload'
if (Test-Path $Payload) { Remove-Item -Recurse -Force $Payload }
New-Item -ItemType Directory -Force -Path $Payload | Out-Null

$nativePref = $PSNativeCommandUseErrorActionPreference
$PSNativeCommandUseErrorActionPreference = $false
& robocopy $Stage $Payload /E /NFL /NDL /NJH /NJS /XD images | Out-Null
if ($LASTEXITCODE -ge 8) { throw "robocopy payload failed ($LASTEXITCODE)" }
$PSNativeCommandUseErrorActionPreference = $nativePref

foreach ($relative in @('Run-M0.ps1', 'app\bench')) {
    $path = Join-Path $Payload $relative
    if (Test-Path $path) { Remove-Item -Recurse -Force $path }
}
Copy-Item (Join-Path $PSScriptRoot 'voxprint-lucent-tools.cmd') (Join-Path $Payload 'voxprint-lucent-tools.cmd')
Copy-Item (Join-Path $PSScriptRoot 'INSTALL-release.txt') (Join-Path $Payload 'INSTALL.txt')

$HarnessZip = Join-Path $Out 'lucent-m0-windows.zip'
$VersionedZip = Join-Path $Out "lucent-$VersionName-windows.zip"
if (Test-Path -LiteralPath $HarnessZip) {
    Move-Item -Force $HarnessZip $VersionedZip
}

$Tools = Join-Path $env:USERPROFILE '.dotnet\tools'
if (-not (Get-Command wix -ErrorAction SilentlyContinue)) {
    dotnet tool install --global wix --version 7.0.0
    $env:PATH = "$Tools;$env:PATH"
}
$Msi = Join-Path $Out "VoxprintAILucent-$VersionName-win64.msi"
$PayloadArg = ((Resolve-Path $Payload).Path) -replace '\\', '/'
Push-Location $PSScriptRoot
try {
    & wix build -acceptEula -d "Payload=$PayloadArg" .\Package.release.wxs -o $Msi
} finally {
    Pop-Location
}
if ($LASTEXITCODE -ne 0) {
    throw "wix build failed with exit $LASTEXITCODE"
}
if (-not (Test-Path -LiteralPath $Msi)) {
    throw "wix build did not write $Msi"
}
Write-Output "Wrote $Msi"
if (Test-Path -LiteralPath $VersionedZip) {
    Write-Output "Wrote $VersionedZip"
}
