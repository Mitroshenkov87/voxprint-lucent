// Copyright 2026 Aleksandr Mitroshenkov
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <QHash>
#include <QImage>
#include <QStringList>
#include <QWidget>

#include <functional>

class QButtonGroup;
class QHBoxLayout;
class QPushButton;

namespace lucent {

/// Bottom filmstrip. Tiles are 88×88 with an 8 px gap. Thumbnails decode off the GUI thread.
class Gallery : public QWidget {
public:
    /// Create an empty strip. The strip itself is 104 px tall.
    explicit Gallery(QWidget *parent = nullptr);

    /// Show `files`, with `current` highlighted. Thumbnails start loading after this.
    void setFiles(const QStringList &files, int current);
    /// Highlight `current` without reloading the folder.
    void setCurrent(int current);
    /// Called with the index the user clicked.
    std::function<void(int)> onChosen;

private:
    void pump();
    void rebuild();

    QStringList files_;
    QHash<QString, QImage> thumbs_;
    QHash<QString, int> pending_;
    QList<QPushButton *> buttons_;
    QWidget *row_ = nullptr;
    QHBoxLayout *rowLayout_ = nullptr;
    QButtonGroup *group_ = nullptr;
    int current_ = -1;
    int workers_ = 0;
    int generation_ = 0;
};

}  // namespace lucent
