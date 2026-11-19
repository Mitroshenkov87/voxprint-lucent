# VOXPrint AI Lucent

Image viewer for the VOXPrint AI Suite: AVIF, JPEG XL, WebP and JPEG, ICC colour, before/after compare. Upscaling goes through VOXPrint AI Enlarge (`voxprint-enlarge-cli`) when it is installed.

- Viewer: C++ / Qt 6.12 (`cpp/`). Helper: Python 3.14 + PySide6 (`voxprint_lucent/`).
- Shared library: voxprint-common v0.8.0 (git tag pin, commit in `share/voxprint-common.pin.json`).
- Packages: `packaging/linux/build-release.sh` (deb), `packaging/windows/build-m0-harness.ps1` + `build-release.ps1` (MSI). CI builds both as workflow artifacts.
- Docs: [docs/README-full.md](docs/README-full.md) and [docs/](docs/).

License: [Apache-2.0](LICENSE)

## ☕ Support the Project

Donations (TRON TRC-20, USDT or TRX): `TYveZBXaSpM4FEHa6iGrc7A6zuLfS4z3x6`
Send only via the TRON (TRC-20) network; other networks lose funds permanently.
