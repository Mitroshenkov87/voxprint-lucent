// Copyright 2026 Aleksandr Mitroshenkov
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <QImage>
#include <QWidget>

#include <chrono>

namespace lucent {

struct LaunchOptions {
    QString image;
    QString navigateNext;
    bool benchmark = false;
};

class Viewer : public QWidget {
public:
    Viewer(LaunchOptions options, std::chrono::steady_clock::time_point started, QWidget *parent = nullptr);
    ~Viewer() override;

    // Opens a path forwarded by a second process. M0 paints it; folder browsing is M1.
    void openPath(const QString &path);

protected:
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    void present(const QImage &image, bool navigateFrame);

    LaunchOptions options_;
    std::chrono::steady_clock::time_point started_;
    QImage image_;
    bool sawImage_ = false;
    bool navigateArmed_ = false;
    std::chrono::steady_clock::time_point navigateStart_{};
};

}  // namespace lucent
