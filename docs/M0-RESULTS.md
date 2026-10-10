# Voxprint AI Lucent: M0 results

Date: 2026-10-10.

Plan rule (PLAN-v1, M0): pass means cold first pixel ≤ 250 ms (median of 10) and cached next/previous ≤ 30 ms on the reference laptop. Fail means the viewer core moves to C++/Qt 6 before M1.

**Suite Lead approved Plan B on 2026-10-10.** `voxprint-lucent` is the C++/Qt 6 executable. `voxprint-lucent-tools` is the on-demand PySide6 process. The shared theme is `share/palette.json` substituted into `share/lucent.qss.in`. M1 starts when this pull request is green. The owner's Windows run confirms the numbers and does not block M1.

## Verdict

| Path | Cold first pixel ≤ 250 ms | Where it was measured |
|---|---|---|
| PySide6, best case (Nuitka onedir, raster, JPEG DCT preview) | **Fail.** 259 ms median, and that is the best cell. Empty window already 243–264 ms. | Linux spike, page cache dropped, N=10 |
| C++/Qt 6 raster preview | **Pass** for JPEG 12 MP (118), JPEG 24 MP (153), WebP (237), AVIF 4:2:0 (234), JXL (229). **Fail** for AVIF 4:4:4 12 MP (369), decode-bound. | Same spike |
| This agent's VM | No cold sample (`drop_caches` denied). Warm PySide6 QRhi JPEG median 281 ms, which matches the spike and is already over 250. Warm C++ JPEG median 41 ms on a smaller synthetic file. | See below |
| Reference laptop (RTX 4090 Laptop, Windows 11 24H2, Defender on) | **Not measured yet.** `bench/Run-M0.ps1` | Owner |

CI virtual machines are indicative only. The PySide6 benchmark job is allowed to fail.

## Linux spike (indicative)

A separate Linux box (Debian 13, 8 vCPU Xeon, no GPU, Xvfb, software GL) ran a PySide6 viewer and a C++ twin. Clock was `CLOCK_MONOTONIC` in the parent, before `subprocess` spawn. Cold was `sync; echo 3 > /proc/sys/vm/drop_caches` before every run. N=10. Median and p95 used linear interpolation. Window 1600×1000. JPEG used `setScaledSize` (DCT preview). Other formats decoded full size. KImageFormats 6.13 supplied AVIF and JXL. The C++ twin linked system Qt 6.8.2. PySide6 was 6.12.0 on CPython 3.14.8.

Cold first image, median / p95, milliseconds:

| Variant | JPEG 12 MP | WebP 12 MP | AVIF 4:2:0 | JXL 12 MP | AVIF 4:4:4 |
|---|---|---|---|---|---|
| PySide6 raster, JPEG preview | 260 / 284 fail | 388 / 432 fail | 385 / 404 fail | 359 / 404 fail | 509 / 521 fail |
| Nuitka onedir, same raster preview | 259 / 273 fail | 369 / 395 fail | 380 / 429 fail | 371 / 417 fail | 502 / 541 fail |
| C++ raster preview | **118 / 125 pass** | **237 / 245 pass** | **234 / 311 pass** (p95 over) | **229 / 243 pass** | 369 / 389 fail |

Cached next/previous passed: PySide6 raster 2.0 ms median, QRhiWidget 9.4 ms median.

The PySide6 cost is the interpreter and shiboken import (~114 ms warm, ~166 ms cold before Qt works). Nuitka did not remove it. An empty PySide6 window was already 243–264 ms cold. Decode cannot start before `QApplication`, because the plugin loaders race otherwise. `QRhiWidget` added about 90–100 ms on llvmpipe, so the first paint stays on the raster path. `setScaledSize` helps JPEG only.

AVIF 4:4:4 at 12 MP stays over budget in C++ on that machine. It is a decode floor, not a Python floor.

## This VM, checked independently

Ubuntu, kernel 6.12, `DISPLAY=:1` (a real X desktop, not Xvfb), `LIBGL_ALWAYS_SOFTWARE=1`. CPython 3.14.8, PySide6 6.12.0. The C++ binary was linked to the distro Qt **6.4.2**, which is not the product Qt. Product builds use Qt 6.12. This machine has no `qwebp` plugin and no KImageFormats plugin, so the C++ binary exits 4 (`decode failed`) for WebP, AVIF, and JXL instead of hanging. `drop_caches` was denied, so there is no cold sample here.

