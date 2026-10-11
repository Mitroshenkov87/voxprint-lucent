# Voxprint suite palette (canonical)

Approved by Aleksandr, 2026-10-10. This is the **canonical theme for every Voxprint app**: a dark theme on Acrylic (Windows 11), plain dark fallback elsewhere, with **yellow primary actions** and **soft blue menus, lists and accents**.

**Source of truth:** voxprint-common 0.1.0 ("Yesod") `src/voxprint_common/ui/tokens.json`, vendored here as `share/tokens.json`. Lucent does not keep a second palette. The splash colour stays this app's accent (`#60a5fa`). WCAG AA pairs are tested in `tests/test_theme_contrast.py`.

Design rules (from the source):
- Every text colour is opaque (translucent text would pick up the blurred desktop).
- Panels, buttons and inputs are (almost) solid dark fills, so contrast does not depend on the backdrop.
- Only the window background is translucent in Acrylic mode, with a strong dark tint (~87 % opaque).
- All readable text >= 4.5:1 (WCAG AA).

## Yellow: primary actions

| Token | Value | Use |
|---|---|---|
| `PRIMARY` | `#f5d76e` | main action buttons (Start, Narrate), primary card border |
| `PRIMARY_HOVER` | `#f9e291` | primary button hover |
| `PRIMARY_BORDER` | `#fbe8a6` | primary button border |
| `TEXT_ON_PRIMARY` | `#1c1604` | text on primary buttons |
| `PRIMARY_DISABLED_BG` | `#36321f` | disabled primary button |
| `PRIMARY_DISABLED_TEXT` | `#b9b08a` | text on disabled primary button |

## Soft blue: menus, lists, accents

| Token | Value | Use |
|---|---|---|
| `MENU_BG` | `#1c2535` | drop-down lists and menus |
| `MENU_SELECTION` | `#2f4c78` | selected item in menus/lists (white text) |
| `ACCENT` | `#60a5fa` | accents, sliders, checkboxes |
| `ACCENT_HOVER` | `#7db9ff` | accent hover |
| `ACCENT_SOFT` | `#93c5fd` | soft accent, ready chips |
| `ACCENT_STRONG` | `#2563eb` | progress chunk, selection (white text passes AA) |
| `TEXT_ON_ACCENT` | `#0b1220` | dark text on the light-blue accent |

## Text

| Token | Value | Use |
|---|---|---|
| `TEXT` | `#f2f2f5` | primary text |
| `TEXT_MUTED` | `#c4c4d0` | secondary text (subtitle, file name, status) |
| `TEXT_FAINT` | `#a8a8b8` | tertiary text (hints, footer, inactive chips) |
| `TEXT_DISABLED` | `#8e8e9e` | disabled controls (exempt from WCAG, still legible) |

## Surfaces

| Token | Value | Use |
|---|---|---|
| `ROOT_PLAIN` | `#17171c` | window background, plain fallback |
| `ROOT_GLASS` | `rgba(16,16,22,222)` | window background on Acrylic (default) |
| `ROOT_GLASS_MORE` | `rgba(16,16,22,167)` | window background, Settings "more transparency" |
| `CARD_PLAIN` | `#202029` | cards, plain fallback |
| `CARD_GLASS` | `rgba(34,34,44,238)` | cards on Acrylic (default) |
| `CARD_GLASS_MORE` | `rgba(34,34,44,214)` | cards, "more transparency" |
| `CONTROL_BG` | `#2c2c38` | buttons, combo boxes, line edits |
| `CONTROL_HOVER` | `#383846` | control hover |
| `CONTROL_BORDER` | `#4b4b5c` | control borders |

## Status

| Token | Value | Use |
|---|---|---|
| `AMBER` | `#fcd34d` | warnings, "personal use only" accents |
| `NOTE_BG` | `#2a2418` | background of warning notes |
| `NOTE_BORDER` | `#6b5720` | border of warning notes |
| `GREEN` | `#86efac` | success |
| `BADGE_GREEN_TEXT` | `#0b1f12` | text on green badges |
| `BADGE_AMBER_TEXT` | `#251a02` | text on amber badges |
| `RED` | `#fca5a5` | errors (Movie Dubber / Enlarge addition; keep) |

## Per-app identity

The splash progress-bar colour may stay app-specific as app identity: Audiobook Builder `#f5d76e`, Movie Dubber `#b98cff`, Enlarge `#ff6b8a`. Everything else uses the tokens above.

Note: the PDF manual palette in `voxprint-audiobook-builder` `docs/manual/build_manual.py` is a separate print/screen document theme, not the app UI theme.

## Screenshot backdrop

All public screenshots use the Alpenglühen wallpaper 01 (`screenshot-backdrop.jpg` in this folder; artwork by Aleksandr Mitroshenkov, Apache-2.0), pinned as a fixed wallpaper (Fill), not the theme slideshow. Minimise other windows and publish cropped window shots only (no desktop icons, taskbar or personal data).
