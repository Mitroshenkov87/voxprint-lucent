# Voxprint AI Lucent

Fast, colour-accurate image viewer for the [Voxprint AI Media Suite](https://github.com/Mitroshenkov87).

Double-click an image and it should be on screen immediately, including JPEG, WebP, and, on Windows, AVIF and JPEG XL. Lucent does not use the network to show a picture. One later action can hand the file to Voxprint AI Enlarge.

Lucent is a clean-room project. No ImageGlass code, assets, themes, icons, or config formats are used. ImageGlass is GPL-3.0. This repository is Apache-2.0.

## System requirements

- Windows 11 24H2 or newer, or Windows Server 2025, x64. Linux distributions from 2025 onward, x86-64-v3 (AVX2). There is no 32-bit build.
- **Viewing does not need an NVIDIA GPU.** Any modern GPU, including an integrated one, is enough. `LUCENT_RHI=0` keeps painting on the CPU.
- The Windows installer brings its own Qt runtime, AVIF and JPEG XL plugins, and the tools program. You do not install Qt or Python yourself.
- Tools and the Enlarge hand-off on Linux need Python 3.14. The Linux package's viewer includes Qt 6.12, JPEG, and WebP. AVIF and JPEG XL plugins are in the Windows installer.
- Enlarge is a separate application. Lucent only contains the client. Enlarge has its own requirements and warns below an RTX 40. Lucent itself still opens images without it.

## Install

Release files are on this repository's GitHub releases. `v0.1.0-rc3` is a pre-release. The release title is `0.1.0 RC · build N "Zohar"`. Check `SHA256SUMS` against the download.

**Windows.** Run `VoxprintAILucent-0.1.0-rc3-win64.msi`. It installs for your user only, under `%LOCALAPPDATA%\Voxprint\Lucent`, and does not ask for an administrator. A Start menu shortcut is created.

The zip `lucent-0.1.0-rc3-windows.zip` is the measurement bundle (the viewer plus `Run-M0.ps1`), not a second install method. Use it when you want the cold-start sample described in `docs/M0-RESULTS.md`.

**Linux.**

```bash
sudo apt install ./voxprint-lucent_0.1.0~rc3_amd64.deb
```

## First run

Open an image from the Start menu shortcut, from a file association, or from a terminal:

```bash
voxprint-lucent photo.jpg
```

The picture is drawn by the C++ program. A slim toolbar hides itself until the pointer is near the top: open, previous and next, zoom, rotate and flip, crop, save a copy or save as (AVIF, JPEG XL, WebP), trash, slideshow, fullscreen, thumbnails, Enlarge, and settings. Right-click the picture for the same actions plus copy and reveal. With no file, the welcome picture opens on the first three such launches. Left and Right move through the same folder. The tools window stays closed until you ask for it.

Press `I`, or run:

```bash
voxprint-lucent-tools --path photo.jpg
```

That window shows dimensions, format, bit depth, colour profile, and a JPEG EXIF and GPS summary. From there you can step frames, export a frame, rotate, flip, crop, copy a colour, copy the image or its path, reveal the folder, or move the file to the trash.

## Keys

All of these can be remapped with Ctrl+K.

- `1` actual size, `2` fit, `3` fill, `4` fit width, `5` fit height
- Wheel zooms on the cursor. Drag pans.
- Left and Right move to the previous and next image
- `S` slideshow, `F11` fullscreen, Ctrl+Shift+F frameless, Ctrl+Shift+W fit the window to the image
- `.` and `,` step frames of a multi-page or animated file
- `I` opens the tools window. Ctrl+E hands the file to Enlarge. Ctrl+O opens a file. Delete moves it to the trash
- Ctrl+S saves an AVIF copy. Ctrl+Shift+S saves as AVIF, JPEG XL, or WebP. `G` toggles thumbnails. Ctrl+, opens settings
- Ctrl+L cycles English, Russian, and German
- `Q` or Esc quits. Esc leaves fullscreen, then stops a slideshow, before it quits

## Formats

Lucent reads JPEG, WebP, PNG, GIF, TIFF, BMP, and, when the plugin is installed, AVIF, JPEG XL, HEIF, RAW, and EXR. The Windows installer includes AVIF and JPEG XL. HEIF uses the system codec only. libde265 and x265 are not shipped.

Where the tools save a file, the default is AVIF. JPEG XL and WebP are the other choices. JPEG and HEIC are legacy choices. PNG is not an output format.

Embedded colour profiles are converted to sRGB. A separate monitor profile is not applied in this version.

## Enlarge

Enlarge is optional. With Voxprint AI Enlarge installed:

```bash
voxprint-lucent-tools --enlarge --path photo.jpg
```

Lucent finds Enlarge from its install registration and runs `voxprint-enlarge` with `--json-progress`. It sends the file path, not the pixels, and does not open Enlarge's socket. The tools window opens a before/after split. Keep leaves the result. Reveal opens the folder. Discard deletes the result and leaves the original. If Enlarge is not installed, the action stays disabled and Install Enlarge opens the Enlarge releases page. If Enlarge reports no usable GPU, the action stays disabled and that button is not shown. A GPU that is usable but below an RTX 40 stays available and shows Enlarge's warning on the button.

`voxprint-lucent --dry-run` checks startup and this hand-off without a window or a GPU. `voxprint-lucent --version` prints the build label. Both are described in [docs/CLI.md](docs/CLI.md).

The protocol is [docs/IPC-ENLARGE.md](docs/IPC-ENLARGE.md). Discovery is [docs/APP-DISCOVERY.md](docs/APP-DISCOVERY.md). This repository does not ship the Enlarge server. Tests use `python -m voxprint_lucent.enlarge_mock`.

## Updates and privacy

- The only network call Lucent makes on its own is an update check, and only when you run `voxprint-lucent-tools --check-update`. It is a plain HTTPS GET of this repository's GitHub releases. The request carries the product name and no account, no install id, and no usage statistics.
- Opening an image does not use the network. There is no background telemetry.
- Statistics and diagnostics are strictly opt-in. They are off by default. Only you turn collection on, or send a report yourself. Nothing in this version sends a diagnostics report.

## Uninstall

**Windows.** Settings, Apps, Voxprint AI Lucent. That removes the per-user install.

**Linux.**

```bash
sudo apt remove voxprint-lucent
```

## ☕ Support the Project

If you find this project useful and would like to support its development, you can buy me a coffee!

- **Network:** TRON (TRC-20)
- **Accepted:** USDT or TRX
- **Address:** `TYveZBXaSpM4FEHa6iGrc7A6zuLfS4z3x6`

> ⚠️ **Important:** Please ensure you are sending funds **only** via the TRON (TRC-20) network. 
> Sending assets from other networks (such as Ethereum ERC-20, BSC BEP-20, etc.) to this address will result in **permanent loss of funds**.

## Licence

Apache License 2.0. See [LICENSE](LICENSE) and [NOTICE](NOTICE).

## Develop

Build instructions, the 250 ms check, and the milestone notes live in `docs/`. A checkout uses Python 3.14:

```bash
python3.14 -m venv .venv
.venv/bin/pip install -e ".[dev]"
.venv/bin/ruff check .
.venv/bin/pytest
```

`--benchmark` prints `FIRST_PIXEL` and quits. It does not open the tools process. The product pin is Qt 6.12.
