# Voxprint AI Lucent: v1 plan

Draft 2026-10-10. Based on [DECISIONS.md](DECISIONS.md) and the viewer research. M0 is the first code in this repository; measured numbers are in [M0-RESULTS.md](M0-RESULTS.md). Estimates below stay marked (est.) until a later milestone replaces them. Repo: private `Mitroshenkov87/voxprint-lucent`.

**Status 2026-10-10:** Suite Lead approved Plan B. Section 2 below is the original PySide6-as-the-app sketch and is superseded where it conflicts with [DECISIONS.md](DECISIONS.md) items 7 and 8. The `voxprint-lucent` executable is C++/Qt 6. PySide6 is the on-demand tools process. M0 failed for PySide6 and is merged. M1, the core viewer, is in [M1.md](M1.md). M2, the tools and info panel, is in [M2.md](M2.md). M3, the Enlarge client, is in [M3.md](M3.md) and speaks [IPC-ENLARGE.md](IPC-ENLARGE.md) against a mock server. The owner's hardware confirmation run is still open and does not block this work.

## 1. Goal of v1 (MVP)
Double-click any image -> it is on screen in <= 250 ms, browsing a folder feels instant, modern formats just work with correct colour, and one click hands the image to Voxprint AI Enlarge and shows a before/after compare. Native, one-button, local; no default-on telemetry (diagnostics only strictly opt-in).

## 2. Architecture
- **App:** Python 3.14 + PySide6 (Qt 6.10+), shared Voxprint acrylic/Mica UI kit and theme tokens. Lazy imports: only `QtCore/QtGui/QtWidgets` and the decoder path load before the first paint; settings, tools, info panel, updater and the Enlarge bridge load on demand.
- **Decode:** Qt image plugins + KImageFormats (AVIF, JXL, HEIF via system libheif on Linux / WIC on Windows, EXR, RAW, PSD, QOI) into `QImage` on a thread pool. RAW shows the embedded preview first. Exotic/untrusted formats can move to a helper subprocess later (post-MVP).
- **Render:** `QRhiWidget` (D3D12 / Vulkan) with textures, mip-mapped or Lanczos scaling in a shader, nearest-neighbour toggle. Any modern GPU (no RTX requirement).
- **Cache:** look-ahead +/-2 images with a memory cap (MB), downscaled preview first, then full resolution.
- **Single instance:** `QLocalServer` (`voxprint-lucent-<user>`); a second launch forwards paths (~10 ms est.).
- **Colour:** embedded ICC -> monitor profile via lcms2; Qt `QColorSpace` for the common spaces; HDR sources tone-mapped to SDR in v1.
- **Network:** the GitHub-releases update check and user-triggered requests. Statistics/diagnostics only strictly opt-in: off by default, enabled by the user in Settings or sent by him as a one-off report (preview of the payload before sending); nothing collected in the background without his knowledge. No default-on telemetry or analytics. The updater module is not imported until its daily check is due or the user clicks "Check now".
- **Packaging:** Nuitka standalone onedir; Windows per-user installer (HKCU, no admin); Linux Flatpak (`org.kde.Platform` runtime has KImageFormats), AppImage optional.

## 3. Milestones

### M0: cold-start spike (1-2 days) - gate for the whole stack
- Minimal PySide6 window that opens one file from argv and paints it via `QRhiWidget`; Nuitka onedir build; no UI kit beyond a frameless dark window.
- Measure on the reference laptop (RTX 4090 Laptop, Windows 11 24H2, Defender on): process start -> first pixel, cold (after reboot) and warm, for 12 MP JPEG, AVIF, JXL, WebP; plus next/previous with a +/-2 cache. Also on Linux (box baseline: ~160-185 ms warm, offscreen).
- **Pass:** cold first pixel <= 250 ms (median of 10) and next/previous <= 30 ms cached. **Fail:** switch the core to C++/Qt 6 (Plan B) before M1.
- Deliverable: `bench/` script + results table in `lucent/STATE.md`. Needs Aleksandr's PC for the Windows run (ask via Suite Lead; GPU lock not needed, no CUDA use).

### M1: core viewer
- Folder browsing (natural sort; Explorer sort order on Windows), look-ahead cache, zoom modes (fit, fill, 1:1, fit width/height, cursor-anchored wheel zoom), pan with inertia, fullscreen, frameless and window-fit modes, slideshow.
- Formats: JPEG, PNG/APNG, GIF, WebP, AVIF, JXL (incl. animation), TIFF multi-page, SVG, BMP/ICO, HEIF via system codec, RAW, EXR/HDR -> SDR.
- EXIF orientation, ICC colour management. Suite theme, EN (default) + RU + DE, remappable hotkeys. WCAG AA theme test.

