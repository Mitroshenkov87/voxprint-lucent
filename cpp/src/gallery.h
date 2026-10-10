// Copyright 2026 Aleksandr Mitroshenkov
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <QHash>
#include <QImage>
#include <QStringList>
#include <QWidget>

#include <functional>

namespace lucent {

// Bottom filmstrip. Thumbnails decode on background threads after setFiles.
class Gallery : public QWidget {
public:
    explicit Gallery(QWidget *parent = nullptr);

    void setFiles(const QStringList &files, int current);
    void setCurrent(int current);
    std::function<void(int)> onChosen;

protected:
    void paintEvent(QPaintEvent *event) override;
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
