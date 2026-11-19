# Viewer shell

Lucent is the viewer variant of the shared VOXPrint shell. The picture is the result. There is no prompt and no primary action. Simple mode is the default.

The window opens at 1200×900 the first time. Later launches restore the last geometry, including maximized and fullscreen. The hard minimum is 640×400. The layout follows a viewer-first concept: a slim toolbar, the picture filling the window, and a thumbnail strip along the bottom that can be hidden. The artwork and the words are Lucent's own.

## Simple mode

A simple interface keeps every current feature. The toolbar carries Open, Previous, Next, Zoom, Rotate, Compare, Enlarge, and Settings. Expert mode is a checkable button at the top right, beside More, and it stays when the bar overflows. More and the picture's context menu carry Save, save as, save a copy, export, Save to downloads, fullscreen, slideshow, crop, fit width, fit height, rotate left, flip, Resize…, thumbnails, frameless, window fit, and Tools. Move to trash asks, then moves the file to the trash. Save to downloads writes the current view into the downloads folder. Resize… takes a width and height or a percent, with keep aspect and resampling. The full list is `docs/ui/FEATURE-MAP.md`.

Under the toolbar, one preset row reads Fit, 100%, Enhance, Photo, and Drawing. Photo and Drawing write Enlarge's `photo` or `art` value, and the Expert combo keeps the same value. The picture fills everything left under that row. Compare shows the Keep, Reveal, and Discard bar after an Enlarge result.

Enlarge stays on the toolbar. It writes scale 2 and runs the Enlarge command with the current Photo or Drawing choice. When Enlarge is not installed, the button opens a confirm dialog: "Enhancing needs VOXPrint Enlarge. Install it?" with Install and Cancel. The download size is added when the suite manifest, an MDL catalog, or the Enlarge release metadata lists it. Install then shows progress with Cancel and downloads the package through `voxprint-lucent-tools --install-enlarge`. If that download cannot start, the releases page opens. The tools window does not run a capability probe until its Check button is pressed. Edits turn that gate red, the probe runs in the background with progress and Cancel, and the process is unloaded when it finishes.

A drag on the picture still crops it. Previous and Next walk the folder. The status row has an Info toggle. On, it shows the file name, the file size, and the pixel size. The Expert info row shows that same line.

The folder strip is 104 px tall with 88×88 thumbnails and an 8 px gap. It can be hidden. Hidden means it is absent. At the 640×400 minimum the strip collapses first, and that space returns to the picture.

The status row stays under the picture. While Enlarge is running it also shows the step, the total, the stage, and Cancel. Time left appears only when Enlarge reports it. Cancel returns the viewer to idle and keeps the settings. After a finished job the picture offers Keep, Reveal, and Discard. Discard deletes only the enlarged file.

## Expert mode

Expert mode is a boolean in `suite.json`. The panel sits on the right of the picture only. It starts at 280 px, and the divider moves it between 220 and 420. Opening or resizing it shrinks the picture and leaves the toolbar, the chips, the status row, and the strip where they are.

The panel shows the live zoom, colour management and the profile, HDR tone-map, crop, the same resize values as Resize… under the label Size, rotate, export format and quality, frames, and file info. Crop reads No crop until a crop exists, then the x/y/w/h fields. Photographs and Drawings write the same Enlarge `photo` or `art` value as the Photo and Drawing chips. Open in Enlarge hands the file to Enlarge's window. Copy, reveal, and trash live here. Trash asks before it moves the file.

A hand edit of a chip's values reads "{name} + changes" while the stored choice is still close, and Custom when it is not. Reset restores that choice. The rule is the shared preset rule, applied to `share/presets/lucent.json`.

## Chrome

Colours and the 2 px focus ring come from `share/tokens.json` schema 1. That file, and the shell catalogs in `share/locales`, are the vendored copy of `src/voxprint_common/ui/tokens.json` and `src/voxprint_common/ui/locales/` at voxprint-common `v0.6.4` (`e47f3dccf84f04102b11c5eab87fdaa16de1655b`). The C++ viewer reads those files directly. The window uses the plain colours. Lucent's own phrases live in `share/locales/lucent` (English and German). Shell catalogs also include Ukrainian and Latvian. Russian is not an interface language. A missing phrase falls back to English, then to the key.

The on-demand helper opens the same surface id, `viewer`, through `ReferenceShell`. Its dependency is `voxprint-common[ui]` at that commit. The native viewer does not import it.

Lucent does not ship models. It reads an MDL catalog only to show an Enlarge download size when that catalog lists one.