### M2: tools and info
- Frame navigator (play/pause, step, export frame as AVIF/JXL/WebP; no PNG output).
- Info panel (dimensions, format, bit depth, colour profile, EXIF/GPS summary, read-only).
- Rotate/flip (lossless where possible), crop (save as AVIF by default), colour picker (hex/RGB copy), copy image/path, delete to Recycle Bin/trash, reveal in folder.

### M3: Enlarge hand-off (needs Enlarge's server side)
- Discovery, capability probe, "Quick enlarge" (headless with progress in the status bar) and "Open in Enlarge..." modes, cancel, before/after compare view (split slider, synced zoom/pan) with Keep / Reveal / Discard. Protocol in section 4.

### M4: integration and release
- File associations helper (Windows Capabilities + `ms-settings:defaultapps` deep link; Linux `.desktop` + `xdg-mime`).
- Self-update check against GitHub releases (once/day max, Settings switch, "Check now"); reuse Audiobook Builder's exe-patch updater.
- Opt-in diagnostics: "Send diagnostic report" (user-initiated, payload preview, no personal data or file paths) and an off-by-default Settings switch for anonymous statistics; endpoint to be decided with Suite Lead.
- Installer, audit gate (ruff/mypy/bandit/gitleaks/pip-audit), Aikido scan, README/LICENSE/NOTICE. Build number/codename per suite release rules; ask Aleksandr "patch or full installer" before building.

Post-MVP: thumbnail grid/strip, compare two arbitrary images, HDR output on HDR displays, batch convert to AVIF/JXL, sandboxed decoder subprocess, Clarity hand-off, ARM64 builds.

## 4. Enlarge hand-off protocol (PROPOSED, to be agreed with Voxprint AI Enlarge)

Enlarge implements the server; Lucent is the first client. Once agreed, the spec moves to a shared versioned file (proposed `suite/IPC-ENLARGE.md`) so other apps can reuse it.

**Discovery (Enlarge registers on install; Lucent only reads):**
- Windows: `HKCU\Software\Voxprint\Enlarge` -> `InstallPath`, `Exe`, `Version`, `IpcName`, `ProtocolVersion`; fallback App Paths `voxprint-enlarge.exe`.
- Linux: `ai.voxprint.Enlarge.desktop` in `$XDG_DATA_HOME`/`$XDG_DATA_DIRS` (+ Flatpak exports) with `X-Voxprint-Ipc=` and `X-Voxprint-Protocol=1`; fallback `voxprint-enlarge` on `PATH`; Flatpak via `flatpak run ai.voxprint.Enlarge`.
- Capability probe: `voxprint-enlarge --capabilities --json` (one JSON object on stdout, exit 0), cached for a day and refreshed when `Version` changes.

**Transport:** `QLocalServer`/`QLocalSocket`. Windows named pipe `\\.\pipe\voxprint-enlarge-<sha256(user SID)[:16]>` with a current-user-only ACL; Linux `$XDG_RUNTIME_DIR/voxprint/enlarge.sock` (dir 0700, socket 0600). UTF-8 JSON-lines, one object per line, every message carries `"protocol": 1`. Unknown fields are ignored; a major change bumps `protocol`.

**Messages:**
- Client -> server: `hello`, `enlarge.request` (`id`, `input`, `scale`, `preset`, `format`, `output`/`output_dir`, `open_ui`), `cancel`.
- Server -> client: `hello` (server version, protocol range), `accepted` (`queue_pos`), `progress` (`pct`, `stage`, optional `eta_s`), `done` (`output`, `width`, `height`, `format`, `ms`), `error` (`code`, `message`, `retryable`).
- Error codes: `gpu_unsupported`, `gpu_busy`, `oom`, `bad_input`, `unsupported_format`, `io_error`, `cancelled`, `internal`.
- Results by file path only; default output next to the source as `<name>.enlarged-<scale>x.avif` (AVIF default per Enlarge decisions; never PNG; never overwrite; numeric suffix on collision).

**Launch:** if no listener, Lucent starts `voxprint-enlarge --serve --hidden` and waits <= 3 s for the socket.

**CLI fallback:** `voxprint-enlarge --input <file> --scale 2 --preset photo [--format avif] [--output <file>] --json-progress` streams the same `accepted/progress/done/error` objects on stdout; exit 0 on success, non-zero on error.

**GPU semantics:** Enlarge warns (does not block) below RTX 40; Lucent shows that warning on the Enlarge button tooltip instead of hiding it, and hides the action only when Enlarge is absent or reports no usable GPU.
