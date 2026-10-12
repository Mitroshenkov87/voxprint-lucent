# Viewer chrome reference

Lucent's window layout follows the public behaviour of [ImageGlass](https://imageglass.org/): a slim top toolbar, a right-click menu with the same commands plus clipboard and reveal, a bottom thumbnail strip for the current folder, and an app menu for settings, hotkeys, language, and about. The welcome picture opens when the app is started with no file.

That reference is behaviour only. It comes from ImageGlass's public docs and screenshots, including the [app API list](https://imageglass.org/docs/app-apis) (open, next/previous, zoom modes, rotate, flip, crop, save, delete, slideshow, fullscreen, gallery, main menu) and the [theme-pack colour roles](https://imageglass.org/docs/theme-pack) for a toolbar and a gallery. No ImageGlass source, icons, themes, or assets were read or copied. ImageGlass is GPL-3.0. This repository is Apache-2.0.

## Clean-room rule

Do not read or copy ImageGlass source, icons, themes, or assets. Toolbar icons are original drawings in `cpp/src/icons.cpp`, credited in `NOTICE`. The save family follows the suite artwork in `icons/` (22×22, 1.7 px stroke). The floppy-disk save mark is not used. Colours come from `share/tokens.json` (voxprint-common 0.1.0, "Yesod"). On Windows 11 the top-level window asks for an acrylic backdrop (`DWMWA_SYSTEMBACKDROP_TYPE`); other systems use the suite glass colour.

## What we matched, and the small differences

Toolbar order follows ImageGlass's public command groups: menu and open, previous and next, zoom modes, rotate and flip, crop, save, delete, slideshow, fullscreen, gallery, then Lucent's Enlarge hand-off and settings. The toolbar is shown at launch and hides until the pointer returns to the top edge.

The context menu repeats those actions and adds copy image, copy path, and reveal in folder.

The thumbnail strip is on by default, toggleable, and decodes off the GUI thread only after the first image paint. It is not on the `--benchmark` path.

Settings keeps the welcome image, the thumbnail strip, the slideshow interval, and the language. Hotkeys stay on Ctrl+K. Re-enabling the welcome image resets its counter to zero. The first three launches with no file show `share/welcome.avif`. The fourth does not.

`share/splash.avif` is the attached splash artwork. The app name and version are drawn as text, not baked into the file. The splash is loaded off the GUI thread and is dismissed as soon as the image is presented, so it does not hold the first pixel. `--benchmark` never constructs it.

Save writes `<name>.saved.avif` beside the file. Save a copy writes `<name>-copy.avif`. Export writes `<name>.export.avif`. Download writes `<name>.avif` in the downloads folder. Save as offers AVIF, JPEG XL, and WebP. PNG is not an output. Each icon has a tooltip.

## Startup

`--benchmark` still constructs only `Viewer`. The interactive window creates the toolbar, menus, settings, and thumbnail strip in `armAfterFirstPaint`, after the first image is presented. The CI guard for that warm tiny JPEG stays at 250 ms. The v0.1.x cold first pixel is 450 ms.
