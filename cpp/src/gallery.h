// Copyright 2026 Aleksandr Mitroshenkov
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <QHash>
#include <QImage>
#include <QStringList>
#include <QWidget>

#include <functional>

namespace lucent {

/// Bottom filmstrip. Thumbnails decode on background threads after setFiles.
class Gallery : public QWidget {
public:
    /// Create an empty strip.
    explicit Gallery(QWidget *parent = nullptr);

    /// Show `files`, with `current` highlighted. Thumbnails start loading after this.
    void setFiles(const QStringList &files, int current);
    /// Highlight `current` without reloading the folder.
    void setCurrent(int current);
    /// Called with the index the user clicked.
    std::function<void(int)> onChosen;

protected:
    /// Draw the loaded thumbnails and the current-file marker.
    void paintEvent(QPaintEvent *event) override;
    /// Select the thumbnail under the cursor.
    void mousePressEvent(QMouseEvent *event) override;

private:
    void pump();
    int indexAt(int x) const;

    QStringList files_;
    QHash<QString, QImage> thumbs_;
    QHash<QString, int> pending_;
    int current_ = -1;
    int workers_ = 0;
    int generation_ = 0;
};

}  // namespace lucent
