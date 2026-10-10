# One command for the reference laptop.
#   ./bench/run_local.ps1
#   ./bench/run_local.ps1 -ReferenceLaptop
#   ./bench/run_local.ps1 -Nuitka -ReferenceLaptop
#   ./bench/run_local.ps1 -ColdAfterReboot -ReferenceLaptop
param(
    [switch]$Nuitka,
    [switch]$ReferenceLaptop,
    [switch]$ColdAfterReboot
)
$ErrorActionPreference = "Stop"
$Root = Resolve-Path (Join-Path $PSScriptRoot "..")
Set-Location $Root
$Python = Join-Path $Root ".venv\Scripts\python.exe"
if (-not (Test-Path $Python)) {
    py -3.14 -m venv (Join-Path $Root ".venv")
    & $Python -m pip install -e ".[dev,nuitka]"
}
$ExeArgs = @()
if ($Nuitka) {
    & $Python -m nuitka --standalone --assume-yes-for-downloads --enable-plugin=pyside6 `
        --output-dir=(Join-Path $Root "build\nuitka") `
        --output-filename=voxprint-lucent `
        (Join-Path $Root "voxprint_lucent\__main__.py")
    $Bin = Get-ChildItem -Recurse (Join-Path $Root "build\nuitka") -Filter "voxprint-lucent.exe" |
        Select-Object -First 1
    if (-not $Bin) { throw "Nuitka did not produce voxprint-lucent.exe" }
    $ExeArgs = @("--executable", $Bin.FullName)
}
$Profiles = @("--runs", "10", "--profiles", "warm,cold")
if ($ColdAfterReboot) {
    $Profiles = @("--runs", "1", "--profiles", "cold")
}
$Ref = @()
if ($ReferenceLaptop) { $Ref = @("--reference-laptop") }
& $Python -m bench.harness @Profiles @ExeArgs @Ref
exit $LASTEXITCODE
