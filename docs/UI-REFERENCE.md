# Viewer shell

Lucent is the viewer variant of the shared VOXPrint shell. The picture is the result. There is no prompt and no primary action. Simple mode is the default.

The window opens at 1200×900 the first time, and at a minimum of 640×400 after that. Later launches restore the last geometry, including maximized and fullscreen. Fullscreen and slideshow stay available from the overflow menu.

## Simple mode

A 40 px toolbar carries Open, Save, Previous, and Next. Under it, one row of preset chips reads Fit, 100%, and Enhance with Enlarge. The chips scroll sideways; they do not wrap. The picture fills everything left under that row.

Enhance with Enlarge writes scale 2 and Enlarge's `photo` preset, then runs the Enlarge command. When Enlarge is not installed the chip stays grey, with the reason and Install Enlarge. Choosing the grey chip does not write values.

A drag on the picture still crops it. Previous and Next walk the folder.

The folder strip is 104 px tall with 88×88 thumbnails and an 8 px gap. It can be hidden. Hidden means it is absent. At the 640×400 minimum the strip collapses first, and that space returns to the picture.

While Enlarge is running, a 36 px row under the picture shows the step, the total, the stage, and Cancel. Time left appears only when Enlarge reports it. Cancel returns the viewer to idle and keeps the settings. After a finished job the picture offers Keep, Reveal, and Discard. Discard deletes only the enlarged file.

## Expert mode

Expert mode is a boolean in `suite.json`. The panel sits on the right of the picture only. It starts at 280 px, and the divider moves it between 220 and 420. Opening or resizing it shrinks the picture and leaves the toolbar, the chips, the status row, and the strip where they are.

The panel shows the live zoom, colour management and the profile, HDR tone-map, numeric crop and rotate, export format and quality, frames, and file info. Photographs and Drawings write Enlarge's `photo` or `art` value. Open in Enlarge hands the file to Enlarge's window. Copy, reveal, and trash live here. Trash asks before it moves the file.

A hand edit of a chip's values reads "{name} + changes" while the stored choice is still close, and Custom when it is not. Reset restores that choice. The rule is the shared preset rule, applied to `share/presets/lucent.json`.

## Chrome

Colours and the 2 px focus ring come from `share/tokens.json` schema 1, pinned to voxprint-common `f4e95ccfa25780abaf50412cb095a7843725102a`. The window uses the plain colours. Labels come from the shell catalogs (`share/locales`) and Lucent's own catalogs (`share/locales/lucent`, English, Russian, and German). A missing phrase falls back to English, then to the key.

Lucent does not ship models, so it has no MDL dependency.
