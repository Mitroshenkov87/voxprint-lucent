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
| Save the current view into the downloads folder | Toolbar, application menu, context menu, labelled Download | Overflow menu and context menu, labelled Save to downloads / В загрузки. It writes the current view as a new file in the downloads folder. It does not fetch a URL. Save a copy writes beside the open file with a -copy suffix. Export writes beside the open file with an .export suffix. | simple |
| Previous picture | Toolbar, context menu, Left | Toolbar and context menu, labelled Previous / Предыдущий, Left | simple |
| Next picture | Toolbar, context menu, Right | Toolbar and context menu, labelled Next / Следующий, Right | simple |
| Fit | Toolbar, context menu zoom menu, 2 | View chip, overflow and context zoom menu, 2 | simple |
| 100% / 1:1 | Toolbar, context menu zoom menu, 1 | View chip, overflow and context zoom menu, 1 | simple |
| Fill | Toolbar, context menu zoom menu, 3 | View chip, overflow and context zoom menu, 3 | simple |
| Fit width | Toolbar, context menu zoom menu, 4 | Overflow and context zoom menu, 4 | simple |
| Fit height | Toolbar, context menu zoom menu, 5 | Overflow and context zoom menu, 5 | simple |
| Free zoom (wheel, cursor-anchored) | Wheel on the picture | Wheel on the picture | simple |
| Pan | Drag the picture | Drag the picture | simple |
| Rotate left | Toolbar, context menu, Ctrl+Left | Overflow menu, context menu, Ctrl+Left | simple |
| Rotate right | Toolbar, context menu, Ctrl+Right | Toolbar button ↻ Rotate / ↻ Повернуть, with a Rotate right tooltip. Overflow menu, context menu, Ctrl+Right, and the Expert rotate field stay. | simple |
| Flip horizontal | Toolbar, context menu, Ctrl+H | Overflow menu, context menu, Expert button, Ctrl+H | simple |
| Flip vertical | Toolbar, context menu, Ctrl+V | Overflow menu, context menu, Expert button, Ctrl+V | simple |
| Crop (drag) | Toolbar, context menu, Ctrl+Shift+X, drag | Toolbar, context menu, drag on the picture, Ctrl+Shift+X. More lists Crop only when the toolbar button does not fit. | simple |
| Numeric crop | Tools window spin boxes | Expert panel. The value reads No crop / Без обрезки until a crop exists, then the x/y/w/h fields. | Expert |
| Resize | No typed pixel size in rc7 | Resize… on the overflow menu and the context menu. Width and height, or a percent, with keep aspect and resampling. The Expert row is labelled Size / Размер and applies the same values with Resize. | simple |
| Convert | Save as, Export, Export frame, `--export --format` | Save as and Export. Export frame and `--export --format` stay. There is no convert wizard. | simple |
| Move to trash | Toolbar, context menu, Delete | Toolbar button Move to trash / В корзину, overflow menu, context menu, Expert button, Delete. Asks, then `QFile::moveToTrash` moves the file to the trash. | simple |
| Copy image | Context menu, Ctrl+C | Context menu, Expert button, Ctrl+C | simple |
| Copy path | Context menu, Ctrl+Shift+C | Context menu, Expert button, Ctrl+Shift+C | simple |
| Reveal in folder | Context menu, Ctrl+R | Context menu, Expert button, Ctrl+R | simple |
| Fullscreen | Toolbar, context menu, F11. Escape leaves it. | Toolbar, context menu, F11. Escape leaves it. More lists it only when the toolbar button does not fit. | simple |
| Slideshow | Toolbar, context menu, S. Escape stops it. | Toolbar, context menu, S. Escape stops it. More lists it only when the toolbar button does not fit. | simple |
| Slideshow seconds | Settings dialog | Expert panel | Expert |
| Thumbnails | Toolbar, settings checkbox, G | Overflow menu, context menu, Expert checkbox, G. Hidden means absent. | simple |
| Frameless | Ctrl+Shift+F | Overflow menu, context menu, Ctrl+Shift+F | simple |
| Window fit | Ctrl+Shift+W | Overflow menu, context menu, Ctrl+Shift+W | simple |
| Tools / info window | I | Overflow menu, context menu, I. Still the on-demand tools process. | simple |
| Info (size, format, profile, EXIF, GPS, frame) | Tools window | Tools window, the Expert info line, and an Info toggle on the status row. The toggle shows one line with the file name, file size, and pixel size. Expert shows that same line. | simple |
| Colour sample (click the preview) | Tools window preview | Tools window preview | simple |
| Frame previous / next / play | Tools window | Expert panel, and comma / period | Expert |
| Export one frame (AVIF, JPEG XL, WebP) | Tools window | Tools window Export frame. PNG is still rejected. | simple |
| Rotate and flip that rewrite a JPEG tag | Tools window | Tools window, and the viewer rotate and flip commands | simple |
| Crop to AVIF | Tools window | Tools window, and the viewer crop | simple |
| Compare after Enlarge (Keep, Reveal, Discard) | Enlarge window | Result overlay after Enhance, and the Enlarge tools window. This is the Enlarge before/after compare. | simple |
| Enhance with Enlarge | Enlarge toolbar button | Enhance row chip, kept enabled. Runs the Enlarge CLI at scale 2 with the current Photo or Drawing choice. When Enlarge is missing, the chip opens the install confirm instead of running the job. | simple |
| Open in Enlarge | Enlarge toolbar button | Expert button, Ctrl+E. Starts the tools process. | Expert |
| Expert mode | Application menu | Checkable Expert mode / Режим эксперта button at the top right, beside More. It stays visible when other toolbar buttons fold into More. | simple |
| Install Enlarge | Menu and toolbar when Enlarge is missing | Confirm dialog from the Enhance chip. English: "Enhancing needs VOXPrint Enlarge. Install it?" with Install and Cancel. Russian: "Для улучшения нужен VOXPrint Enlarge. Установить?" with Установить and Отмена. The download size is added when the suite manifest, an MDL catalog (`shared/mdl.json`, `mdl/catalog.json`, or `mdl/index.json`), or the Enlarge GitHub release metadata lists it. Install then downloads that package with the status progress and Cancel (`voxprint-lucent-tools --install-enlarge`, the same releases feed the updater uses) and opens the file. If the download cannot start, the releases page opens. | simple |
| Photographs / Drawings | Not on the old toolbar | Photo / Drawing chips on the Enhance row (Фото / Рисунок), and the Expert Снимки combo. Both write Enlarge `photo` or `art` and stay on the same value. | simple |
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
| First launch 1200×900, then restore geometry | Stored window geometry | `ui.geometry` in the viewer settings file. Hard minimum 640×400. At 1200×900 and at 960×640 the View and Enhance rows stay on one line each. Toolbar buttons that do not fit fold into More from the right. Expert mode and More stay visible. The picture rectangle starts below the chip rows. | simple |
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
| `voxprint-lucent-tools --enlarge-size` | Not in rc7 | CLI. Prints the Enlarge package size in bytes, or 0. | simple |
| `voxprint-lucent-tools --install-enlarge` | Not in rc7 | CLI. Downloads the Enlarge release package and prints progress. Not on the cold-start path. | simple |
| `voxprint-lucent-tools --shell` | Not in rc7 | CLI. Opens the shared viewer surface. Not on the cold-start path. | simple |

## Decisions

- Resize is the Resize… dialog on the overflow menu and the context menu: width and height, or a percent, with keep aspect and resampling. The Expert row is labelled Size / Размер and applies the same live values with Resize.
- Save to downloads writes the current view into the downloads folder. It is not a URL fetch and it is not Save a copy or Export.
- Save as and Export cover convert. Export frame and `--export --format` stay. There is no convert wizard.
- The Enlarge before/after compare stays as it is. Comparing two arbitrary pictures is not a current feature.
- The CLI flags stay as they are. The table marks them simple because they stay available without Expert mode.
