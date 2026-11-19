# Packaging

Both installers are built by `.github/workflows/ci.yml` as workflow artifacts. There are no release pages or release assets.

## Linux: Debian package

`packaging/linux/build-release.sh` builds `dist/voxprint-lucent_<version>_amd64.deb`. Everything installs under `/opt/voxprint-ai-suite/lucent` (the suite install root from voxprint-common):

- the C++ viewer linked against libavif and libjxl (native AVIF and JPEG XL), with those libraries and their codec libraries bundled in `lib/`;
- the Qt 6.12 runtime and plugins (aqtinstall);
- an embedded Python 3.14 (python-build-standalone through `uv`) with PySide6-Essentials, imagecodecs, numpy, voxprint-common from its git tag, and the `voxprint_lucent` helper;
- `/usr/bin/voxprint-lucent`, `voxprint-lucent-cli`, `voxprint-lucent-tools`, the desktop entry and hicolor icons.

The launcher records the install in `<suite home>/state/installed-apps.json` through `python -m voxprint_lucent.register` (voxprint-common `register_app`). Build needs cmake, ninja, a C++ compiler, uv, `libavif-dev` and `libjxl-dev`.

`packaging/linux/smoke-deb.sh <deb> [--install]` installs (or unpacks) the package and opens WebP, AVIF, JPEG XL and JPEG offscreen.

## Windows: WiX MSI (Lucent-only exception)

`packaging/windows/build-m0-harness.ps1` builds the tree (C++ viewer, Qt 6.12, KImageFormats AVIF and JPEG XL plugins, embeddable Python 3.14 with the helper). `packaging/windows/build-release.ps1` packs it into `dist/voxprint-lucent-setup-<version>-win-x64.msi` with WiX 7 (`Package.release.wxs`): per machine under `C:\Program Files\VOXPrint AI Suite\Lucent`, Start menu `VOXPrint AI Suite\VOXPrint AI Lucent`, App Paths, `HKLM\Software\VOXPrint\Lucent`, and "Open with" entries for image types. Setup runs `migrate-suite.ps1`, which also registers Lucent in `installed-apps.json`.

`packaging/windows/smoke-msi.ps1 -Msi <file>` installs silently, opens the same samples offscreen, checks the registry row, and uninstalls.

## Flatpak

`packaging/linux/ai.voxprint.Lucent.yml` is kept for a later Flatpak build. CI does not build it.
