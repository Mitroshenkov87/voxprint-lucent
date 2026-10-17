# Feature map

A simple interface never means less functionality. Every current Lucent feature stays. Everyday actions sit on the simple-mode toolbar, the overflow menu, or the picture's context menu. The rest sit in a dialog or the Expert panel. Keyboard shortcuts keep working in both modes.

The old location is the rc7 chrome (toolbar, application menu, context menu, settings dialog, or the tools process). The new location is this shell.

| Feature | Old location | New location | Mode |
| --- | --- | --- | --- |
| Open a picture | Toolbar, context menu, Ctrl+O | Toolbar, overflow menu, context menu, Ctrl+O | simple |
| Save | Toolbar, application menu, context menu | Toolbar, context menu | simple |
| Save as | Toolbar, application menu, context menu, Ctrl+Shift+S | Overflow menu, context menu, Ctrl+Shift+S | simple |
| Save a copy | Toolbar, application menu, context menu, Ctrl+S | Overflow menu, context menu, Ctrl+S | simple |
| Export the current view | Toolbar, application menu, context menu | Overflow menu, context menu | simple |
| Download a copy | Toolbar, application menu, context menu | Overflow menu, context menu | simple |
| Previous picture | Toolbar, context menu, Left | Toolbar, context menu, Left | simple |
| Next picture | Toolbar, context menu, Right | Toolbar, context menu, Right | simple |
| Fit | Toolbar, context menu zoom menu, 2 | Preset chip, overflow and context zoom menu, 2 | simple |
| 100% / 1:1 | Toolbar, context menu zoom menu, 1 | Preset chip, overflow and context zoom menu, 1 | simple |
| Fill | Toolbar, context menu zoom menu, 3 | Overflow and context zoom menu, 3 | simple |
| Fit width | Toolbar, context menu zoom menu, 4 | Overflow and context zoom menu, 4 | simple |
| Fit height | Toolbar, context menu zoom menu, 5 | Overflow and context zoom menu, 5 | simple |
| Free zoom (wheel, cursor-anchored) | Wheel on the picture | Wheel on the picture | simple |
| Pan | Drag the picture | Drag the picture | simple |
| Rotate left | Toolbar, context menu, Ctrl+Left | Overflow menu, context menu, Ctrl+Left | simple |
| Rotate right | Toolbar, context menu, Ctrl+Right | Overflow menu, context menu, Ctrl+Right; Expert rotate field | simple |
| Flip horizontal | Toolbar, context menu, Ctrl+H | Overflow menu, context menu, Expert button, Ctrl+H | simple |
| Flip vertical | Toolbar, context menu, Ctrl+V | Overflow menu, context menu, Expert button, Ctrl+V | simple |
| Crop (drag) | Toolbar, context menu, Ctrl+Shift+X, drag | Overflow menu, context menu, drag on the picture, Ctrl+Shift+X | simple |
| Numeric crop | Tools window spin boxes | Expert panel crop fields | Expert |
| Move to trash | Toolbar, context menu, Delete | Overflow menu, context menu, Expert button, Delete. Asks before moving. | simple |
| Copy image | Context menu, Ctrl+C | Context menu, Expert button, Ctrl+C | simple |
| Copy path | Context menu, Ctrl+Shift+C | Context menu, Expert button, Ctrl+Shift+C | simple |
| Reveal in folder | Context menu, Ctrl+R | Context menu, Expert button, Ctrl+R | simple |
| Fullscreen | Toolbar, context menu, F11. Escape leaves it. | Overflow menu, context menu, F11. Escape leaves it. | simple |
| Slideshow | Toolbar, context menu, S. Escape stops it. | Overflow menu, context menu, S. Escape stops it. | simple |
| Slideshow seconds | Settings dialog | Expert panel | Expert |
| Thumbnails | Toolbar, settings checkbox, G | Overflow menu, context menu, Expert checkbox, G. Hidden means absent. | simple |
| Frameless | Ctrl+Shift+F | Overflow menu, context menu, Ctrl+Shift+F | simple |
| Window fit | Ctrl+Shift+W | Overflow menu, context menu, Ctrl+Shift+W | simple |
| Tools / info window | I | Overflow menu, context menu, I. Still the on-demand tools process. | simple |
| Info (size, format, profile, EXIF, GPS, frame) | Tools window | Tools window, and the Expert info line | simple |
| Colour sample (click the preview) | Tools window preview | Tools window preview | simple |
| Frame previous / next / play | Tools window | Expert panel, and comma / period | Expert |
| Export one frame (AVIF, JPEG XL, WebP) | Tools window | Tools window Export frame. PNG is still rejected. | simple |
| Rotate and flip that rewrite a JPEG tag | Tools window | Tools window, and the viewer rotate and flip commands | simple |
| Crop to AVIF | Tools window | Tools window, and the viewer crop | simple |
| Compare after Enlarge (Keep, Reveal, Discard) | Enlarge window | Result overlay after Enhance, and the Enlarge tools window | simple |
| Enhance with Enlarge | Enlarge toolbar button | Preset chip. Runs the Enlarge CLI. Grey, with Install Enlarge, when Enlarge is missing. | simple |
| Open in Enlarge | Enlarge toolbar button | Expert button, Ctrl+E. Starts the tools process. | Expert |
| Install Enlarge | Menu and toolbar when Enlarge is missing | Chip reason and Install Enlarge button | simple |
| Photographs / Drawings | Not on the old toolbar | Expert combo. Writes Enlarge `photo` or `art`. | Expert |
| Export format and quality | Save-as dialog and tools export | Expert format and quality, used by Enhance. Save as still picks a file type. | Expert |
| Colour management and profile | Not a separate control | Expert panel | Expert |
| HDR tone-map | Not a separate control | Expert panel | Expert |
| Live zoom percent | Status line | Expert zoom readout and percent field | Expert |
| Language (English, Russian, German) | Application menu, Ctrl+L | Expert combo, Ctrl+L | Expert |
| Hotkey editor | Application menu, Ctrl+K | Expert button, Ctrl+K | Expert |
| Settings | Toolbar, application menu, context menu, Ctrl+Comma | Expert panel. Ctrl+Comma opens it. | Expert |
| Welcome image | Settings checkbox | Expert checkbox | Expert |
| About | Application menu | Overflow menu | simple |
| Check for updates | Not on the old toolbar | Overflow menu. Runs `voxprint-lucent-tools --check-update`. | simple |
| Quit | Q, Escape when not in fullscreen or slideshow | Q, Escape when not in fullscreen or slideshow, and the window close button | simple |
| First launch 1200×900, then restore geometry | Stored window geometry | `ui.geometry` in the viewer settings file. Minimum 640×400. | simple |
| Single running instance | A second launch forwards the path | Unchanged | simple |
| Decode errors (HEIF, RAW/EXR/HDR, unreadable file) | Status text | Status text | simple |
| Preset chip close edit and Reset | Not in rc7 | Chip reads "{name} + changes" or Custom. Reset restores the stored choice. | simple |
| `voxprint-lucent --help` | CLI | CLI. No window. | simple |
| `voxprint-lucent --version` | CLI | CLI. No window. Wins over `--dry-run`, `--benchmark`, and a path. | simple |
| `voxprint-lucent --dry-run [IMAGE]` | CLI | CLI. JSON only. No window, GPU, or tools process. | simple |
| `voxprint-lucent --selftest --json` | CLI | CLI. Codec and runtime checks. No window. | simple |
| `voxprint-lucent --benchmark IMAGE` | CLI | CLI. Prints FIRST_PIXEL and exits. Does not construct the shell. | simple |
| `voxprint-lucent --navigate-next IMAGE` | CLI, with `--benchmark` | CLI, with `--benchmark`. Prints NAVIGATE_PIXEL. | simple |
| `voxprint-lucent IMAGE` | CLI | CLI. Opens the viewer. A second launch forwards the path. | simple |
| Python `--export [DEST]` | CLI | CLI. Saves a copy and quits. Default format AVIF. | simple |
| Python `--format` | CLI, with `--export` | CLI. avif, jxl, webp, jpeg, heic. PNG is rejected. | simple |
| Python `--lossless` | CLI, with `--export` | CLI. Lossless JPEG XL or WebP when that format is selected. | simple |
| Python `--version`, `-h` / `--help`, image path | CLI | CLI | simple |
| `voxprint-lucent-tools --ping` | CLI | CLI. Answers `pong` without Qt. | simple |
| `voxprint-lucent-tools --path` | CLI | CLI. Image for the tools window. | simple |
| `voxprint-lucent-tools --enlarge` | CLI | CLI. Enlarge hand-off instead of the info panel. | simple |
| `voxprint-lucent-tools --frame` | CLI | CLI. Frame index for Enlarge. | simple |
| `voxprint-lucent-tools --check-update` | CLI | CLI. Prints a newer release tag. | simple |
| `voxprint-lucent-tools --shell` | Not in rc7 | CLI. Opens the shared viewer surface. Not on the cold-start path. | simple |

## Open questions

- **Resize.** Lucent has no command that resizes to a typed pixel size. Crop and Enlarge's scale change the pixel size. Where should a resize field live: the Expert panel, or a dialog?
- **Convert.** Save as, Export, the tools window's Export frame, and `--export --format` already write another format. Is a separate convert wizard required, or are those the convert feature?
- **Two-picture compare.** The current compare is the Enlarge result (Keep, Reveal, Discard). Comparing two arbitrary pictures is not a current command. Should that stay out of this shell?
- **CLI mode.** The flags above do not open simple or Expert mode. The table marks them simple because they stay available without Expert.
