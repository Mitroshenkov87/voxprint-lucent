# Build the per-user MSI skeleton with WiX Toolset 7.
$ErrorActionPreference = "Stop"
$Root = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$Out = Join-Path $Root "dist"
New-Item -ItemType Directory -Force -Path $Out | Out-Null
$Tools = Join-Path $env:USERPROFILE ".dotnet\tools"
if (-not (Get-Command wix -ErrorAction SilentlyContinue)) {
    dotnet tool install --global wix --version 7.0.0
    $env:PATH = "$Tools;$env:PATH"
}
$Msi = Join-Path $Out "voxprint-lucent-setup-0.1.0-win-x64.msi"
# File sources in Package.wxs are relative to the working directory.
Push-Location $PSScriptRoot
try {
    & wix build -acceptEula wix7 .\Package.wxs -o $Msi
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
