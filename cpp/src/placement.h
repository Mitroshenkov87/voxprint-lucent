// Copyright 2026 Aleksandr Mitroshenkov
// SPDX-License-Identifier: Apache-2.0

#pragma once

namespace lucent {

/// How the image meets the window.
enum class ZoomMode {
    Fit,        ///< Whole image visible, letterboxed.
    Fill,       ///< Window covered, image cropped.
    OneToOne,   ///< One image pixel per screen pixel.
    FitWidth,   ///< Image width matches the window.
    FitHeight,  ///< Image height matches the window.
    Free,       ///< User zoom and pan.
};

/// Scale and the image point drawn at the top-left of the view.
struct Placement {
    /// View pixels per image pixel.
    double scale = 1.0;
    /// Image x drawn at the left edge of the view.
    double originX = 0.0;
    /// Image y drawn at the top edge of the view.
    double originY = 0.0;
};

/// Placement that centres an image drawn at `scale`.
Placement centeredPlacement(double scale, int imageW, int imageH, int viewW, int viewH);

/// Placement for `mode` in a view of `viewW` by `viewH`.
Placement placementFor(ZoomMode mode, int imageW, int imageH, int viewW, int viewH);

/// Zoom `current` by `factor` so the image point under the cursor stays put.
Placement zoomAtCursor(const Placement &current, double factor, double cursorX, double cursorY);

/// Move `current` by a view-pixel delta.
Placement panBy(const Placement &current, double dxView, double dyView);

}  // namespace lucent
