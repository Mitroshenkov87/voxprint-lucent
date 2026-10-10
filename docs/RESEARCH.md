# Voxprint AI Media Suite — Image Viewer: research

Date: 2026-10-10 (MSK). Read-only research. Nothing was published, forked or sent.
Working title in this doc: **Viewer**. Name candidates are in §6.

---

## 0. TL;DR

- **ImageGlass is still GPL-3.0** (the source never changed licence), but since ImageGlass 10 (GA 21 Aug 2026) it is **open-core**: a paid **ImageGlass Pro** edition unlocked by a signed licence file. The Pro gate (`ProGate.cs`, `LicenseVerifier.cs`, an embedded public key) lives *inside* the GPL repo, and the update check sends coarse usage stats in its User-Agent (opt-out via `DO_NOT_TRACK=1`). Earlier steps: a paid Microsoft Store edition (around v8.5, early 2022) and "register for commercial use" nags. The name and logo are explicitly trademarked.
- **For us:** no ImageGlass code, assets, themes, icons or config formats in an Apache-2.0 app. **Clean-room only**: take the feature ideas, write our own code. Don't use "Glass" or "Kobe" in any name.
- **Stack recommendation:** **PySide6 (Qt 6.10+/6.12) on Python 3.14**, with a hard **cold-start budget** (first pixel ≤ 250 ms on the 4090 laptop). Decoding stays in C/C++ (Qt image plugins plus KDE KImageFormats, libavif/libjxl/libwebp, LibRaw, OpenEXR, lcms2), so Python never touches pixels. Rendering is GPU via Qt RHI (D3D12/Vulkan). **Plan B, if the budget fails:** the same design in **C++/Qt 6**, keeping the theme (QSS/tokens), plugins and IPC spec. Rust/egui is the fastest option but would split the suite's UI toolkit and theme.
- **GPU policy:** the viewer should **not** require an RTX 40. It's a viewer and uses any D3D12/Vulkan GPU. Only the "Enlarge" action needs Enlarge plus a supported RTX. This is a deliberate exception to the suite rule and **needs Aleksandr's OK**.
- **Enlarge hand-off:** discover Enlarge through a per-user registry key / App Paths (Windows) or the `.desktop` file plus `PATH` (Linux). Hand off over a **local socket** (QLocalServer: named pipe or unix socket) using JSON-lines; if Enlarge isn't running, start it with CLI args. The result comes back as a file path and opens in a before/after compare view. **No URI scheme.**
- **Names (top 3):** **Voxprint AI Lumen**, **Voxprint AI Optic**, **Voxprint AI Viewer** (plain descriptive). Details in §6.

---

## 1. ImageGlass: licence history and what is paid

| When | What happened | Source |
|---|---|---|
| 2010 → | Created by Dương Diệu Pháp. Open source, Windows, WinForms/.NET Framework. Repo started 2012-12-30. | GitHub API, `d2phap/ImageGlass` |
| v8 "Kobe" (2020–22) | **"Kobe" was the v8 codename** (and is still the name of the default theme pack). v8 ran on .NET Framework 4.8, Windows 7–11, and added AVIF/HEIF/CR3/JP2 via Magick.NET. | https://imageglass.org/release/imageglass-8-0-12-8-kobe-30 |
| 2022-01-22 (v8.5) | **Paid "ImageGlass Store"** launched on the Microsoft Store (fee, 7-day trial, auto-updates and early hotfixes). "Classic" stayed free. The EULA *recommends* (does not enforce) registering for commercial use. | https://imageglass.org/news/announcing-imageglass-kobe-8-5-74 ; https://github.com/d2phap/ImageGlass/tree/8.5.1.22 |
| v8 EOL | "ImageGlass 8 is end of life… opt for the **Paid Support Service**, or upgrade to ImageGlass 9 (free)." | https://imageglass.org/news/announcing-imageglass-8-7-79 |
| v9 (2023–26) | .NET 6+ rewrite with D2D hardware acceleration and a WebView2 settings UI/SVG. The LICENSE was a custom EULA wrapper ("ImageGlass Store is a paid version… Classic free") around GPL-3.0 source. | https://raw.githubusercontent.com/d2phap/ImageGlass/9.0.9.1230/LICENSE |
| 2026-03-03 | 2026 roadmap announced. | https://github.com/d2phap/ImageGlass/discussions/2287 |
| 2026-08-21 (v10.0.4 GA) | **Full rewrite on .NET + Avalonia, NativeAOT**, cross-platform (Win/macOS/Linux X11). **ImageGlass Pro launched**: Pro Individual $14.90 one-time (v10.x only), Pro Team $69, Pro Business from $99/seat/yr subscription. v9 gets 6 months of hotfixes, then EOL, with "paid extended support". | https://imageglass.org/news/introducing-imageglass-10-rebuilt-for-speed-and-cross-platform-support-107 ; https://imageglass.org/pricing |
| 2026-07-20 / 07-25 | Classic licence page (GPLv3) and Pro licence terms published. Pro is "trust-based": a signed licence file checked locally, no account and no activation server. A **separate MIT source grant is offered for sale**. | https://imageglass.org/license?type=classic ; https://imageglass.org/license?type=pro ; https://imageglass.org/pro |
| current LICENSE | GPL-3.0 source plus informational notes. **"The ImageGlass name and logo are trademarks… written permission must be obtained before using the ImageGlass name or logo in any distributed works."** GitHub shows the licence as `NOASSERTION` because of the wrapper text. | https://raw.githubusercontent.com/d2phap/ImageGlass/develop/LICENSE |