Warm parent-clock samples, N=5, no page-cache drop:

| Binary | Image | Median ms | Samples |
|---|---|---|---|
| C++ raster, Qt 6.4.2 | synthetic JPEG 4000×3000, 0.95 MB | 41.0 | 41.0, 40.8, 38.4, 41.3, 42.4 |
| PySide6 `QRhiWidget` | same JPEG | 281.3 | 281.4, 281.3, 278.5, 276.4, 285.9 |
| PySide6 `QRhiWidget` | synthetic WebP | 290.3 | 290.6, 290.0, 299.5, 290.3, 286.9 |
| PySide6 via imagecodecs | synthetic AVIF, 2.6 MB | 601.9 | 601.9, 601.9, 583.7, 593.7, 618.2 |
| PySide6 via imagecodecs | synthetic JXL, 6.2 MB | 1236.5 | 1239.5, 1246.2, 1233.7, 1231.4, 1236.5 |

One warm `import PySide6.QtWidgets` was 73.7 ms. The spike's warm `QtGui` import was about 114 ms.

### Where this disagrees with the spike, and where it does not

- The PySide6 JPEG number agrees. This VM's QRhi warm median is 281.3 ms. The spike's `py-rhi-preview` JPEG warm median was 281 ms. Both are over 250 ms with no cold penalty added. This does not rescue PySide6.
- A faster warm import (74 ms vs ~114 ms) is a faster machine. It does not touch the spike's cold empty-window result of 243–264 ms.
- C++ JPEG warm 41 ms is faster than the spike's 79 ms warm. The file is a block-noise gradient of 0.95 MB, not the spike's 2.8 MB photo-like JPEG, and the Qt is 6.4.2 rather than 6.8.2. Direction agrees (comfortably under 250, warm). It is not a cold pass and not the product binary.
- AVIF and JXL are much slower here (602 ms and 1236 ms warm) than the spike's ~300 ms warm plugin path. This tree decodes those formats with imagecodecs, and the fixtures are larger (2.6 MB and 6.2 MB versus 0.97 MB and 1.9 MB). That is a different decoder, not a correction of the KImageFormats times. The Windows harness builds KImageFormats 6.31 against Qt 6.12 so the owner's C++ run uses the plugin path.
- Cached navigate on this VM's Python QRhi path was about 25–33 ms (N=5). The spike's QRhi navigate median was 9.4 ms with a filled look-ahead cache. The implementations differ, and this GPU is llvmpipe/lavapipe.
- p95 in `bench/stats.py` and in `Run-M0.ps1` is nearest-rank. For N=10 that is the maximum. The spike interpolated. Even-N medians match (average of the two middle values).
- The window background token `ROOT_PLAIN` is `#17171c`. The spike painted `#121418`. That does not move the timing.

## How the owner runs it

CI on `windows-latest` uploads `lucent-m0-windows.zip`. Unzip it on Windows 11 or Windows Server 2025. Leave Microsoft Defender real-time protection on.

A cold sample is the first launch after reboot. Runs 2 through N in that same boot are not cold, because Windows has cached the executable and the Qt DLLs. Ten independent cold medians would need ten reboots. The script records run 1 of the C++ JPEG launch as `cold_gate` and also writes the session median, nearest-rank p95, and the warm median.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\Run-M0.ps1 -ScheduleCold
```

Reboot, sign in, and do not open the viewer yourself. The one-shot task `VoxprintLucent-M0-Cold` runs `-ColdResume` at logon and then removes itself.

If a scheduled task is not wanted, reboot and run `.\Run-M0.ps1` yourself while uptime is still under three minutes. The same script from a git checkout is `bench\Run-M0.ps1`.

Outputs: `results\m0-<timestamp>.log` and `results\m0-<timestamp>.json` (or `bench\results\` in a checkout). The JSON includes OS build, CPU, GPU, and Defender state. `Get-MpComputerStatus` needs permission; if it fails, the field is recorded as unavailable rather than guessed.

`-WarmOnly` skips the cold gate. PySide6 in the zip is the comparison harness and is expected to miss 250 ms.

## What M1 does next

M1 is recorded in [M1.md](M1.md). `--benchmark` is unchanged. AVIF 4:4:4 remains a known decode-bound miss until a smaller preview exists. The reference laptop run is still open.
