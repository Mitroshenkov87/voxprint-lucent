// Copyright 2026 Aleksandr Mitroshenkov
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "placement.h"

#include <QImage>
#include <QWidget>

namespace lucent {

/// GPU view of one image. Absent when the platform has no RHI device.
class RhiSurface : public QWidget {
    Q_OBJECT

public:
    /// Create a surface, or return null when RHI cannot start.
    static RhiSurface *tryCreate(QWidget *parent);
    /// Replace the image drawn on the next frame.
    void setImage(const QImage &image);
    /// Place `image` inside the widget.
    void setPlacement(const Placement &placement);
    /// Mark that a frame was presented.
    void notifyFrame();
    /// Mark that the device failed.
    void notifyFailed();

signals:
    /// The first frame for the current image has been presented.
    void frameReady();
    /// The RHI device can no longer draw.
    void failed();

protected:
    /// Resize the GPU canvas with the widget.
    void resizeEvent(QResizeEvent *event) override;

private:
    explicit RhiSurface(QWidget *parent);
    QWidget *canvas_ = nullptr;
};

}  // namespace lucent
