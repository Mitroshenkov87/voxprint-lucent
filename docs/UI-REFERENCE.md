# Viewer shell

Lucent is the viewer variant of the shared VOXPrint shell. The picture is the result. There is no prompt and no primary action. Simple mode is the default.

The window opens at 1200×900 the first time. Later launches restore the last geometry, including maximized and fullscreen. The hard minimum is 640×400. At 1200×900 and at 960×640 the View row and the Enhance row each stay on one line. Toolbar buttons that do not fit fold into More from the right, and More stays visible.

## Simple mode

A simple interface keeps every current feature. The 40 px toolbar carries Open, Save, Previous (Предыдущий), Next (Следующий), Rotate (↻, tooltip Rotate right / Повернуть вправо), Crop, Delete (Удалить), Slideshow, and Fullscreen. Delete asks, then moves the file to the trash. More and the picture's context menu carry save as, save a copy, export, download, fit width, fit height, rotate left, flip, Resize…, thumbnails, frameless, window fit, and Tools. The toolbar actions stay in More as well. Resize… takes a width and height or a percent, with keep aspect and resampling. The full list is `docs/ui/FEATURE-MAP.md`.

Under the toolbar, the View row reads Fit, Fill, and 100%. The Enhance row reads Enhance with Enlarge, then Photo and Drawing (Фото and Рисунок). Those two chips are the Снимки setting: they write Enlarge's `photo` or `art` value, and the Expert combo keeps the same value. The picture fills everything left under those rows.

Enhance with Enlarge writes scale 2 and runs the Enlarge command with the current Photo or Drawing choice. When Enlarge is not installed the chip stays grey, with the reason and Install Enlarge. Choosing the grey chip does not write values.

A drag on the picture still crops it. Previous and Next walk the folder. The status row has an Info toggle. On, it shows the file name, the file size, and the pixel size. The Expert info row shows that same line.

The folder strip is 104 px tall with 88×88 thumbnails and an 8 px gap. It can be hidden. Hidden means it is absent. At the 640×400 minimum the strip collapses first, and that space returns to the picture.

The status row stays under the picture. While Enlarge is running it also shows the step, the total, the stage, and Cancel. Time left appears only when Enlarge reports it. Cancel returns the viewer to idle and keeps the settings. After a finished job the picture offers Keep, Reveal, and Discard. Discard deletes only the enlarged file.

## Expert mode

Expert mode is a boolean in `suite.json`. The panel sits on the right of the picture only. It starts at 280 px, and the divider moves it between 220 and 420. Opening or resizing it shrinks the picture and leaves the toolbar, the chips, the status row, and the strip where they are.

The panel shows the live zoom, colour management and the profile, HDR tone-map, numeric crop, the same resize values as Resize…, rotate, export format and quality, frames, and file info. Photographs and Drawings write the same Enlarge `photo` or `art` value as the Photo and Drawing chips. Open in Enlarge hands the file to Enlarge's window. Copy, reveal, and trash live here. Trash asks before it moves the file.

A hand edit of a chip's values reads "{name} + changes" while the stored choice is still close, and Custom when it is not. Reset restores that choice. The rule is the shared preset rule, applied to `share/presets/lucent.json`.

## Chrome

Colours and the 2 px focus ring come from `share/tokens.json` schema 1. That file, and the shell catalogs in `share/locales`, are the vendored copy of `src/voxprint_common/ui/tokens.json` and `src/voxprint_common/ui/locales/` at voxprint-common `0d1e30de68cc49549a2a94cb3695e5601c17e7db`. The C++ viewer reads those files directly. The window uses the plain colours. Lucent's own phrases live in `share/locales/lucent` (English, Russian, and German). A missing phrase falls back to English, then to the key.

The on-demand helper opens the same surface id, `viewer`, through `ReferenceShell`. Its dependency is `voxprint-common[ui]` at that commit. The native viewer does not import it.

Lucent does not ship models, so it has no MDL dependency.
