# Architecture

Lucent is two programs.

`voxprint-lucent` is a C++ and Qt 6 application. It owns startup, the single running instance, decoding, the image cache, and the shell. This is the program that opens a picture. It does not import Python, and PySide6 is not on the cold-start path. `--benchmark` constructs `Viewer` only.

`voxprint-lucent-tools` is a Python process that uses PySide6. It starts when you open Enlarge's window, the shared viewer screen, or check for updates. It is not running while the first picture is painted. The helper depends on `voxprint-common[ui]` at commit `0d1e30de68cc49549a2a94cb3695e5601c17e7db` and opens that screen with `ReferenceShell(app="viewer")`.

## Shell

The interactive window is the viewer variant of the shared shell. Simple mode is a toolbar (Open, Save, Previous / Предыдущий, Next / Следующий), the Fit / 100% / Enhance with Enlarge chips, and the picture. Expert mode adds a C++ side panel on the right of the picture. The panel reads and writes the same settings document as the chips.

Preset documents use schema `voxprint-presets/1`. Lucent's file is `share/presets/lucent.json`. Choosing a chip writes only that option's keys. A close hand edit reads "{name} + changes". A far edit reads Custom. Reset restores the stored option. Reserved safety keys are rejected.

## State

Suite state is `<data home>/state/suite.json`. The data home is `%LOCALAPPDATA%\VOXPrint AI Suite` on Windows and `${XDG_DATA_HOME:-~/.local/share}/voxprint-ai-suite` on Linux. `VOXPRINT_HOME`, when set and not empty, replaces that directory. The spelling of the directory lives in one function in each program: `dataHome()` in `cpp/src/shell_logic.cpp` and `data_home()` in `voxprint_lucent/suite_state.py`.

`ui.expert_mode` is a boolean in `suite.json`. The viewer screen's surface id is `viewer`. The native document is `<data home>/state/viewer/image-settings.json`, schema 1, with `values`, `chosen`, and `overrides`, plus `ui.geometry` and `ui.history_visible`. The helper's reference screen writes `viewer-settings.json` in that same directory, which is the file name `state_names` gives the viewer variant.

A writer locks `<file>.lock`, re-reads, changes only its own keys, writes a temporary file, fsyncs it, and replaces the destination. A reader keeps the last good document when the file is invalid. A missing file means the defaults.

## Startup

1. `--help`, `--version`, `--dry-run`, and `--selftest` finish before any window is created.
2. Otherwise the process takes a per-user local socket. If Lucent is already running, the new process sends the picture path to it and exits.
3. The viewer decodes the picture and paints it. The toolbar, chips, expert panel, and thumbnail strip are created after that first paint.
4. The tools process starts later, from Open in Enlarge or Check for updates, and only then.

`--benchmark` measures that first paint and exits. It does not start the tools process. The cold first-pixel gate is 450 ms. The warm gate is 250 ms.

The first launch uses 1200×900. After that the window restores `ui.geometry`. The viewer's minimum is 640×400. Fullscreen and slideshow restore that geometry when they end.

## Enlarge

Enlarge is a separate application. Lucent does not link it in.

Discovery looks for an installed copy: the per-user registry on Windows, a `.desktop` file on Linux, then `voxprint-enlarge` on `PATH`. `LUCENT_ENLARGE_EXE` overrides that search for a test. The executable must exist. Flatpak is not searched.

Enhance with Enlarge runs:

```text
voxprint-enlarge --input FILE --output FILE --scale 2 --preset photo --quality-tier fast --format avif --quality N --json-progress
```

Lucent reads the JSON lines Enlarge prints. Cancel kills that process. Open in Enlarge still starts the tools process, which opens Enlarge's window. `--dry-run` on Lucent checks discovery and prints one JSON object. It does not open a window.

## Theme

The C++ viewer reads `share/tokens.json` and `share/locales`, schema 1, vendored from voxprint-common commit `0d1e30de68cc49549a2a94cb3695e5601c17e7db` at `src/voxprint_common/ui/tokens.json` and `src/voxprint_common/ui/locales/`. It does not import that package. The pin file is `share/voxprint-common.pin.json`. Changing that commit is the re-pin. The C++ build writes `lucent.qss` from `share/lucent.qss.in` and those tokens. The focus ring is 2 px in the accent colour. Lucent has no models and does not use MDL.
