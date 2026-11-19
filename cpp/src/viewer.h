// Copyright 2026 Aleksandr Mitroshenkov
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <QImage>
#include <QWidget>

#include <chrono>

namespace lucent {

/// Arguments that select what the viewer opens.
struct LaunchOptions {
    /// Image path, or empty when the user started Lucent with no file.
    QString image;
    /// Second image opened by `--navigate-next` during a benchmark.
    QString navigateNext;
    /// Measure the first paint and exit.
    bool benchmark = false;
};

/// Benchmark window. The interactive viewer is Browser.
class Viewer : public QWidget {
public:
    /// Paint `options.image` and record `started` for the first-pixel line.
    Viewer(LaunchOptions options, std::chrono::steady_clock::time_point started, QWidget *parent = nullptr);
    /// Release the decoded image.
    ~Viewer() override;

    /// Open a path forwarded by a second process.
    void openPath(const QString &path);

protected:
    /// Draw the current image, or a flat background before it arrives.
    void paintEvent(QPaintEvent *event) override;
    /// Leave the benchmark on Escape or Q.
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
