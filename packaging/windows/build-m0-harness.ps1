#Requires -Version 5.1
<#
.SYNOPSIS
    Build the self-contained Windows M0 harness zip.

.DESCRIPTION
    Used by GitHub Actions on windows-latest. Produces dist\lucent-m0-windows.zip
    with the C++/Qt 6.12 viewer, windeployqt runtime, KImageFormats AVIF and
    JPEG XL plugins (HEIF off, no libde265/x265), embeddable Python 3.14 plus
    PySide6-Essentials, the comparison viewer, 12 MP test images, and Run-M0.ps1.
#>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $true

$Repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$QtVersion = '6.12.0'
$QtRoot = 'C:\Qt'
$Stage = Join-Path $Repo 'dist\m0-harness'
$Zip = Join-Path $Repo 'dist\lucent-m0-windows.zip'
$Work = Join-Path $Repo 'dist\m0-work'
$PythonVersion = '3.14.8'

function Invoke-Native {
    param([scriptblock]$Command)
    & $Command
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed with exit $LASTEXITCODE"
    }
}

function Import-VisualStudio {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path -LiteralPath $vswhere)) {
        throw 'vswhere.exe was not found'
    }
    $install = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $install) {
        throw 'No Visual Studio C++ toolset was found'
    }
    $bat = Join-Path $install 'VC\Auxiliary\Build\vcvars64.bat'
    Write-Host "Importing $bat"
    cmd /c "`"$bat`" >nul && set" | ForEach-Object {
        if ($_ -match '^(.*?)=(.*)$') {
            Set-Item -Path "Env:$($matches[1])" -Value $matches[2]
        }
    }
    if (-not (Get-Command ninja -ErrorAction SilentlyContinue)) {
        throw 'ninja was not found after importing the Visual Studio environment'
    }
    $env:CC = 'cl'
    $env:CXX = 'cl'
}

if (-not $env:VCPKG_ROOT) {
    if ($env:VCPKG_INSTALLATION_ROOT) {
        $env:VCPKG_ROOT = $env:VCPKG_INSTALLATION_ROOT
    } elseif (Test-Path 'C:\vcpkg\vcpkg.exe') {
        $env:VCPKG_ROOT = 'C:\vcpkg'
    } else {
        throw 'vcpkg was not found. windows-latest is expected to provide C:\vcpkg.'
    }
}

Write-Host "vcpkg: $env:VCPKG_ROOT"
New-Item -ItemType Directory -Force -Path $Work, (Split-Path $Stage) | Out-Null
if (Test-Path $Stage) { Remove-Item -Recurse -Force $Stage }
New-Item -ItemType Directory -Force -Path $Stage | Out-Null

Write-Host 'Installing Qt 6.12.0 (aqtinstall 3.3.0 misses the Windows feed folder)'
python -m pip install --upgrade py7zr
$QtPrefix = Join-Path $QtRoot "$QtVersion\msvc2022_64"
Invoke-Native { python (Join-Path $Repo 'packaging\windows\fetch_qt.py') $QtPrefix }
if (-not (Test-Path (Join-Path $QtPrefix 'bin\windeployqt.exe'))) {
    throw "Qt was not installed at $QtPrefix"
}

Write-Host 'Installing libavif, libjxl, and pkgconf'
$env:VCPKG_DEFAULT_TRIPLET = 'x64-windows'
Invoke-Native { & (Join-Path $env:VCPKG_ROOT 'vcpkg.exe') install libavif libjxl pkgconf --triplet x64-windows }
$VcpkgInstalled = Join-Path $env:VCPKG_ROOT 'installed\x64-windows'
$PkgConfig = Get-ChildItem -Path $VcpkgInstalled -Recurse -Filter pkgconf.exe | Select-Object -First 1
if (-not $PkgConfig) { throw 'pkgconf.exe was not installed by vcpkg' }

Import-VisualStudio

Write-Host 'Building extra-cmake-modules v6.31.0'
$EcmSrc = Join-Path $Work 'extra-cmake-modules'
$EcmPrefix = Join-Path $Work 'ecm-install'
if (-not (Test-Path $EcmSrc)) {
    Invoke-Native { git clone --depth 1 --branch v6.31.0 https://invent.kde.org/frameworks/extra-cmake-modules.git $EcmSrc }
}
Invoke-Native {
    cmake -S $EcmSrc -B (Join-Path $Work 'ecm-build') -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_INSTALL_PREFIX=$EcmPrefix"
}
Invoke-Native { cmake --build (Join-Path $Work 'ecm-build') --target install }

Write-Host 'Building KImageFormats v6.31.0 (HEIF off)'
$KSrc = Join-Path $Work 'kimageformats'
$KPrefix = Join-Path $Work 'kimageformats-install'
if (-not (Test-Path $KSrc)) {
    Invoke-Native { git clone --depth 1 --branch v6.31.0 https://invent.kde.org/frameworks/kimageformats.git $KSrc }
}
$env:PKG_CONFIG = $PkgConfig.FullName
$env:PKG_CONFIG_PATH = Join-Path $VcpkgInstalled 'lib\pkgconfig'
$prefix = "$QtPrefix;$EcmPrefix;$VcpkgInstalled"
Invoke-Native {
    cmake -S $KSrc -B (Join-Path $Work 'kimageformats-build') -G Ninja -DCMAKE_BUILD_TYPE=Release `
        "-DCMAKE_PREFIX_PATH=$prefix" `
        "-DCMAKE_INSTALL_PREFIX=$KPrefix" `
        "-DPKG_CONFIG_EXECUTABLE=$($PkgConfig.FullName)" `
        -DBUILD_TESTING=OFF `
        -DKIMAGEFORMATS_HEIF=OFF `
        -DKIMAGEFORMATS_JXL=ON
}
Invoke-Native { cmake --build (Join-Path $Work 'kimageformats-build') --target install }

$AvifPlugin = Get-ChildItem -Path $KPrefix, (Join-Path $Work 'kimageformats-build') -Recurse -Filter kimg_avif.dll | Where-Object { $_.FullName -notmatch '\\Debug\\' } | Select-Object -First 1
$JxlPlugin = Get-ChildItem -Path $KPrefix, (Join-Path $Work 'kimageformats-build') -Recurse -Filter kimg_jxl.dll | Where-Object { $_.FullName -notmatch '\\Debug\\' } | Select-Object -First 1
if (-not $AvifPlugin) { throw 'kimg_avif.dll was not produced' }
if (-not $JxlPlugin) { throw 'kimg_jxl.dll was not produced' }
$Heif = Get-ChildItem -Path $KPrefix -Recurse -Filter 'kimg_heif.dll' -ErrorAction SilentlyContinue
if ($Heif) { throw 'kimg_heif.dll was built; HEIF must stay off' }

Write-Host 'Building voxprint-lucent'
$CppBuild = Join-Path $Repo 'cpp\build-windows'
Invoke-Native {
    cmake -S (Join-Path $Repo 'cpp') -B $CppBuild -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_PREFIX_PATH=$QtPrefix"
}
Invoke-Native { cmake --build $CppBuild }
$Exe = Join-Path $CppBuild 'voxprint-lucent.exe'
if (-not (Test-Path $Exe)) { throw "Missing $Exe" }
Copy-Item $Exe (Join-Path $Stage 'voxprint-lucent.exe')
Copy-Item -Recurse (Join-Path $CppBuild 'share') (Join-Path $Stage 'share')

Write-Host 'Deploying the Qt runtime'
$Windeploy = Join-Path $QtPrefix 'bin\windeployqt.exe'
Invoke-Native { & $Windeploy --release --compiler-runtime --no-translations (Join-Path $Stage 'voxprint-lucent.exe') }
$FormatsDir = Join-Path $Stage 'imageformats'
New-Item -ItemType Directory -Force -Path $FormatsDir | Out-Null
Copy-Item $AvifPlugin.FullName (Join-Path $FormatsDir 'kimg_avif.dll')
Copy-Item $JxlPlugin.FullName (Join-Path $FormatsDir 'kimg_jxl.dll')
$QtFormats = Join-Path $QtPrefix 'plugins\imageformats'
foreach ($name in @('qjpeg.dll', 'qwebp.dll')) {
    $src = Join-Path $QtFormats $name
    if (-not (Test-Path $src)) { throw "Qt plugin missing: $src" }
    Copy-Item $src (Join-Path $FormatsDir $name)
}
Get-ChildItem (Join-Path $VcpkgInstalled 'bin\*.dll') | Copy-Item -Destination $Stage
$banned = @(Get-ChildItem $Stage -Recurse -File | Where-Object { $_.Name -match 'de265|x265|^kimg_heif\.dll$' })
if ($banned) {
    throw ("Refusing to ship: " + ($banned.Name -join ', '))
}

Write-Host "Installing embeddable Python $PythonVersion"
$PyZip = Join-Path $Work "python-$PythonVersion-embed-amd64.zip"
Invoke-WebRequest -Uri "https://www.python.org/ftp/python/$PythonVersion/python-$PythonVersion-embed-amd64.zip" -OutFile $PyZip
$PyDir = Join-Path $Stage 'python'
Expand-Archive -Path $PyZip -DestinationPath $PyDir -Force
$Pth = Get-ChildItem $PyDir -Filter 'python*._pth' | Select-Object -First 1
@(
    'python314.zip',
    '.',
    'Lib\site-packages',
    'import site'
) | Set-Content -LiteralPath $Pth.FullName -Encoding ASCII
Invoke-WebRequest -Uri 'https://bootstrap.pypa.io/get-pip.py' -OutFile (Join-Path $Work 'get-pip.py')
Invoke-Native { & (Join-Path $PyDir 'python.exe') (Join-Path $Work 'get-pip.py') }
Invoke-Native {
    & (Join-Path $PyDir 'python.exe') -m pip install --disable-pip-version-check 'PySide6-Essentials==6.12.0' 'imagecodecs==2026.10.10' 'numpy>=2.3'
}

Write-Host 'Copying the PySide6 comparison tree'
$App = Join-Path $Stage 'app'
New-Item -ItemType Directory -Force -Path $App | Out-Null
$nativePref = $PSNativeCommandUseErrorActionPreference
$PSNativeCommandUseErrorActionPreference = $false
& robocopy (Join-Path $Repo 'voxprint_lucent') (Join-Path $App 'voxprint_lucent') /E /NFL /NDL /NJH /NJS /XD __pycache__ | Out-Null
if ($LASTEXITCODE -ge 8) { throw "robocopy package failed ($LASTEXITCODE)" }
& robocopy (Join-Path $Repo 'bench') (Join-Path $App 'bench') /E /NFL /NDL /NJH /NJS /XD __pycache__ fixtures results | Out-Null
if ($LASTEXITCODE -ge 8) { throw "robocopy bench failed ($LASTEXITCODE)" }
& robocopy (Join-Path $Repo 'share') (Join-Path $App 'share') /E /NFL /NDL /NJH /NJS | Out-Null
if ($LASTEXITCODE -ge 8) { throw "robocopy share failed ($LASTEXITCODE)" }
$PSNativeCommandUseErrorActionPreference = $nativePref
Copy-Item (Join-Path $Repo 'bench\Run-M0.ps1') (Join-Path $Stage 'Run-M0.ps1')

Write-Host 'Generating test images'
$ImageDir = Join-Path $Stage 'images'
$env:PYTHONPATH = $App
Invoke-Native {
    & (Join-Path $PyDir 'python.exe') -c @"
from pathlib import Path
import imagecodecs
import numpy as np
from bench.fixtures import ensure_fixtures
root = Path(r'$ImageDir')
ensure_fixtures(root)
small = np.zeros((64, 64, 3), np.uint8)
small[:, :, 0] = 200
root.joinpath('smoke.jpg').write_bytes(bytes(imagecodecs.jpeg_encode(small, level=90)))
root.joinpath('smoke.webp').write_bytes(bytes(imagecodecs.webp_encode(small, level=80)))
root.joinpath('smoke.avif').write_bytes(bytes(imagecodecs.avif_encode(small, speed=8)))
root.joinpath('smoke.jxl').write_bytes(bytes(imagecodecs.jpegxl_encode(small, effort=3)))
"@
}

Write-Host 'Smoking the C++ viewer (offscreen: proves decode and FIRST_PIXEL, not a monitor measurement)'
$platforms = Join-Path $Stage 'platforms'
New-Item -ItemType Directory -Force -Path $platforms | Out-Null
Copy-Item (Join-Path $QtPrefix 'plugins\platforms\qoffscreen.dll') (Join-Path $platforms 'qoffscreen.dll')
$env:QT_QPA_PLATFORM = 'offscreen'
foreach ($name in @('smoke.jpg', 'smoke.webp', 'smoke.avif', 'smoke.jxl')) {
    & (Join-Path $Stage 'voxprint-lucent.exe') (Join-Path $ImageDir $name) --benchmark
    if ($LASTEXITCODE -ne 0) { throw "C++ smoke failed for $name (exit $LASTEXITCODE)" }
}
$env:LUCENT_SHARE = Join-Path $App 'share'
Invoke-Native { & (Join-Path $PyDir 'python.exe') -m voxprint_lucent.tools_host --ping }

$vcpkgRev = (git -C $env:VCPKG_ROOT rev-parse HEAD).Trim()
@(
    "qt=$QtVersion",
    "kimageformats=v6.31.0",
    "extra-cmake-modules=v6.31.0",
    "python=$PythonVersion",
    "pyside6-essentials=6.12.0",
    "imagecodecs=2026.10.10",
    "vcpkg=$vcpkgRev",
    'heif=off'
) | Set-Content -LiteralPath (Join-Path $Stage 'VERSIONS.txt') -Encoding ASCII

if (Test-Path $Zip) { Remove-Item -Force $Zip }
Invoke-Native { tar.exe -a -c -f $Zip -C (Split-Path $Stage) (Split-Path $Stage -Leaf) }
Write-Host "Wrote $Zip"
