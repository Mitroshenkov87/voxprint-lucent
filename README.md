# Voxprint AI Lucent

Fast, colour-accurate image viewer for the [Voxprint AI Media Suite](https://github.com/Mitroshenkov87).

Double-click an image and it should be on screen immediately, including AVIF, JPEG XL, and WebP, with colour that follows the embedded profile. One later action hands the file to Voxprint AI Enlarge. Lucent is a clean-room project: the ideas of a fast native viewer are familiar, and no ImageGlass code, assets, themes, icons, or config formats are used. ImageGlass is GPL-3.0. This repository is Apache-2.0 and contains no GPL code.

## Platforms

- Windows 11 24H2 and newer, x64
- Linux distributions released from 2025 onward (Wayland first)

The viewer is a C++/Qt 6 executable, `voxprint-lucent`, built for x86-64-v3 (AVX2). There is no 32-bit build and no fallback for older operating systems or CPUs. Code stays friendly to a future ARM build; ARM is not built now.

**Lucent does not require an NVIDIA GPU.** That is an exception to the suite GPU rule. Any modern GPU, including an integrated one, is enough to view images. The Enlarge hand-off needs Voxprint AI Enlarge, and Enlarge is the component that warns when the GPU is below RTX 40.

## Architecture

`voxprint-lucent` owns startup, the single instance, decode, the look-ahead cache, and rendering. Settings, tools, the info panel, the Enlarge UI, and the updater run in a separate process, `voxprint-lucent-tools` (Python 3.14 and PySide6), and only when you open them. They are not on the path that paints the first image.

Both processes take their colours from one token file, `share/palette.json`, filled into `share/lucent.qss.in`. The splash identity colour is `#60a5fa`. A blocking splash is not shown before the first pixel; the colour is the window border and the theme accent.

Milestone M0 measured that split. PySide6 does not meet a 250 ms cold first pixel. The C++ raster path does, except for 12 MP AVIF 4:4:4, which is limited by the decode. The numbers, the independent check, and the pass/fail table are in [docs/M0-RESULTS.md](docs/M0-RESULTS.md). The reference laptop has not been measured yet. That run confirms the result. Work on the core viewer continues without waiting for it.

## Network

- An update check against this repository's GitHub releases is allowed, and so is any request you trigger yourself (for example "Check for updates now").
- Statistics and diagnostics are strictly opt-in. They are off by default. Only you turn collection on, or send a report yourself.
- There is no background telemetry, and viewing an image does not use the network. The update check is not implemented in this milestone; the policy is in `voxprint_lucent/network_policy.py` and the viewer does not import it at startup.

## Output formats

Where the tools save a file, the default format is AVIF. JPEG XL and WebP are options. JPEG and HEIC are legacy options. PNG is not an output format. HEIC is the system codec only and is not bundled. libde265 and x265 are not shipped.

## Run the Windows measurement

On Windows 11 or Windows Server 2025, download the `lucent-m0-windows` artifact from CI and unzip it. Leave Microsoft Defender real-time protection on. A cold sample means the first launch after a reboot. Later launches in that same boot are not cold.

Register the one-shot logon task, then reboot:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\Run-M0.ps1 -ScheduleCold
```

Sign in and let the task start the run. It removes itself when it finishes. If you would rather not use a scheduled task, reboot and run `.\Run-M0.ps1` yourself while the machine has been up for less than three minutes.

From a checkout of this repository the same script is `bench\Run-M0.ps1`. It writes `m0-<timestamp>.log` and `m0-<timestamp>.json` with the OS build, CPU, GPU, Defender state, each sample, the median, the nearest-rank p95, and pass/fail of the first post-reboot C++ JPEG launch against 250 ms. Ten separate cold medians would need ten reboots; the log says so.

`-WarmOnly` records warm runs and leaves the cold gate as unmeasured.

## Develop

```bash
python3.14 -m venv .venv
.venv/bin/pip install -e ".[dev]"
cmake -S cpp -B cpp/build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build cpp/build
.venv/bin/ruff check .
.venv/bin/pytest
```

The C++ viewer needs Qt 6. The product pin is Qt 6.12, matching PySide6. A local configure may find an older system Qt; that binary is only a smoke test. CI builds Ubuntu against the Qt 6.12 SDK and builds the Windows zip with Qt 6.12, KImageFormats AVIF/JPEG XL plugins, and the PySide6 comparison viewer.

```bash
cpp/build/voxprint-lucent photo.jpg --benchmark
.venv/bin/voxprint-lucent-tools --ping
```

`--benchmark` prints `FIRST_PIXEL` and quits. It does not take the single-instance socket, so a timing run does not pay for a connect timeout.

Decisions, the v1 plan, and the viewer research live in `docs/`.

## ☕ Support the Project

If you find this project useful and would like to support its development, you can buy me a coffee!

- **Network:** TRON (TRC-20)
- **Accepted:** USDT or TRX
- **Address:** `TYveZBXaSpM4FEHa6iGrc7A6zuLfS4z3x6`

> ⚠️ **Important:** Please ensure you are sending funds **only** via the TRON (TRC-20) network. 
> Sending assets from other networks (such as Ethereum ERC-20, BSC BEP-20, etc.) to this address will result in **permanent loss of funds**.

## Licence

Apache License 2.0. See [LICENSE](LICENSE) and [NOTICE](NOTICE).
