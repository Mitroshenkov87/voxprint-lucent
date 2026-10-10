// Copyright 2026 Aleksandr Mitroshenkov
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "placement.h"

#include <QImage>
#include <QWidget>

namespace lucent {

class RhiSurface : public QWidget {
    Q_OBJECT

public:
    static RhiSurface *tryCreate(QWidget *parent);
    void setImage(const QImage &image);
    void setPlacement(const Placement &placement);
    void notifyFrame();
    void notifyFailed();

signals:
    void frameReady();
    void failed();

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    explicit RhiSurface(QWidget *parent);
    QWidget *canvas_ = nullptr;
};

}  // namespace lucent
