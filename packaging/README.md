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

The policy and the minute estimates are in [docs/CI.md](../docs/CI.md). Pull requests run Ubuntu lint, tests, and the C++ smoke build. Pushes to `main` and `workflow_dispatch` also run the benchmarks, the skeleton deb, the skeleton WiX package, and the Windows M0 harness. A `v*` tag runs `.github/workflows/release.yml` instead: the full Windows MSI, the measurement zip, and the Linux package, with `SHA256SUMS`, published as a GitHub release (pre-release when the tag contains `-`).

`windows-latest` builds `dist/lucent-m0-windows.zip` on those heavy runs: the C++ viewer, the Qt 6.12 runtime, KImageFormats AVIF and JPEG XL plugins, embeddable Python with the PySide6 comparison viewer, test images, and `Run-M0.ps1`. libavif in that zip is `libavif[dav1d]`: the vcpkg port enables no AV1 codec unless a feature is named, and without one the AVIF plugin loads and then rejects every file. That embeddable interpreter ignores `PYTHONPATH` while `python*._pth` exists, so the build lists `..\app` in that file (paths are relative to the `python` folder). A git checkout still uses a normal interpreter and `PYTHONPATH`. Nuitka is not part of CI. The Ubuntu smoke build uses aqtinstall Qt 6.12, not the distro's Qt 6.4, and still requires in-process `first_image_presented_ms` of at most 250.