**Pro-only features (v10)** (https://imageglass.org/pricing): live HDR Tone Mapper tool, free and margin panning, File Watcher (live folder/photo changes), mouse-click action remapping UI, default/admin config files for managed deployment, feature locks for shared devices, plus MSI deployment and priority support (Business). The Store edition can't use custom file associations or icons because of sandbox limits.

**Strictly, "activation/registration" overstates it.** Classic needs no account or activation, and Pro uses an offline licence file. What *is* true, and is the part that matters for our pitch:
- open-core gating code (`source/ImageGlass.Lib/Common/ProGate.cs`, `ServiceProviders/Licensing/*`, `Assets/Licensing/ig_license_2026.spki.pem`, "Pro" badges in the UI) ships inside the GPL app;
- the Pro licence covers one major line only (v10.x), so v11 may need a new purchase;
- the update check sends usage tokens such as OS, arch, package, channel, language, edition (e.g. `pro-msstore`) and update gap (`Update/UsageStatsAgent.cs`). It's aggregate and opt-out (`DO_NOT_TRACK=1`), but it's still telemetry;
- the trend over the years is paid Store → paid support → paid Pro tier with recurring Business seats.

**What this means for us**
- GPL-3.0 code can't be copied into an Apache-2.0 app. The same applies to its themes, icons, SVG assets, `igconfig.json` schema, `.igtheme` format and SDK protocol. **Clean-room rules:** the people (bots) writing our code work from feature descriptions (this doc, the public docs/website) and don't read ImageGlass source. Feature ideas, UX conventions (zoom modes, frameless mode, etc.) and general behaviour aren't copyrightable. Note that the source files I opened for this report were only for the facts above, and none of them is quoted for reuse.
- Don't build compatibility with ImageGlass plugin or theme formats (it would invite derivative-work arguments and adds nothing for us).
- Name/branding: see §6.

---

## 2. ImageGlass feature list and tech stack

**Features (Classic, v10)** (README, website, v10 release notes):
- 90+ formats via **Magick.NET (ImageMagick)** plus **SkiaSharp** codecs, and an in-house **SVG/SVGZ** renderer (Svg.Skia) with SMIL animation. Codec plugin architecture (third-party decoders, permission prompt, signature pinning).
- Hardware-aware **look-ahead caching** (memory cap, max file size/pixels, look-ahead depth) and "Turbo" fast browsing. Decoding off the UI thread.
- **Colour management** (per-monitor ICC on Windows via `Win32ColorProfileProvider`) and basic **HDR** (AVIF, JXL, Radiance HDR, EXR) with a default tone-map curve. The live tone mapper is Pro-only.
- Zoom modes (auto, lock, fit width/height, scale-to-fill, fixed %) with cursor-anchored zoom and selectable interpolation. Touch pinch/pan.
- Window modes: **Frameless**, **Window Fit** (window resizes to the image), Full screen, **Slideshow** (random interval, sound cue). Windows backdrop (Mica/Acrylic).
- Built-in tools: rotate/flip, **crop**, resize, **colour picker**, **frame navigation** (GIF/WebP/APNG/multi-page TIFF, Motion Photos), **lossless compression**, set as wallpaper/lock screen, print, share, open-with.
- **EXIF** through a separate app, ExifGlass.
- Themes (`.igtheme` = zip of JSON plus SVG icons), layouts, remappable hotkeys and mouse actions, toolbar customisation, language packs (Crowdin).
- External tools and plugins via ImageGlass.SDK (named pipes).
- Windows: Explorer sort order (it reads the folder's current Explorer sorting), file-type icon packs, Startup Boost (v9 kept a resident process to fake fast start).
- Quick Setup wizard, auto-update.

**Stack history**: v1–8 WinForms/.NET Framework/GDI+ → v9 .NET 6–8 WinForms + **Direct2D** (D2Phap.DXControl) + **WebView2** for settings/SVG → v10 **.NET 10 + Avalonia + NativeAOT + SkiaSharp + Magick.NET**, one codebase. Packaging: Windows MSIX/MSI/ZIP; macOS DMG; Linux AppImage/Flatpak/tar.gz. Their own README still lists Windows 10 1809 as the minimum, so they carry the long tail we're deliberately dropping.

Lessons worth stealing as *ideas*: AOT/native start-up, off-thread decode, look-ahead cache with explicit memory caps, Window Fit/frameless modes, Explorer sort order, frame navigator, colour picker, plugin permission prompts.

---

## 3. Alternatives / prior art

| App | Licence | Stack | Best at / what to learn |
|---|---|---|---|
| **qView** | GPL-3.0 | C++/Qt | Minimal, chrome-less UI; relies entirely on Qt image plugins (plus KImageFormats). Shows a Qt viewer can feel instant. Low activity (last push Apr 2026). |
| **nomacs** | GPL-3.0 | C++/Qt, LibRaw, OpenCV | Sync of zoom/position between several instances (compare), thumbnail strip, batch, plugins. Active. |
| **qimgv** | GPL-3.0 | C++/Qt (+mpv) | Very fast folder browsing, a good thumbnail grid, optional video. |
| **Oculante** | **MIT** | Rust (notan/egui, wgpu) | GPU-rendered, very fast, HDR/EXR/RAW/JXL/AVIF, channel view, pixel inspector, simple edits, compare. Permissive, so it can be read freely (code still shouldn't be pasted without MIT notices). |
| **GNOME Loupe** | GPL-3.0 | Rust/GTK4, **glycin** | Decoders run in **sandboxed subprocesses** (glycin). Good security pattern for untrusted files; modern Linux default. |
| **JPEGView** (sylikc fork) | GPL-2.0 | C++/Win32, SIMD | Extremely fast JPEG decode/resample, auto-enhance, lean UI. Windows-only; inactive since 2024. |
| **XnView MP** | Freeware (private/edu), paid commercial; closed | C++/Qt | Huge format list (500+), browser/DAM, batch convert. A reference for breadth. |
| **IrfanView** | Freeware non-commercial; closed | C/Win32 | Legendary start-up speed (tiny native binary), plugin packs, batch. Windows-only. |
| **Windows Photos** | Proprietary, bundled | WinUI/WinAppSDK | System integration, HEIF/AVIF/JXL via **WIC Store extensions**, OneDrive/AI features (super resolution on Copilot+ PCs). Slow-ish start, account/cloud nudges: the thing we're the alternative to. |
| **ImageGlass** | GPL-3.0 core + paid Pro | .NET/Avalonia AOT | See §2. |

None of the open ones is Apache-2.0 compatible except Oculante (MIT), so everything else is ideas only.

---

## 4. Tech-stack recommendation

### 4.1 Measured baseline (box, Linux, offscreen, warm cache, Python 3.14.8, PySide6 6.12.0, Pillow 12.3.0)
- `import PySide6.QtWidgets` ≈ **85 ms**; QApplication + decode of a 12 MP JPEG (Qt) + first paint ≈ **160–185 ms** end-to-end.
- Pillow decode of 12 MP: JPEG 55 ms, AVIF 75 ms, JXL (pillow-jxl-plugin) 147 ms, WebP 170 ms (single-threaded).
- Stock PySide6 wheels' Qt formats: bmp, gif, ico, jpeg, png, svg(z), tga, tiff, wbmp, **webp**, xpm/pbm/ppm/icns. **No AVIF/JXL/HEIF/EXR/RAW**, so add KImageFormats or our own loader.
- Caveats: Windows cold start with a frozen app (Defender scan, DLL loading) is typically 2–4× worse. **Nuitka standalone (onedir), never onefile.** It has to be measured on the laptop.

### 4.2 Options

| | PySide6 / Qt Widgets | C++ / Qt 6 | Rust (egui / iced / Slint) |
|---|---|---|---|
| Cold start to first pixel | ~150–400 ms (interpreter + shiboken import) | ~50–120 ms | ~30–80 ms |
| Suite consistency | **Same as every other app**: theme, i18n, Acrylic helper, packaging, bots' know-how | Same Qt widgets/QSS/theme tokens; different language | New toolkit; theme and Acrylic re-implemented (`window-vibrancy`) |
| Decoders | Qt plugins (C++) + KImageFormats; heavy formats in C libs | Same, direct | image-rs, `jxl-oxide`, `dav1d`/libavif, `libraw-rs`, `exr` crate: good, all permissive |
| GPU | Qt RHI (QRhiWidget, D3D12/Vulkan), QOpenGLWidget | Same | wgpu |
| Licences | Qt/PySide6 LGPL-3 (dynamic linking OK with Apache-2.0) | Same | egui/iced MIT/Apache; **Slint is GPL-3/royalty-free/commercial, so avoid** |
| Risk | Start-up budget | More C++ build work (MSVC, vcpkg) | Toolkit split, smaller widget set (dialogs, settings) |

### 4.3 Recommendation
1. **Build on PySide6 + Python 3.14** for consistency, under a **start-up budget gate**: cold first pixel ≤ 250 ms, next/prev image ≤ 30 ms (cached) on the 4090 laptop, Windows 11 24H2. Start with a 1–2 day spike that measures it in a Nuitka build.
   - Keep Python out of the hot path: Qt decodes (C++ plugins) into `QImage`; Python only orchestrates. Lazy-import everything not needed for the first paint (settings, tools, EXIF, Enlarge bridge).
   - **Single-instance** via QLocalServer: a second double-click forwards the path to the running window (≈ 10 ms). Optional "stay resident in tray" (opt-in, not ImageGlass-style Startup Boost by default).
   - Look-ahead cache (±2 images, memory cap in MB) on a thread pool; downscaled preview first, then full res.
   - Render via `QRhiWidget` (D3D12 on Windows, Vulkan on Linux): textures plus mip-mapped/Lanczos scaling in a shader, with a nearest-neighbour toggle for pixel peeping.
2. **Plan B** if the gate fails: port the viewer core to **C++/Qt 6** with the same QSS theme, same KImageFormats, same IPC spec. Settings/tools could stay PySide6 as separate processes if useful.
3. Rust/egui: only if we ever want a radically minimal, non-Qt viewer. Not recommended for suite coherence.

### 4.4 Decoding libraries (all compatible with an Apache-2.0 app when linked dynamically)
| Format | Library | Licence | Notes |
|---|---|---|---|
| AVIF (incl. HDR, animated) | libavif + **dav1d** | BSD-2 / BSD-2 | via KImageFormats `kimg_avif` or Pillow ≥ 11.3 (built-in) |
| JPEG XL | **libjxl** | BSD-3 | KImageFormats `kimg_jxl`; Pillow needs `pillow-jxl-plugin` (MIT/Apache) |
| WebP (animated) | libwebp | BSD-3 | built into Qt (qtimageformats) and Pillow |
| HEIF/HEIC | **system codec only**: Windows WIC (HEIF + HEVC Video Extensions from the Store), Linux system `libheif` | (libheif LGPL-3; HEVC patents) | Don't bundle libde265/x265. Detect, and show "install the codec" if missing |
| RAW | **LibRaw** | LGPL-2.1 / CDDL | KImageFormats `kimg_raw`; show the embedded JPEG preview first |
| EXR | **OpenEXR** | BSD-3 | KImageFormats `kimg_exr`; tone-map for SDR displays |
| JPEG | libjpeg-turbo | BSD/IJG | Qt default |
| PNG/APNG, GIF, TIFF, SVG | Qt built-ins (QtSvg for SVG) | LGPL-3 | APNG needs a small custom path |
| Colour management | **lcms2** | MIT | Convert from embedded ICC to the **monitor profile** (Windows: `GetICMProfile`/WCS per monitor; Linux: colord/Wayland colour-management). Qt 6 `QColorSpace` covers sRGB/P3/linear; lcms2 for arbitrary ICC |
| EXIF/XMP | **exiv2** (GPL-2+, **avoid**) → use **Pillow EXIF / `exif` lib**, or **libexif** (LGPL-2.1), or Qt's built-in orientation handling | | Respect EXIF orientation everywhere |
| Metadata JSON | own minimal parser | | |

**KImageFormats** (KDE, LGPL-2.1+) gives Qt plugins for AVIF, JXL, HEIF, EXR, RAW, PSD, QOI, HDR, KRA/ORA, PCX, etc. We'd ship it as plugins next to PySide6, built in CI (vcpkg on Windows, distro or flatpak runtime on Linux). Security idea from Loupe/glycin: decode untrusted exotic formats (RAW/PSD/EXR) in a **helper subprocess** so a decoder crash doesn't kill the viewer.

### 4.5 Forward-only platform choices
- Windows 11 24H2+ x64 only (ARM64 build "not now, not blocked"), Linux 2025+ (Wayland first, X11 still works through Qt).
- **GPU:** D3D12/Vulkan via Qt RHI work on any modern GPU, iGPU included. **Recommendation: don't require RTX 40 for the viewer.** Hide or disable the "Enlarge" action unless Enlarge is installed *and* reports a supported GPU. **Decision needed from Aleksandr**: the suite rule says "won't start without a supported GPU".
- HDR output (scRGB swap chain on Windows) is a later milestone; v1 does SDR tone-mapping.
- Default save/export formats: AVIF/JXL/WebP (lossless crop/rotate where the format allows: JPEG lossless via libjpeg-turbo `jpegtran`, JXL lossless).

### 4.6 File associations
**Windows 11** (an app can't silently become default; the user picks):
- Per-user install (HKCU, no admin), or an MSIX with `uap:FileTypeAssociation` (cleaner, but MSIX sandboxing complicates the Enlarge IPC and custom icons; per-user Inno/WiX is simpler).
- Register a ProgID per family, e.g. `Voxprint.Viewer.avif` → `HKCU\Software\Classes\Voxprint.Viewer.avif\shell\open\command = "...\VoxprintViewer.exe" "%1"`, plus `DefaultIcon`, `FriendlyTypeName`.
- `HKCU\Software\Classes\.avif\OpenWithProgids\Voxprint.Viewer.avif` (adds us to "Open with" without hijacking).
- `HKCU\Software\Voxprint\Viewer\Capabilities\FileAssociations` (ext → ProgID) + `ApplicationName/Description` + `HKCU\Software\RegisteredApplications\Voxprint Viewer = Software\Voxprint\Viewer\Capabilities`.
- `HKCU\Software\Classes\Applications\VoxprintViewer.exe\SupportedTypes`.
- Call `SHChangeNotify(SHCNE_ASSOCCHANGED)`, then a "Make default" button that opens `ms-settings:defaultapps?registeredAppUser=Voxprint%20Viewer` (Win11 deep link to our app's defaults page).
- App Paths key `HKCU\...\App Paths\VoxprintViewer.exe` so `start VoxprintViewer` works.
**Linux**:
- `ai.voxprint.Viewer.desktop` with `MimeType=image/avif;image/jxl;image/webp;image/heif;image/png;image/jpeg;image/gif;image/tiff;image/x-exr;image/x-adobe-dng;...` and `Exec=voxprint-viewer %F`; `update-desktop-database`. Offer `xdg-mime default ai.voxprint.Viewer.desktop image/avif ...` from a settings button.
- Ship `shared-mime-info` XML only for types the distro lacks. Flatpak (`org.kde.Platform` runtime has KImageFormats) is the preferred package; AppImage optional.

---

## 5. Enlarge integration design

**Goals:** a one-click "Enlarge…" (toolbar plus context menu, hotkey `E`) that works only when Enlarge is installed; no network; no URI scheme; result shown back in the viewer.

### 5.1 Discovery (Enlarge registers itself on install; the viewer only reads)
- **Windows:** `HKCU\Software\Voxprint\Enlarge` → `InstallPath`, `Exe`, `Version`, `IpcName`, `ProtocolVersion` (+ fallback to the App Paths key `voxprint-enlarge.exe`). Check that the file exists and its Authenticode signer matches ours (later, once we sign).
- **Linux:** look for `ai.voxprint.Enlarge.desktop` in `$XDG_DATA_HOME/applications` and `$XDG_DATA_DIRS/*/applications` (or Flatpak export dirs), and read a custom key `X-Voxprint-Ipc=` / `X-Voxprint-Protocol=1`; fall back to `voxprint-enlarge` on `PATH`. In Flatpak, launch via `flatpak run ai.voxprint.Enlarge` (portal-friendly).
- A shared tiny module `voxprint_suite.discovery` (in a common package used by all apps) so every app finds every other app the same way. This also gives Movie Dubber/Clarity and others cross-launch for free.
- Capability probe: `voxprint-enlarge --capabilities --json` → `{"protocol":1,"gpu_ok":true,"models":[...],"max_scale":4,"input_formats":[...]}`. Cache it for a day; this lets the viewer grey out the button with a tooltip ("needs RTX 40+").

### 5.2 Hand-off protocol
- **Transport:** `QLocalSocket`/`QLocalServer` (named pipe `\\.\pipe\voxprint-enlarge-<user-SID-hash>` on Windows with a current-user-only ACL; `$XDG_RUNTIME_DIR/voxprint/enlarge.sock` mode 0600 on Linux). Messages are JSON-lines, one object per line, with a `protocol` version field.
- **Flow:**
  1. Viewer connects. If there's no listener, it spawns `voxprint-enlarge --serve --hidden` (or `--ipc-name ...`), waits ≤ 3 s for the socket, then connects.
  2. `→ {"type":"enlarge.request","id":"<uuid>","input":"C:/…/photo.avif","preset":"photo","scale":2,"output_dir":null,"open_ui":false,"reply":true}`
  3. `← {"type":"accepted","id":…,"queue_pos":0}` → `← {"type":"progress","id":…,"pct":42,"stage":"tiles"}` → `← {"type":"done","id":…,"output":"C:/…/photo.enlarged-2x.avif","width":…,"height":…,"ms":…}` or `{"type":"error","code":"gpu_unsupported|oom|bad_input|cancelled","message":…}`
  4. `→ {"type":"cancel","id":…}` supported.
- **Two modes in the UI:** "Quick enlarge" (headless: progress bar in the viewer's status area, Enlarge stays hidden) and "Open in Enlarge…" (`open_ui:true`: Enlarge window opens with the image loaded so the user can choose a model or settings; the result still comes back over the socket if the viewer is still connected).
- **CLI fallback** (no IPC, scripts, file managers): `voxprint-enlarge --input <file> --scale 2 --preset photo --output <file> --json-progress`, which streams the same JSON objects on stdout, exit code 0/≠0. The viewer can use this path too if the socket fails.
- **Result:** pass a file path, never pixels. Default output goes next to the source as `name.enlarged-2x.<same modern format>`, never overwriting. The viewer opens it in a **before/after compare view** (split slider, synced zoom/pan, nomacs-style sync) with "Keep", "Reveal in folder" and "Discard" (moves to the Recycle Bin/trash).
- **Why not a URI scheme (`voxprint-enlarge://`):** any web page could trigger it (needs extra hardening, prompts) and it can't return a result. Not needed for local app-to-app. **Why not COM/D-Bus:** platform-specific; a local socket plus JSON works the same on both OSes and from Python.
- Spec lives as `docs/suite-ipc.md` in a shared place, versioned; Enlarge implements the server side (that's a task for the Enlarge bot later, not part of this research).

---

## 6. Names

Siblings: Audiobook Builder, Movie Dubber, Clarity, Enlarge, Plotweaver (mix of plain descriptive names and single evocative words). Quick checks used web search plus GitHub repo search for "<name> image viewer" (2026-10-10). This isn't a trademark search.

| Candidate | Clash check | Verdict |
|---|---|---|
| **Voxprint AI Lumen** | No notable image viewer. "Lumen" is used by Laravel Lumen (PHP framework) and a React media editor SDK (lumeneditor.com), both other categories; a few tiny GitHub hobby repos. | **Top pick**: light/brightness, short, pairs with Clarity/Enlarge |
| **Voxprint AI Optic** | No image-viewer repos found. Generic word. | Strong |
| **Voxprint AI Viewer** | Descriptive and unique with the prefix. | Safest; matches "Movie Dubber" plainness |
| **Voxprint AI Lucent** | No viewer found. Historic Lucent Technologies brand (Nokia-owned, telecom). | Good, slight legacy-brand echo |
| **Voxprint AI Glance** | Tiny hobby viewers; **"Glance" is a popular self-hosted dashboard (glanceapp, ~37k★)**. | OK, but noisy search |
| **Voxprint AI Prism** | **`djieff/prism` is an OCIO image viewer** (small); also Prism.js, Telestream PRISM, the PRISM programme. | Avoid |
| **Voxprint AI Iris** | Iris PTZ camera software (AI, imaging-adjacent); hobby repos. | Avoid |
| ~~Loupe~~ | **GNOME's default Image Viewer is Loupe** (`org.gnome.Loupe`). | Don't use |

**ImageGlass branding note:** "ImageGlass" is a claimed trademark (name and logo). Avoid "Glass" in the name (e.g. "PicGlass", "VoxGlass"), the "Kobe" theme name, the ExifGlass naming style, and a lookalike logo or teal palette. Our blue + yellow Acrylic theme and a "Voxprint AI …" name are clearly distinct. Don't describe it as an "ImageGlass fork/replacement" in the README; "inspired by classic Windows viewers" or no mention at all is fine.

---

## 7. MVP feature list (v0.1)

1. Instant open (single instance, start-up budget gate, lazy imports), folder browsing ←/→ with natural or Explorer-like sort, look-ahead cache.
2. Formats: JPEG, PNG/APNG, GIF, WebP, **AVIF, JXL** (first-class, incl. animation and HDR → SDR tone map), TIFF (multi-page), SVG, BMP/ICO, **HEIF via system codec**, RAW (embedded preview, then full LibRaw), EXR/HDR.
3. Colour management: embedded ICC → monitor profile (lcms2), EXIF orientation.
4. Zoom: fit, fill, 1:1, fit width/height, cursor-anchored wheel zoom, smooth vs nearest-neighbour, pan with inertia; Window-Fit and Frameless modes; fullscreen; slideshow.
5. Frame navigator for animations / multi-page (play/pause, step, export frame as AVIF/PNG).
6. Info panel: dimensions, format, bit depth, colour profile, EXIF/GPS summary (read-only).
7. Basic tools: rotate/flip (lossless where possible), crop, colour picker (hex/RGB copy), copy image/path, delete to Recycle Bin, open containing folder.
8. **"Enlarge…"** integration (discovery, quick/advanced hand-off, progress, before/after compare).
9. Suite blue + yellow theme on Acrylic (Win11) / plain on Linux, EN + RU, remappable hotkeys.
10. File associations helper (Windows Capabilities + Settings deep link; Linux `.desktop` + xdg-mime).
11. Zero network: no update check, no telemetry. Updates via installer/Flatpak only. Settings in a plain TOML/JSON file.

Post-MVP: thumbnail grid/strip, compare two arbitrary images, HDR output on HDR displays, batch convert to AVIF/JXL, sandboxed decoder subprocess, Clarity hand-off (denoise), ARM64 builds.

## 8. Open decisions for Aleksandr
1. Does the viewer get an exception to the "won't start without RTX 40" rule? (Recommended: yes; only Enlarge features need RTX.)
2. Name (top 3: Lumen / Optic / Viewer).
3. PySide6 with a start-up gate (recommended) vs going straight to C++/Qt.
