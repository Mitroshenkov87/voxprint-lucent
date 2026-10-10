# Packaging

M0 ships installer skeletons. They produce artifacts. They are not the Nuitka onedir release from PLAN-v1 milestone M4.

## Windows: WiX Toolset 7

`packaging/windows/build-installer.ps1` builds a per-user MSI with WiX 7 (`wix build`). WiX 7 is the current Toolset. The package installs under `%LOCALAPPDATA%\Voxprint\Lucent` (HKCU, no administrator). That matches the suite rule for a per-user install.

MSIX is not used. A packaged sandbox would get in the way of the later Enlarge local socket and of per-user file associations. Inno Setup is not used; WiX is the forward-only Windows installer.

The MSI currently installs a launcher script. That script is a packaging skeleton. The product executable is the C++ `voxprint-lucent` binary. M4 replaces the skeleton payload with the real install layout.

## Linux: Debian package in CI, Flatpak later

`packaging/linux/build-deb.sh` builds `voxprint-lucent_<version>_amd64.deb` with `dpkg-deb`. The archive contains the Python package, a `voxprint-lucent` launcher, and `ai.voxprint.Lucent.desktop`. It depends on Python 3.14 and expects the Python dependencies to be present. CI can build it in seconds.

Flatpak with the `org.kde.Platform` runtime is the intended Linux delivery (that runtime already has KImageFormats). Downloading the runtime on every GitHub Actions run would spend the free minutes budget, so CI does not build a Flatpak. The manifest is `packaging/linux/ai.voxprint.Lucent.yml` for that later build.

## What CI runs

Lint, tests, a non-blocking PySide6 benchmark, a C++ smoke build on `ubuntu-latest` (aqtinstall Qt 6.12, not the distro's Qt 6.4), the WiX skeleton, and the deb. `windows-latest` also builds `dist/lucent-m0-windows.zip`: the C++ viewer, the Qt 6.12 runtime, KImageFormats AVIF and JPEG XL plugins, embeddable Python with the PySide6 comparison viewer, test images, and `Run-M0.ps1`. Nuitka is not part of CI.
