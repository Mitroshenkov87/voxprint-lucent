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
$Msi = Join-Path $Out "VoxprintAILucent-0.1.0.msi"
& wix build -acceptEula wix7 (Join-Path $PSScriptRoot "Package.wxs") -o $Msi
if ($LASTEXITCODE -ne 0) {
    throw "wix build failed with exit $LASTEXITCODE"
}
if (-not (Test-Path -LiteralPath $Msi)) {
    throw "wix build did not write $Msi"
}
Write-Output "Wrote $Msi"
