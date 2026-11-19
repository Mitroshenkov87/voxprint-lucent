// Copyright 2026 Aleksandr Mitroshenkov
// SPDX-License-Identifier: Apache-2.0

#include "placement.h"

#include <algorithm>
#include <cmath>

namespace lucent {
namespace {

double positiveScale(int image, int view) {
    if (image <= 0 || view <= 0) {
        return 1.0;
    }
    return static_cast<double>(view) / static_cast<double>(image);
}

}  // namespace

Placement centeredPlacement(double scale, int imageW, int imageH, int viewW, int viewH) {
    Placement placement;
    if (scale < 0.01) {
        scale = 0.01;
    }
    placement.scale = scale;
    placement.originX = imageW / 2.0 - viewW / (2.0 * scale);
    placement.originY = imageH / 2.0 - viewH / (2.0 * scale);
    return placement;
}

Placement placementFor(ZoomMode mode, int imageW, int imageH, int viewW, int viewH) {
    if (imageW <= 0 || imageH <= 0 || viewW <= 0 || viewH <= 0) {
        return {};
    }
    const double fit = std::min(positiveScale(imageW, viewW), positiveScale(imageH, viewH));
    const double fill = std::max(positiveScale(imageW, viewW), positiveScale(imageH, viewH));
    switch (mode) {
    case ZoomMode::Fit:
        return centeredPlacement(fit, imageW, imageH, viewW, viewH);
    case ZoomMode::Fill:
        return centeredPlacement(fill, imageW, imageH, viewW, viewH);
    case ZoomMode::OneToOne:
        return centeredPlacement(1.0, imageW, imageH, viewW, viewH);
    case ZoomMode::FitWidth:
        return centeredPlacement(positiveScale(imageW, viewW), imageW, imageH, viewW, viewH);
    case ZoomMode::FitHeight:
        return centeredPlacement(positiveScale(imageH, viewH), imageW, imageH, viewW, viewH);
    case ZoomMode::Free:
        return centeredPlacement(1.0, imageW, imageH, viewW, viewH);
    }
    return {};
}

Placement zoomAtCursor(const Placement &current, double factor, double cursorX, double cursorY) {
    Placement next = current;
    const double scale = current.scale > 0.0 ? current.scale : 1.0;
    const double imageX = current.originX + cursorX / scale;
    const double imageY = current.originY + cursorY / scale;
    next.scale = std::clamp(scale * factor, 0.01, 64.0);
    next.originX = imageX - cursorX / next.scale;
    next.originY = imageY - cursorY / next.scale;
    return next;
}

Placement panBy(const Placement &current, double dxView, double dyView) {
    Placement next = current;
    const double scale = current.scale > 0.0 ? current.scale : 1.0;
    next.originX -= dxView / scale;
    next.originY -= dyView / scale;
    return next;
}

}  // namespace lucent
