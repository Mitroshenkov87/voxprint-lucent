// Copyright 2026 Aleksandr Mitroshenkov
// SPDX-License-Identifier: Apache-2.0

#include "chrome.h"

#include "gallery.h"
#include "hotkeys.h"
#include "icons.h"
#include "phrases.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QEventLoop>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLibrary>
#include <QMenu>
#include <QMessageBox>
#include <QProcess>
#include <QSettings>
#include <QSpinBox>
#include <QTimer>
#include <QToolButton>
#include <QUrl>

#include <algorithm>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

namespace lucent {
namespace {

QString shotDir() {
    const QString dir = qEnvironmentVariable("LUCENT_SCREENSHOT_DIR", QStringLiteral("/tmp/lucent-shots"));
    QDir().mkpath(dir);
    return dir;
}

void saveGrab(QWidget *widget, const QString &name) {
    if (widget == nullptr) {
        return;
    }
    widget->grab().save(shotDir() + QLatin1Char('/') + name + QStringLiteral(".png"));
    QTimer::singleShot(0, qApp, &QApplication::quit);
}

QToolButton *toolButton(QWidget *parent, Icon icon, Text label, const std::function<void()> &fn) {
    auto *button = new QToolButton(parent);
    button->setIcon(lucentIcon(icon));
    button->setToolTip(text(label));
    button->setAutoRaise(true);
    button->setIconSize(QSize(18, 18));
    button->setFixedSize(32, 32);
    QObject::connect(button, &QToolButton::clicked, parent, fn);
    return button;
}

void addGap(QHBoxLayout *layout) {
    auto *line = new QFrame(layout->parentWidget());
    line->setFrameShape(QFrame::VLine);
    line->setFixedHeight(18);
    layout->addWidget(line);
}

}  // namespace

struct Chrome::Impl {
    QWidget *host = nullptr;
    ChromeCallbacks cb;
    QWidget *bar = nullptr;
    Gallery *gallery = nullptr;
    QTimer *hideTimer = nullptr;
    bool pinned = false;
    bool galleryOn = true;
};

void requestAcrylic(QWidget *window) {
#ifdef Q_OS_WIN
    if (window == nullptr) {
        return;
    }
    using SetAttr = HRESULT(WINAPI *)(HWND, DWORD, LPCVOID, DWORD);
    QLibrary dwm(QStringLiteral("dwmapi"));
    auto setAttr = reinterpret_cast<SetAttr>(dwm.resolve("DwmSetWindowAttribute"));
    if (setAttr == nullptr) {
        return;
    }
    HWND hwnd = reinterpret_cast<HWND>(window->winId());
    const int acrylic = 3;  // DWMSBT_TRANSIENTWINDOW
    setAttr(hwnd, 38, &acrylic, sizeof(acrylic));
    const BOOL dark = TRUE;
    setAttr(hwnd, 20, &dark, sizeof(dark));
#else
    Q_UNUSED(window);
#endif
}

Chrome::Chrome(QWidget *host, ChromeCallbacks callbacks) : impl_(new Impl) {
    impl_->host = host;
    impl_->cb = std::move(callbacks);
    QSettings settings;
    impl_->galleryOn = settings.value(QStringLiteral("gallery/visible"), true).toBool();

    impl_->bar = new QWidget(host);
    impl_->bar->setObjectName(QStringLiteral("lucent-toolbar"));
    impl_->bar->setFixedHeight(40);
    impl_->bar->setStyleSheet(QStringLiteral(
        "QWidget#lucent-toolbar { background-color: rgba(16, 16, 22, 210); }"
        "QToolButton { background: transparent; border: none; border-radius: 4px; }"
        "QToolButton:hover { background: #383846; }"));
    auto *row = new QHBoxLayout(impl_->bar);
    row->setContentsMargins(6, 4, 6, 4);
    row->setSpacing(2);

    const auto add = [this, row](Icon icon, Text label, const std::function<void()> &fn) {
        row->addWidget(toolButton(impl_->bar, icon, label, fn));
    };
    add(Icon::Menu, Text::AppMenu, [this] { openMenu(); });
    add(Icon::Open, Text::Open, [this] { openFile(); });
    add(Icon::Previous, Text::Previous, [this] { impl_->cb.step(-1); });
    add(Icon::Next, Text::Next, [this] { impl_->cb.step(1); });
    addGap(row);
    add(Icon::Fit, Text::Fit, [this] { impl_->cb.zoom(ZoomMode::Fit); });
    add(Icon::Fill, Text::Fill, [this] { impl_->cb.zoom(ZoomMode::Fill); });
    add(Icon::OneToOne, Text::OneToOne, [this] { impl_->cb.zoom(ZoomMode::OneToOne); });
    add(Icon::FitWidth, Text::FitWidth, [this] { impl_->cb.zoom(ZoomMode::FitWidth); });
    add(Icon::FitHeight, Text::FitHeight, [this] { impl_->cb.zoom(ZoomMode::FitHeight); });
    addGap(row);
    add(Icon::RotateLeft, Text::RotateLeft, [this] { impl_->cb.rotate(false); });
    add(Icon::RotateRight, Text::RotateRight, [this] { impl_->cb.rotate(true); });
    add(Icon::FlipHorizontal, Text::FlipHorizontal, [this] { impl_->cb.flip(true); });
    add(Icon::FlipVertical, Text::FlipVertical, [this] { impl_->cb.flip(false); });
    add(Icon::Crop, Text::Crop, [this] { impl_->cb.crop(); });
    addGap(row);
    add(Icon::Save, Text::SaveCopy, [this] { impl_->cb.saveCopy(); });
    add(Icon::SaveAs, Text::SaveAs, [this] { impl_->cb.saveAs(); });
    add(Icon::Trash, Text::MoveToTrash, [this] { impl_->cb.trash(); });
    addGap(row);
    add(Icon::Slideshow, Text::Slideshow, [this] { impl_->cb.slideshow(); });
    add(Icon::Fullscreen, Text::Fullscreen, [this] { impl_->cb.fullscreen(); });
    add(Icon::Gallery, Text::Gallery, [this] { setGalleryVisible(!impl_->galleryOn); });
    add(Icon::Enlarge, Text::Enlarge, [this] { impl_->cb.enlarge(); });
    add(Icon::Settings, Text::Settings, [this] { openSettings(); });
    row->addStretch(1);

    impl_->gallery = new Gallery(host);
    impl_->gallery->onChosen = [this](int index) {
        if (impl_->cb.openIndex) {
            impl_->cb.openIndex(index);
        }
    };
    impl_->gallery->setVisible(impl_->galleryOn);

    impl_->hideTimer = new QTimer(host);
    impl_->hideTimer->setSingleShot(true);
    impl_->hideTimer->setInterval(1400);
    QObject::connect(impl_->hideTimer, &QTimer::timeout, host, [this] {
        if (!impl_->pinned && impl_->bar != nullptr && !impl_->bar->underMouse()) {
            impl_->bar->hide();
        }
    });
    impl_->bar->show();
    impl_->hideTimer->start();
    relayout();
    requestAcrylic(host);
}

Chrome::~Chrome() {
    delete impl_;
}

void Chrome::relayout() {
    if (impl_->host == nullptr) {
        return;
    }
    if (impl_->bar != nullptr) {
        impl_->bar->setGeometry(0, 0, impl_->host->width(), impl_->bar->height());
        impl_->bar->raise();
    }
    if (impl_->gallery != nullptr && impl_->galleryOn) {
        impl_->gallery->setGeometry(0, impl_->host->height() - impl_->gallery->height(), impl_->host->width(),
                                     impl_->gallery->height());
        impl_->gallery->raise();
        impl_->gallery->show();
    }
}

void Chrome::mouseAt(const QPoint &pos) {
    if (impl_->bar == nullptr || impl_->pinned) {
        return;
    }
    if (pos.y() <= impl_->bar->height() + 8) {
        impl_->bar->show();
        impl_->bar->raise();
        impl_->hideTimer->stop();
        return;
    }
    if (impl_->bar->isVisible() && !impl_->bar->underMouse()) {
        impl_->hideTimer->start();
    }
}

void Chrome::setFolder(const QStringList &files, int index) {
    if (impl_->gallery != nullptr) {
        impl_->gallery->setFiles(files, index);
    }
}

void Chrome::setCurrent(int index) {
    if (impl_->gallery != nullptr) {
        impl_->gallery->setCurrent(index);
    }
}

int Chrome::galleryHeight() const {
    if (impl_->gallery == nullptr || !impl_->galleryOn || !impl_->gallery->isVisible()) {
        return 0;
    }
    return impl_->gallery->height();
}

void Chrome::setGalleryVisible(bool visible) {
    impl_->galleryOn = visible;
    QSettings settings;
    settings.setValue(QStringLiteral("gallery/visible"), visible);
    if (impl_->gallery != nullptr) {
        impl_->gallery->setVisible(visible);
    }
    if (impl_->cb.galleryVisible) {
        impl_->cb.galleryVisible(visible);
    }
    relayout();
}

void Chrome::openFile() {
    const QString path = QFileDialog::getOpenFileName(impl_->host, text(Text::Open));
    if (!path.isEmpty() && impl_->cb.openPath) {
        impl_->cb.openPath(path);
    }
}

void Chrome::openMenu() {
    impl_->pinned = true;
    impl_->bar->show();
    QMenu menu(impl_->host);
    menu.setObjectName(QStringLiteral("lucent-app-menu"));
    menu.addAction(lucentIcon(Icon::Settings), text(Text::Settings), [this] { openSettings(); });
    menu.addAction(text(Text::Hotkeys), [this] { impl_->cb.hotkeys(); });
    QMenu *languages = menu.addMenu(text(Text::Language));
    languages->addAction(QStringLiteral("English"), [this] { impl_->cb.language(QStringLiteral("en")); });
    languages->addAction(QStringLiteral("Русский"), [this] { impl_->cb.language(QStringLiteral("ru")); });
    languages->addAction(QStringLiteral("Deutsch"), [this] { impl_->cb.language(QStringLiteral("de")); });
    menu.addSeparator();
    menu.addAction(text(Text::About), [this] { openAbout(); });
    const QByteArray shot = qgetenv("LUCENT_SCREENSHOT");
    if (shot == "menu") {
        menu.popup(impl_->bar->mapToGlobal(QPoint(8, impl_->bar->height())));
        QTimer::singleShot(150, &menu, [&menu] { saveGrab(&menu, QStringLiteral("menu")); });
        QEventLoop loop;
        QTimer::singleShot(800, &loop, &QEventLoop::quit);
        loop.exec();
        impl_->pinned = false;
        return;
    }
    menu.exec(impl_->bar->mapToGlobal(QPoint(8, impl_->bar->height())));
    impl_->pinned = false;
    impl_->hideTimer->start();
}

void Chrome::contextMenu(const QPoint &globalPos) {
    impl_->pinned = true;
    if (impl_->bar != nullptr) {
        impl_->bar->show();
    }
    QMenu menu(impl_->host);
    menu.setObjectName(QStringLiteral("lucent-context-menu"));
    const auto add = [&menu](Icon icon, Text label, const std::function<void()> &fn) {
        menu.addAction(lucentIcon(icon), text(label), fn);
    };
    add(Icon::Open, Text::Open, [this] { openFile(); });
    add(Icon::Previous, Text::Previous, [this] { impl_->cb.step(-1); });
    add(Icon::Next, Text::Next, [this] { impl_->cb.step(1); });
    menu.addSeparator();
    QMenu *zoom = menu.addMenu(text(Text::Free));
    zoom->addAction(text(Text::Fit), [this] { impl_->cb.zoom(ZoomMode::Fit); });
    zoom->addAction(text(Text::Fill), [this] { impl_->cb.zoom(ZoomMode::Fill); });
    zoom->addAction(text(Text::OneToOne), [this] { impl_->cb.zoom(ZoomMode::OneToOne); });
    zoom->addAction(text(Text::FitWidth), [this] { impl_->cb.zoom(ZoomMode::FitWidth); });
    zoom->addAction(text(Text::FitHeight), [this] { impl_->cb.zoom(ZoomMode::FitHeight); });
    add(Icon::RotateLeft, Text::RotateLeft, [this] { impl_->cb.rotate(false); });
    add(Icon::RotateRight, Text::RotateRight, [this] { impl_->cb.rotate(true); });
    add(Icon::FlipHorizontal, Text::FlipHorizontal, [this] { impl_->cb.flip(true); });
    add(Icon::FlipVertical, Text::FlipVertical, [this] { impl_->cb.flip(false); });
    add(Icon::Crop, Text::Crop, [this] { impl_->cb.crop(); });
    menu.addSeparator();
    add(Icon::Save, Text::SaveCopy, [this] { impl_->cb.saveCopy(); });
    add(Icon::SaveAs, Text::SaveAs, [this] { impl_->cb.saveAs(); });
    add(Icon::Trash, Text::MoveToTrash, [this] { impl_->cb.trash(); });
    menu.addSeparator();
    add(Icon::Slideshow, Text::Slideshow, [this] { impl_->cb.slideshow(); });
    add(Icon::Fullscreen, Text::Fullscreen, [this] { impl_->cb.fullscreen(); });
    add(Icon::Enlarge, Text::Enlarge, [this] { impl_->cb.enlarge(); });
    menu.addSeparator();
    add(Icon::Copy, Text::CopyImage, [this] { copyImage(); });
    add(Icon::Path, Text::CopyPath, [this] { copyPath(); });
    add(Icon::Reveal, Text::RevealInFolder, [this] { reveal(); });
    add(Icon::Settings, Text::Settings, [this] { openSettings(); });
    if (qgetenv("LUCENT_SCREENSHOT") == "context") {
        menu.popup(globalPos);
        QTimer::singleShot(150, &menu, [&menu] { saveGrab(&menu, QStringLiteral("context")); });
        QEventLoop loop;
        QTimer::singleShot(800, &loop, &QEventLoop::quit);
        loop.exec();
        impl_->pinned = false;
        return;
    }
    menu.exec(globalPos);
    impl_->pinned = false;
}

void Chrome::openSettings() {
    QSettings settings;
    const bool welcomeWas = settings.value(QStringLiteral("welcome/enabled"), true).toBool();
    QDialog dialog(impl_->host);
    dialog.setObjectName(QStringLiteral("lucent-settings"));
    dialog.setWindowTitle(text(Text::Settings));
    dialog.setStyleSheet(impl_->host->styleSheet());
    auto *form = new QFormLayout(&dialog);
    auto *welcome = new QCheckBox(text(Text::WelcomeImage), &dialog);
    welcome->setChecked(welcomeWas);
    auto *gallery = new QCheckBox(text(Text::Gallery), &dialog);
    gallery->setChecked(impl_->galleryOn);
    auto *seconds = new QSpinBox(&dialog);
    seconds->setRange(1, 60);
    seconds->setValue(std::max(1, settings.value(QStringLiteral("slideshow/ms"), 3000).toInt() / 1000));
    auto *language = new QComboBox(&dialog);
    language->addItem(QStringLiteral("English"), QStringLiteral("en"));
    language->addItem(QStringLiteral("Русский"), QStringLiteral("ru"));
    language->addItem(QStringLiteral("Deutsch"), QStringLiteral("de"));
    const int current = language->findData(lucent::language());
    if (current >= 0) {
        language->setCurrentIndex(current);
    }
    form->addRow(welcome);
    form->addRow(gallery);
    form->addRow(text(Text::Slideshow), seconds);
    form->addRow(text(Text::Language), language);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    form->addRow(buttons);
    dialog.resize(420, 220);
    if (qgetenv("LUCENT_SCREENSHOT") == "settings") {
        dialog.show();
        dialog.raise();
        QTimer::singleShot(150, &dialog, [&dialog] { saveGrab(&dialog, QStringLiteral("settings")); });
        QEventLoop loop;
        QTimer::singleShot(800, &loop, &QEventLoop::quit);
        loop.exec();
        return;
    }
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    const bool welcomeNow = welcome->isChecked();
    settings.setValue(QStringLiteral("welcome/enabled"), welcomeNow);
    if (welcomeNow && !welcomeWas) {
        settings.setValue(QStringLiteral("welcome/emptyCount"), welcomeCountAfterReenable());
    }
    setGalleryVisible(gallery->isChecked());
    if (impl_->cb.slideshowMs) {
        impl_->cb.slideshowMs(seconds->value() * 1000);
    }
    settings.setValue(QStringLiteral("slideshow/ms"), seconds->value() * 1000);
    if (impl_->cb.language) {
        impl_->cb.language(language->currentData().toString());
    }
}

void Chrome::openAbout() {
    QMessageBox box(impl_->host);
    box.setWindowTitle(text(Text::About));
    box.setText(QStringLiteral("Voxprint AI Lucent ") + QStringLiteral(LUCENT_VERSION));
    box.setInformativeText(QStringLiteral("Apache License 2.0"));
    box.exec();
}

void Chrome::copyImage() {
    if (!impl_->cb.image) {
        return;
    }
    const QImage image = impl_->cb.image();
    if (!image.isNull()) {
        QGuiApplication::clipboard()->setImage(image);
    }
}

void Chrome::copyPath() {
    if (impl_->cb.path) {
        QGuiApplication::clipboard()->setText(impl_->cb.path());
    }
}

void Chrome::reveal() {
    const QString path = impl_->cb.path ? impl_->cb.path() : QString();
    if (path.isEmpty()) {
        return;
    }
#ifdef Q_OS_WIN
    QProcess::startDetached(QStringLiteral("explorer.exe"),
                             {QStringLiteral("/select,"), QDir::toNativeSeparators(path)});
#else
    QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
#endif
}

void Chrome::showSettings() {
    openSettings();
}

void Chrome::toggleGallery() {
    setGalleryVisible(!impl_->galleryOn);
}

void Chrome::prepareShots() {
    const QByteArray mode = qgetenv("LUCENT_SCREENSHOT");
    if (mode.isEmpty() || mode == "splash") {
        return;
    }
    impl_->pinned = true;
    if (impl_->bar != nullptr) {
        impl_->bar->show();
        impl_->bar->raise();
    }
    relayout();
    if (mode == "toolbar") {
        QTimer::singleShot(200, impl_->host, [this] { saveGrab(impl_->host, QStringLiteral("toolbar")); });
    } else if (mode == "thumbs") {
        setGalleryVisible(true);
        QTimer::singleShot(400, impl_->host, [this] { saveGrab(impl_->host, QStringLiteral("thumbs")); });
    } else if (mode == "context") {
        QTimer::singleShot(200, impl_->host, [this] {
            contextMenu(impl_->host->mapToGlobal(QPoint(impl_->host->width() / 3, impl_->host->height() / 3)));
        });
    } else if (mode == "menu") {
        QTimer::singleShot(200, impl_->host, [this] { openMenu(); });
    } else if (mode == "settings") {
        QTimer::singleShot(200, impl_->host, [this] { openSettings(); });
    } else if (mode == "welcome") {
        QTimer::singleShot(800, impl_->host, [this] { saveGrab(impl_->host, QStringLiteral("welcome")); });
    }
}

}  // namespace lucent
