# Build the per-machine MSI from a harness stage that is already on disk.
# Run packaging/windows/build-m0-harness.ps1 first.
$ErrorActionPreference = 'Stop'

$Root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$Stage = Join-Path $Root 'dist\m0-harness'
$Exe = Join-Path $Stage 'voxprint-lucent.exe'
if (-not (Test-Path -LiteralPath $Exe)) {
    throw "Missing $Exe. Run packaging/windows/build-m0-harness.ps1 first."
}

$Tag = $env:LUCENT_RELEASE_TAG
if (-not $Tag) { $Tag = 'v0.1.0-rc7' }
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

foreach ($relative in @('Run-M0.ps1', 'Run-Checks.ps1', 'app\bench')) {
    $path = Join-Path $Payload $relative
    if (Test-Path $path) { Remove-Item -Recurse -Force $path }
}
foreach ($name in @('vcruntime140.dll', 'msvcp140.dll', 'qjpeg.dll', 'qwebp.dll', 'kimg_avif.dll', 'kimg_jxl.dll')) {
    $found = Get-ChildItem -Path $Payload -Recurse -Filter $name -File -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $found) {
        throw "The installer is missing $name. Those files are copied while the package is built."
    }
}
Copy-Item (Join-Path $PSScriptRoot 'voxprint-lucent-tools.cmd') (Join-Path $Payload 'voxprint-lucent-tools.cmd')
Copy-Item (Join-Path $PSScriptRoot 'INSTALL-release.txt') (Join-Path $Payload 'INSTALL.txt')
Copy-Item (Join-Path $PSScriptRoot 'remove-legacy-user.ps1') (Join-Path $Payload 'remove-legacy-user.ps1')
$smokeSrc = Join-Path $Stage 'images\smoke.jpg'
if (-not (Test-Path -LiteralPath $smokeSrc)) { $smokeSrc = Join-Path $PSScriptRoot 'smoke.jpg' }
if (-not (Test-Path -LiteralPath $smokeSrc)) { throw "Missing images\smoke.jpg" }
New-Item -ItemType Directory -Force -Path (Join-Path $Payload 'images') | Out-Null
Copy-Item $smokeSrc (Join-Path $Payload 'images\smoke.jpg')

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
    & wix build -acceptEula wix7 -arch x64 -d "Payload=$PayloadArg" .\Package.release.wxs -o $Msi
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
