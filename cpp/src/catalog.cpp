// Copyright 2026 Aleksandr Mitroshenkov
// SPDX-License-Identifier: Apache-2.0

#include "catalog.h"

#include "natural.h"

#include <QDir>
#include <QFileInfo>

#include <algorithm>
#include <string_view>

#ifdef Q_OS_WIN
#include <shlwapi.h>
#endif

namespace lucent {
namespace {

bool suffixIs(const QString &suffix, const char *name) {
    return suffix == QLatin1String(name);
}

int compareFileNames(const QString &left, const QString &right) {
#ifdef Q_OS_WIN
    return StrCmpLogicalW(reinterpret_cast<LPCWSTR>(left.utf16()), reinterpret_cast<LPCWSTR>(right.utf16()));
#else
    const QByteArray a = left.toUtf8();
    const QByteArray b = right.toUtf8();
    return naturalCompare(std::string_view(a.constData(), static_cast<size_t>(a.size())),
                          std::string_view(b.constData(), static_cast<size_t>(b.size())));
#endif
}

}  // namespace

bool isViewableImage(const QString &path) {
    const QString suffix = QFileInfo(path).suffix().toLower();
    return suffixIs(suffix, "jpg") || suffixIs(suffix, "jpeg") || suffixIs(suffix, "jpe") || suffixIs(suffix, "png")
        || suffixIs(suffix, "apng") || suffixIs(suffix, "gif") || suffixIs(suffix, "webp") || suffixIs(suffix, "avif")
        || suffixIs(suffix, "avifs") || suffixIs(suffix, "jxl") || suffixIs(suffix, "tif") || suffixIs(suffix, "tiff")
        || suffixIs(suffix, "svg") || suffixIs(suffix, "svgz") || suffixIs(suffix, "bmp") || suffixIs(suffix, "ico")
        || suffixIs(suffix, "heif") || suffixIs(suffix, "heic") || suffixIs(suffix, "exr") || suffixIs(suffix, "hdr")
        || suffixIs(suffix, "pic") || suffixIs(suffix, "raw") || suffixIs(suffix, "cr2") || suffixIs(suffix, "cr3")
        || suffixIs(suffix, "nef") || suffixIs(suffix, "arw") || suffixIs(suffix, "dng") || suffixIs(suffix, "orf")
        || suffixIs(suffix, "rw2") || suffixIs(suffix, "raf") || suffixIs(suffix, "pef");
}

QStringList listFolderImages(const QString &directory) {
    QDir dir(directory);
    const QFileInfoList entries = dir.entryInfoList(QDir::Files | QDir::Readable, QDir::Unsorted);
    QStringList names;
    for (const QFileInfo &info : entries) {
        if (info.fileName().startsWith(QLatin1Char('.'))) {
            continue;
        }
        if (isViewableImage(info.fileName())) {
            names.append(info.fileName());
        }
    }
    std::sort(names.begin(), names.end(), [](const QString &left, const QString &right) {
        return compareFileNames(left, right) < 0;
    });
    QStringList paths;
    paths.reserve(names.size());
    for (const QString &name : names) {
        paths.append(dir.filePath(name));
    }
    return paths;
}

}  // namespace lucent
