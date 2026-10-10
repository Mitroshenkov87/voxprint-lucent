// Copyright 2026 Aleksandr Mitroshenkov
// SPDX-License-Identifier: Apache-2.0

#pragma once

namespace lucent {

enum class ZoomMode {
    Fit,
    Fill,
    OneToOne,
    FitWidth,
    FitHeight,
    Free,
};

struct Placement {
    double scale = 1.0;
    double originX = 0.0;
    double originY = 0.0;
};

Placement centeredPlacement(double scale, int imageW, int imageH, int viewW, int viewH);
Placement placementFor(ZoomMode mode, int imageW, int imageH, int viewW, int viewH);
Placement zoomAtCursor(const Placement &current, double factor, double cursorX, double cursorY);
Placement panBy(const Placement &current, double dxView, double dyView);

}  // namespace lucent
