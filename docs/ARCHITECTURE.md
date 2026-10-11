# Architecture

Lucent is two programs.

`voxprint-lucent` is a C++ and Qt 6 application. It owns startup, the single running instance, decoding, the image cache, and drawing. This is the program that opens a picture.

`voxprint-lucent-tools` is a Python process that uses PySide6. It starts when you open the info panel, settings, or Enlarge. It is not running while the first picture is painted.

## Startup

1. `--help`, `--version`, `--dry-run`, and `--selftest` finish before any window is created.
2. Otherwise the process takes a per-user local socket. If Lucent is already running, the new process sends the picture path to it and exits.
3. The viewer decodes the picture and paints it. Toolbar, menus, and the thumbnail strip are created after that first paint.
4. The tools process starts later, from the menu, and only then.

`--benchmark` measures that first paint and exits. It does not start the tools process.

## Enlarge

Enlarge is a separate application. Lucent does not link it in.

Discovery looks for an installed copy: the per-user registry on Windows, a `.desktop` file on Linux, then `voxprint-enlarge` on `PATH`. `LUCENT_ENLARGE_EXE` overrides that search for a test. The executable must exist. Flatpak is not searched.

A found install is asked:

```text
voxprint-enlarge --capabilities --json
```

The answer is cached against that version. The Enlarge action is available when the answer reports a usable GPU. When nothing is installed, the interface links to the Enlarge releases page.

A job is handed off on the command line, for example:

```text
voxprint-enlarge --input FILE --scale 2 --preset photo --quality-tier fast --format avif --json-progress
```

Lucent reads the JSON lines Enlarge prints. `--dry-run` on Lucent checks this path and prints one JSON object. It does not open a window. The tools process can also open the Enlarge window for the current file.

## Theme

Both programs read `share/palette.json`. The keys (`ACCENT`, `ROOT_PLAIN`, and the rest) are the colour tokens. The C++ viewer fills `share/lucent.qss.in` with those tokens at startup. The tools process builds the same stylesheet from the same file, so the two windows use the same colours.
