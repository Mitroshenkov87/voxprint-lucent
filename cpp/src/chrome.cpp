// Copyright 2026 Aleksandr Mitroshenkov
// SPDX-License-Identifier: Apache-2.0

#include "chrome.h"

#include "dry_run.h"
#include "gallery.h"
#include "labels.h"
#include "lucent_build.h"
#include "phrases.h"
#include "shell_logic.h"
#include "theme.h"
#include "welcome_policy.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDesktopServices>
#include <QDialog>
#include <QDir>
#include <QEvent>
#include <QEventLoop>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#ifdef Q_OS_WIN
#include <QLibrary>
#include <qt_windows.h>
#endif
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QSpinBox>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace lucent {
namespace {

constexpr int kTop = 40;
constexpr int kPageLeft = 16;
constexpr int kPageTop = 12;
constexpr int kPageRight = 16;
constexpr int kPageBottom = 12;
constexpr int kBarGap = 10;
constexpr int kRowGap = 8;
constexpr int kExpertGap = 12;
constexpr int kExpertDefault = 280;
constexpr int kExpertMin = 220;
constexpr int kExpertMax = 420;
constexpr int kStatus = 36;
constexpr int kHistory = 104;
constexpr int kViewerMinH = 400;

const char *kAppId = kSurfaceId;

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

QString qs(const std::string &text) { return QString::fromStdString(text); }

std::string ss(const QString &text) { return text.toStdString(); }

QString jsonText(const Json &object, const char *key, const QString &fallback = {}) {
    const Json *value = object.field(key);
    if (value == nullptr) {
        return fallback;
    }
    if (value->kind == Json::Kind::String) {
        return qs(value->text);
    }
    if (value->isNumber()) {
        return QString::number(value->asNumber());
    }
    return fallback;
}

int jsonInt(const Json &object, const char *key, int fallback) {
    const Json *value = object.field(key);
    if (value == nullptr || !value->isNumber()) {
        return fallback;
    }
    return static_cast<int>(std::lround(value->asNumber()));
}

bool jsonBool(const Json &object, const char *key, bool fallback) {
    const Json *value = object.field(key);
    if (value == nullptr || value->kind != Json::Kind::Bool) {
        return fallback;
    }
    return value->boolean;
}

Json objectFromValues(const std::map<std::string, Json> &values) {
    Json object;
    object.kind = Json::Kind::Object;
    for (const auto &item : values) {
        object.set(item.first, item.second);
    }
    return object;
}

Json objectFromChosen(const std::map<std::string, std::string> &chosen) {
    Json object;
    object.kind = Json::Kind::Object;
    for (const auto &item : chosen) {
        object.set(item.first, Json::fromString(item.second));
    }
    return object;
}

Json objectFromOverrides(const std::map<std::string, std::vector<std::string>> &overrides) {
    Json object;
    object.kind = Json::Kind::Object;
    for (const auto &item : overrides) {
        Json array;
        array.kind = Json::Kind::Array;
        for (const std::string &key : item.second) {
            array.array.push_back(Json::fromString(key));
        }
        object.set(item.first, std::move(array));
    }
    return object;
}

QString activeLanguage() {
    const QByteArray env = qgetenv("LUCENT_LANG");
    if (!env.isEmpty()) {
        return QString::fromUtf8(env).left(2).toLower();
    }
    const Json suite = readDocument(suiteSettingsPath());
    const Json *language = suite.field("ui_language");
    if (language != nullptr && language->kind == Json::Kind::String && !language->text.empty()) {
        return qs(language->text).left(2).toLower();
    }
    return QStringLiteral("en");
}

QString etaText(double seconds) {
    if (!(seconds > 0.0)) {
        return {};
    }
    const int whole = static_cast<int>(std::lround(seconds));
    if (whole >= 3600) {
        return label(QStringLiteral("mdl.eta.hours"))
            .replace(QStringLiteral("{hours}"), QString::number(whole / 3600))
            .replace(QStringLiteral("{minutes}"), QString::number((whole % 3600) / 60));
    }
    if (whole >= 60) {
        return label(QStringLiteral("mdl.eta.minutes"))
            .replace(QStringLiteral("{minutes}"), QString::number(whole / 60))
            .replace(QStringLiteral("{seconds}"), QString::number(whole % 60));
    }
    return label(QStringLiteral("mdl.eta.seconds")).replace(QStringLiteral("{seconds}"), QString::number(whole));
}

QString stageSentence(const QString &stage) {
    const QString key = QStringLiteral("lucent.stage.") + stage;
    const QString text = label(key);
    return text == key ? stage : text;
}

class SplitHandle : public QFrame {
public:
    explicit SplitHandle(QWidget *parent) : QFrame(parent) {
        setObjectName(QStringLiteral("splitLine"));
        setCursor(Qt::SplitHCursor);
        setFixedWidth(6);
    }

    std::function<void(int)> onDrag;

protected:
    void mousePressEvent(QMouseEvent *event) override {
        last_ = event->globalPosition().toPoint();
        setProperty("hot", true);
        style()->unpolish(this);
        style()->polish(this);
    }

    void mouseMoveEvent(QMouseEvent *event) override {
        const QPoint now = event->globalPosition().toPoint();
        if (onDrag) {
            onDrag(last_.x() - now.x());
        }
        last_ = now;
    }

    void mouseReleaseEvent(QMouseEvent *) override {
        setProperty("hot", false);
        style()->unpolish(this);
        style()->polish(this);
    }

private:
    QPoint last_;
};

class GeometryFilter : public QObject {
public:
    GeometryFilter(QWidget *host, Chrome *chrome, QTimer *timer) : QObject(host), host_(host), chrome_(chrome), timer_(timer) {}

    bool eventFilter(QObject *object, QEvent *event) override {
        if (object == host_
            && (event->type() == QEvent::Resize || event->type() == QEvent::Move || event->type() == QEvent::WindowStateChange)) {
            chrome_->relayout();
            timer_->start();
        }
        if (object == host_ && event->type() == QEvent::Close) {
            chrome_->saveGeometry();
        }
        return QObject::eventFilter(object, event);
    }

private:
    QWidget *host_ = nullptr;
    Chrome *chrome_ = nullptr;
    QTimer *timer_ = nullptr;
};

}  // namespace

struct Chrome::Impl {
    QWidget *host = nullptr;
    ChromeCallbacks cb;
    PresetFile presets;
    SettingState state;
    bool presetsOk = false;
    bool historyOn = true;
    bool expertOn = false;
    bool shotOverride = false;
    bool shotExpert = false;
    int expertWidth = kExpertDefault;
    QRect result;
    LiveView live;
    bool applying = false;
    int rotation = 0;
    bool geometryReady = false;

    QWidget *bar = nullptr;
    QWidget *presetRow = nullptr;
    QHBoxLayout *chips = nullptr;
    QLabel *customLabel = nullptr;
    QPushButton *resetButton = nullptr;
    QLabel *featureText = nullptr;
    QPushButton *installButton = nullptr;
    QFrame *expert = nullptr;
    SplitHandle *split = nullptr;
    QWidget *statusRow = nullptr;
    QLabel *progressText = nullptr;
    QProgressBar *progress = nullptr;
    Gallery *gallery = nullptr;
    QWidget *compare = nullptr;
    QProcess *job = nullptr;
    QByteArray jobBuffer;
    QString jobOutput;
    bool jobCancel = false;
    QTimer *stateTimer = nullptr;
    QObject *filter = nullptr;

    QLabel *zoomValue = nullptr;
    QSpinBox *zoomSpin = nullptr;
    QComboBox *colour = nullptr;
    QLabel *profile = nullptr;
    QComboBox *hdr = nullptr;
    QSpinBox *cropX = nullptr;
    QSpinBox *cropY = nullptr;
    QSpinBox *cropW = nullptr;
    QSpinBox *cropH = nullptr;
    QComboBox *rotate = nullptr;
    QComboBox *format = nullptr;
    QSpinBox *quality = nullptr;
    QLabel *frames = nullptr;
    QLabel *info = nullptr;
    QComboBox *pictures = nullptr;
    QSpinBox *slideshow = nullptr;
    QCheckBox *thumbs = nullptr;
    QCheckBox *welcome = nullptr;
    QComboBox *language = nullptr;
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
    const int acrylic = 3;
    setAttr(hwnd, 38, &acrylic, sizeof(acrylic));
#else
    Q_UNUSED(window);
#endif
}

Chrome::Chrome(QWidget *host, ChromeCallbacks callbacks) : impl_(new Impl) {
    impl_->host = host;
    impl_->cb = std::move(callbacks);
    host->setObjectName(QStringLiteral("shell"));
    loadCatalogs(findShareDir());
    const QString language = activeLanguage();
    setCatalogLanguage(language);
    setLanguage(language);
    loadPresets();
    buildBar();
    buildPresets();
    buildExpert();
    buildStatus();
    buildCompare();
    impl_->gallery = new Gallery(host);
    impl_->gallery->onChosen = [this](int index) {
        if (impl_->cb.openIndex) {
            impl_->cb.openIndex(index);
        }
    };
    impl_->split = new SplitHandle(host);
    impl_->split->onDrag = [this](int delta) { resizeExpert(delta); };
    auto *watch = new QTimer(host);
    watch->setSingleShot(true);
    watch->setInterval(200);
    QObject::connect(watch, &QTimer::timeout, host, [this] { saveGeometry(); });
    auto *filter = new GeometryFilter(host, this, watch);
    impl_->filter = filter;
    host->installEventFilter(filter);
    impl_->stateTimer = new QTimer(host);
    impl_->stateTimer->setSingleShot(true);
    impl_->stateTimer->setInterval(150);
    QObject::connect(impl_->stateTimer, &QTimer::timeout, host, [this] {
        persistState();
        refreshChips();
    });
    impl_->geometryReady = true;
    refreshChips();
    refreshExpert();
    relayout();
    requestAcrylic(host->window());
}

Chrome::~Chrome() {
    delete impl_->filter;
    impl_->filter = nullptr;
    if (impl_->job != nullptr) {
        impl_->job->kill();
    }
    delete impl_;
}

void Chrome::loadPresets() {
    const QString path = findShareDir() + QStringLiteral("/presets/lucent.json");
    Json document;
    std::string error;
    const std::string bytes = readFile(std::filesystem::path(ss(path)));
    if (!parseJson(bytes, document, error) || !loadPresetDocument(document, impl_->presets, error)) {
        impl_->presetsOk = false;
        return;
    }
    impl_->presetsOk = true;
    const bool enlarge = !enlargeCliCommand().isEmpty();
    setOptionAvailable(impl_->presets, "view", "enhance", enlarge);
    const Json stored = readDocument(appSettingsPath(kAppId));
    impl_->state = mergePresets(impl_->presets, {}, stored.kind == Json::Kind::Object ? &stored : nullptr);
    if (impl_->state.values.count("viewer.colour") == 0) {
        impl_->state.values["viewer.colour"] = Json::fromString("srgb");
    }
    if (impl_->state.values.count("viewer.hdr") == 0) {
        impl_->state.values["viewer.hdr"] = Json::fromString("auto");
    }
    if (impl_->state.values.count("export.format") == 0) {
        impl_->state.values["export.format"] = Json::fromString("avif");
    }
    if (impl_->state.values.count("export.quality") == 0) {
        impl_->state.values["export.quality"] = Json::fromInt(60);
    }
    impl_->historyOn = jsonBool(stored, "ui.history_visible", true);
    impl_->expertWidth = std::clamp(jsonInt(stored, "ui.expert_width", kExpertDefault), kExpertMin, kExpertMax);
    impl_->expertOn = readExpertMode();
}

void Chrome::persistState() {
    if (!impl_->presetsOk) {
        return;
    }
    std::map<std::string, Json> own;
    own.emplace("values", objectFromValues(impl_->state.values));
    own.emplace("chosen", objectFromChosen(impl_->state.chosen));
    own.emplace("overrides", objectFromOverrides(impl_->state.overrides));
    std::string error;
    writeOwnKeys(appSettingsPath(kAppId), own, error);
}

void Chrome::persistFlag(const std::string &key, const Json &value) {
    std::map<std::string, Json> own;
    own.emplace(key, value);
    std::string error;
    writeOwnKeys(appSettingsPath(kAppId), own, error);
}

bool Chrome::panelOpen() const {
    if (impl_->shotOverride) {
        return impl_->shotExpert;
    }
    return impl_->expertOn;
}

void Chrome::buildBar() {
    impl_->bar = new QWidget(impl_->host);
    impl_->bar->setObjectName(QStringLiteral("topBar"));
    auto *row = new QHBoxLayout(impl_->bar);
    row->setContentsMargins(8, 0, 8, 0);
    row->setSpacing(8);
    const auto add = [this, row](const char *key, const std::function<void()> &fn) {
        auto *button = new QPushButton(label(QString::fromUtf8(key)), impl_->bar);
        QObject::connect(button, &QPushButton::clicked, impl_->host, fn);
        row->addWidget(button);
        return button;
    };
    add("simple.viewer.open", [this] { openFile(); });
    add("simple.viewer.save", [this] {
        if (impl_->cb.save) {
            impl_->cb.save();
        }
    });
    add("simple.viewer.previous", [this] {
        if (impl_->cb.step) {
            impl_->cb.step(-1);
        }
    });
    add("simple.viewer.next", [this] {
        if (impl_->cb.step) {
            impl_->cb.step(1);
        }
    });
    row->addStretch(1);
    auto *more = new QPushButton(label(QStringLiteral("lucent.action.more")), impl_->bar);
    QObject::connect(more, &QPushButton::clicked, impl_->host, [this] { openMenu(); });
    row->addWidget(more);
}

void Chrome::buildPresets() {
    impl_->presetRow = new QWidget(impl_->host);
    auto *outer = new QHBoxLayout(impl_->presetRow);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(8);
    auto *scroll = new QScrollArea(impl_->presetRow);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    auto *inner = new QWidget(scroll);
    impl_->chips = new QHBoxLayout(inner);
    impl_->chips->setContentsMargins(0, 0, 0, 0);
    impl_->chips->setSpacing(8);
    scroll->setWidget(inner);
    outer->addWidget(scroll, 1);
    impl_->customLabel = new QLabel(impl_->presetRow);
    impl_->customLabel->setObjectName(QStringLiteral("presetLabel"));
    impl_->resetButton = new QPushButton(impl_->presetRow);
    impl_->featureText = new QLabel(impl_->presetRow);
    impl_->featureText->setObjectName(QStringLiteral("featureText"));
    impl_->installButton = new QPushButton(impl_->presetRow);
    QObject::connect(impl_->resetButton, &QPushButton::clicked, impl_->host, [this] { resetView(); });
    QObject::connect(impl_->installButton, &QPushButton::clicked, impl_->host, [] {
        QDesktopServices::openUrl(QUrl(enlargeReleasesUrl()));
    });
    outer->addWidget(impl_->customLabel);
    outer->addWidget(impl_->resetButton);
    outer->addWidget(impl_->featureText);
    outer->addWidget(impl_->installButton);
}

void Chrome::buildExpert() {
    impl_->expert = new QFrame(impl_->host);
    impl_->expert->setObjectName(QStringLiteral("expertPanel"));
    auto *scroll = new QScrollArea(impl_->expert);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto *formHost = new QWidget(scroll);
    auto *form = new QFormLayout(formHost);
    form->setContentsMargins(12, 10, 12, 10);
    form->setSpacing(4);
    scroll->setWidget(formHost);
    auto *wrap = new QVBoxLayout(impl_->expert);
    wrap->setContentsMargins(0, 0, 0, 0);
    wrap->addWidget(scroll);

    impl_->zoomValue = new QLabel(formHost);
    impl_->zoomSpin = new QSpinBox(formHost);
    impl_->zoomSpin->setRange(5, 800);
    impl_->zoomSpin->setSuffix(QStringLiteral("%"));
    impl_->colour = new QComboBox(formHost);
    impl_->profile = new QLabel(formHost);
    impl_->hdr = new QComboBox(formHost);
    impl_->cropX = new QSpinBox(formHost);
    impl_->cropY = new QSpinBox(formHost);
    impl_->cropW = new QSpinBox(formHost);
    impl_->cropH = new QSpinBox(formHost);
    for (QSpinBox *box : {impl_->cropX, impl_->cropY, impl_->cropW, impl_->cropH}) {
        box->setRange(0, 100000);
    }
    impl_->rotate = new QComboBox(formHost);
    impl_->format = new QComboBox(formHost);
    impl_->quality = new QSpinBox(formHost);
    impl_->quality->setRange(1, 100);
    impl_->frames = new QLabel(formHost);
    impl_->info = new QLabel(formHost);
    impl_->info->setWordWrap(true);
    impl_->info->setObjectName(QStringLiteral("status"));
    impl_->pictures = new QComboBox(formHost);
    impl_->slideshow = new QSpinBox(formHost);
    impl_->slideshow->setRange(1, 60);
    impl_->slideshow->setValue(std::max(1, QSettings().value(QStringLiteral("slideshow/ms"), 3000).toInt() / 1000));
    impl_->thumbs = new QCheckBox(formHost);
    impl_->welcome = new QCheckBox(formHost);
    impl_->language = new QComboBox(formHost);

    form->addRow(label(QStringLiteral("lucent.expert.zoom")), impl_->zoomValue);
    form->addRow(QString(), impl_->zoomSpin);
    form->addRow(label(QStringLiteral("lucent.expert.colour")), impl_->colour);
    form->addRow(label(QStringLiteral("lucent.expert.icc")), impl_->profile);
    form->addRow(label(QStringLiteral("lucent.expert.hdr")), impl_->hdr);
    auto *cropRow = new QWidget(formHost);
    auto *cropLayout = new QHBoxLayout(cropRow);
    cropLayout->setContentsMargins(0, 0, 0, 0);
    cropLayout->addWidget(impl_->cropX);
    cropLayout->addWidget(impl_->cropY);
    cropLayout->addWidget(impl_->cropW);
    cropLayout->addWidget(impl_->cropH);
    form->addRow(label(QStringLiteral("lucent.expert.crop")), cropRow);
    form->addRow(label(QStringLiteral("lucent.expert.rotate")), impl_->rotate);
    form->addRow(label(QStringLiteral("lucent.expert.format")), impl_->format);
    form->addRow(label(QStringLiteral("lucent.expert.quality")), impl_->quality);
    form->addRow(label(QStringLiteral("lucent.expert.frames")), impl_->frames);
    auto *frameButtons = new QWidget(formHost);
    auto *frameRow = new QHBoxLayout(frameButtons);
    frameRow->setContentsMargins(0, 0, 0, 0);
    auto *previous = new QPushButton(label(QStringLiteral("lucent.tools.prev")), frameButtons);
    auto *play = new QPushButton(label(QStringLiteral("lucent.tools.play")), frameButtons);
    auto *next = new QPushButton(label(QStringLiteral("lucent.tools.next")), frameButtons);
    frameRow->addWidget(previous);
    frameRow->addWidget(play);
    frameRow->addWidget(next);
    form->addRow(QString(), frameButtons);
    form->addRow(label(QStringLiteral("lucent.expert.info")), impl_->info);
    form->addRow(label(QStringLiteral("lucent.expert.pictures")), impl_->pictures);

    const auto action = [this, form, formHost](const char *key, const std::function<void()> &fn) {
        auto *button = new QPushButton(label(QString::fromUtf8(key)), formHost);
        QObject::connect(button, &QPushButton::clicked, impl_->host, fn);
        form->addRow(button);
    };
    action("lucent.action.flip_h", [this] {
        if (impl_->cb.flip) {
            impl_->cb.flip(true);
        }
    });
    action("lucent.action.flip_v", [this] {
        if (impl_->cb.flip) {
            impl_->cb.flip(false);
        }
    });
    action("lucent.expert.open_enlarge", [this] {
        if (impl_->cb.enlarge) {
            impl_->cb.enlarge();
        }
    });
    action("lucent.action.copy_path", [this] { copyPath(); });
    action("lucent.action.copy_image", [this] { copyImage(); });
    action("lucent.tools.reveal", [this] { reveal(); });
    action("lucent.action.trash", [this] {
        if (impl_->cb.trash) {
            impl_->cb.trash();
        }
    });
    action("lucent.expert.hotkeys", [this] {
        if (impl_->cb.hotkeys) {
            impl_->cb.hotkeys();
        }
    });
    form->addRow(label(QStringLiteral("lucent.expert.slideshow")), impl_->slideshow);
    form->addRow(impl_->thumbs);
    form->addRow(impl_->welcome);
    form->addRow(label(QStringLiteral("lucent.expert.language")), impl_->language);

    QObject::connect(previous, &QPushButton::clicked, impl_->host, [this] {
        if (impl_->cb.stepFrame) {
            impl_->cb.stepFrame(-1);
        }
    });
    QObject::connect(next, &QPushButton::clicked, impl_->host, [this] {
        if (impl_->cb.stepFrame) {
            impl_->cb.stepFrame(1);
        }
    });
    QObject::connect(play, &QPushButton::clicked, impl_->host, [this] {
        if (impl_->cb.togglePlay) {
            impl_->cb.togglePlay();
        }
    });
    QObject::connect(impl_->zoomSpin, &QSpinBox::editingFinished, impl_->host, [this] { commitZoomSpin(); });
    QObject::connect(impl_->colour, &QComboBox::currentIndexChanged, impl_->host, [this](int) { commitCombo(impl_->colour, "viewer.colour"); });
    QObject::connect(impl_->hdr, &QComboBox::currentIndexChanged, impl_->host, [this](int) { commitCombo(impl_->hdr, "viewer.hdr"); });
    QObject::connect(impl_->format, &QComboBox::currentIndexChanged, impl_->host, [this](int) { commitCombo(impl_->format, "export.format"); });
    QObject::connect(impl_->pictures, &QComboBox::currentIndexChanged, impl_->host, [this](int) { commitCombo(impl_->pictures, "enlarge.preset"); });
    QObject::connect(impl_->quality, &QSpinBox::editingFinished, impl_->host, [this] {
        editValue(impl_->presets, impl_->state, "export.quality", Json::fromInt(impl_->quality->value()));
        persistState();
        refreshChips();
    });
    const auto cropDone = [this] {
        if (impl_->applying || !impl_->cb.numericCrop) {
            return;
        }
        impl_->cb.numericCrop(impl_->cropX->value(), impl_->cropY->value(), impl_->cropW->value(), impl_->cropH->value());
    };
    for (QSpinBox *box : {impl_->cropX, impl_->cropY, impl_->cropW, impl_->cropH}) {
        QObject::connect(box, &QSpinBox::editingFinished, impl_->host, cropDone);
    }
    QObject::connect(impl_->rotate, &QComboBox::currentIndexChanged, impl_->host, [this](int index) {
        if (impl_->applying || !impl_->cb.rotate) {
            return;
        }
        const int degrees = impl_->rotate->itemData(index).toInt();
        int steps = ((degrees - impl_->rotation) / 90) % 4;
        if (steps < 0) {
            steps += 4;
        }
        for (int i = 0; i < steps; ++i) {
            impl_->cb.rotate(true);
        }
        impl_->rotation = degrees;
    });
    QObject::connect(impl_->slideshow, &QSpinBox::valueChanged, impl_->host, [this](int seconds) {
        QSettings().setValue(QStringLiteral("slideshow/ms"), seconds * 1000);
        if (impl_->cb.slideshowMs) {
            impl_->cb.slideshowMs(seconds * 1000);
        }
    });
    QObject::connect(impl_->thumbs, &QCheckBox::toggled, impl_->host, [this](bool on) {
        if (impl_->applying) {
            return;
        }
        impl_->historyOn = on;
        persistFlag("ui.history_visible", Json::fromBool(on));
        if (impl_->cb.galleryVisible) {
            impl_->cb.galleryVisible(on);
        }
        relayout();
    });
    QObject::connect(impl_->welcome, &QCheckBox::toggled, impl_->host, [this](bool on) {
        if (impl_->applying) {
            return;
        }
        const bool was = QSettings().value(QStringLiteral("welcome/enabled"), true).toBool();
        QSettings().setValue(QStringLiteral("welcome/enabled"), on);
        if (on && !was) {
            QSettings().setValue(QStringLiteral("welcome/emptyCount"), welcomeCountAfterReenable());
        }
    });
    QObject::connect(impl_->language, &QComboBox::currentIndexChanged, impl_->host, [this](int index) {
        if (impl_->applying) {
            return;
        }
        const QString code = impl_->language->itemData(index).toString();
        std::map<std::string, Json> own;
        own.emplace("ui_language", Json::fromString(ss(code)));
        std::string error;
        writeOwnKeys(suiteSettingsPath(), own, error);
        setCatalogLanguage(code);
        setLanguage(code);
        if (impl_->cb.language) {
            impl_->cb.language(code);
        }
        retranslate();
    });
}

void Chrome::buildStatus() {
    impl_->statusRow = new QWidget(impl_->host);
    auto *row = new QHBoxLayout(impl_->statusRow);
    row->setContentsMargins(0, 4, 0, 4);
    row->setSpacing(8);
    impl_->progressText = new QLabel(impl_->statusRow);
    impl_->progressText->setObjectName(QStringLiteral("progressText"));
    impl_->progress = new QProgressBar(impl_->statusRow);
    impl_->progress->setObjectName(QStringLiteral("progress"));
    impl_->progress->setTextVisible(false);
    auto *cancel = new QPushButton(label(QStringLiteral("mdl.cancel")), impl_->statusRow);
    QObject::connect(cancel, &QPushButton::clicked, impl_->host, [this] { cancelJob(); });
    row->addWidget(impl_->progressText, 1);
    row->addWidget(impl_->progress, 1);
    row->addWidget(cancel);
}

void Chrome::buildCompare() {
    impl_->compare = new QWidget(impl_->host);
    impl_->compare->setObjectName(QStringLiteral("stage"));
    auto *row = new QHBoxLayout(impl_->compare);
    row->setContentsMargins(8, 8, 8, 8);
    const auto add = [this, row](const char *key, const std::function<void()> &fn) {
        auto *button = new QPushButton(label(QString::fromUtf8(key)), impl_->compare);
        QObject::connect(button, &QPushButton::clicked, impl_->host, fn);
        row->addWidget(button);
    };
    add("lucent.action.keep", [this] { impl_->compare->hide(); });
    add("lucent.action.reveal", [this] {
        if (!impl_->jobOutput.isEmpty()) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(impl_->jobOutput).absolutePath()));
        }
    });
    add("lucent.action.discard", [this] {
        if (!impl_->jobOutput.isEmpty()) {
            QFile::remove(impl_->jobOutput);
        }
        impl_->compare->hide();
    });
    impl_->compare->hide();
}

void Chrome::refreshChips() {
    if (impl_->chips == nullptr) {
        return;
    }
    while (QLayoutItem *item = impl_->chips->takeAt(0)) {
        if (item->widget() != nullptr) {
            item->widget()->deleteLater();
        }
        delete item;
    }
    auto *groupLabel = new QLabel(label(QStringLiteral("simple.preset.view")), impl_->presetRow);
    groupLabel->setObjectName(QStringLiteral("presetLabel"));
    groupLabel->setMinimumWidth(96);
    impl_->chips->addWidget(groupLabel);
    const PresetGroup *group = findGroup(impl_->presets, "view");
    const ChipStatus status = impl_->presetsOk ? chipFor(impl_->presets, impl_->state, "view") : ChipStatus{};
    if (group != nullptr) {
        for (const PresetOption &option : group->options) {
            const QString name = label(qs(option.labelKey));
            QString caption = name;
            const bool stored = status.optionId == option.id && status.kind != ChipKind::Custom;
            if (stored && status.kind == ChipKind::Modified) {
                caption = qs(formatChip(ChipKind::Modified, ss(name), ss(label(QStringLiteral("simple.preset.changed"))),
                                         ss(label(QStringLiteral("simple.preset.custom")))));
            }
            auto *chip = new QPushButton(caption, impl_->presetRow);
            chip->setProperty("chip", true);
            chip->setProperty("available", option.available ? QStringLiteral("true") : QStringLiteral("false"));
            chip->setCheckable(option.available);
            chip->setChecked(stored && status.kind == ChipKind::Exact);
            if (stored && status.kind == ChipKind::Modified) {
                chip->setChecked(true);
            }
            chip->setMinimumWidth(156);
            const QString id = qs(option.id);
            QObject::connect(chip, &QPushButton::clicked, impl_->host, [this, id] { chooseChip(id); });
            impl_->chips->addWidget(chip);
        }
    }
    impl_->chips->addStretch(1);
    const bool custom = status.kind == ChipKind::Custom;
    const bool changed = status.kind != ChipKind::Exact;
    impl_->customLabel->setVisible(custom);
    impl_->customLabel->setText(label(QStringLiteral("simple.preset.custom")));
    const QString storedName = label(qs(status.labelKey));
    impl_->resetButton->setVisible(changed && !status.optionId.empty());
    impl_->resetButton->setText(labelWithName(label(QStringLiteral("simple.preset.reset")), storedName));
    const PresetOption *enhance = group == nullptr ? nullptr : findOption(*group, "enhance");
    const bool missing = enhance != nullptr && !enhance->available;
    impl_->featureText->setVisible(missing);
    impl_->installButton->setVisible(missing);
    if (enhance != nullptr) {
        impl_->featureText->setText(label(qs(enhance->reasonKey)));
        impl_->installButton->setText(label(qs(enhance->actionLabelKey)));
    }
}

void Chrome::refreshExpert() {
    impl_->applying = true;
    const auto select = [](QComboBox *box, const QString &id) {
        const int index = box->findData(id);
        if (index >= 0) {
            box->setCurrentIndex(index);
        }
    };
    impl_->colour->clear();
    impl_->colour->addItem(label(QStringLiteral("lucent.colour.srgb")), QStringLiteral("srgb"));
    impl_->colour->addItem(label(QStringLiteral("lucent.colour.embed")), QStringLiteral("embed"));
    impl_->colour->addItem(label(QStringLiteral("lucent.colour.off")), QStringLiteral("off"));
    impl_->hdr->clear();
    impl_->hdr->addItem(label(QStringLiteral("lucent.hdr.auto")), QStringLiteral("auto"));
    impl_->hdr->addItem(label(QStringLiteral("lucent.hdr.sdr")), QStringLiteral("sdr"));
    impl_->hdr->addItem(label(QStringLiteral("lucent.hdr.off")), QStringLiteral("off"));
    impl_->format->clear();
    impl_->format->addItem(label(QStringLiteral("lucent.format.avif")), QStringLiteral("avif"));
    impl_->format->addItem(label(QStringLiteral("lucent.format.jxl")), QStringLiteral("jxl"));
    impl_->format->addItem(label(QStringLiteral("lucent.format.webp")), QStringLiteral("webp"));
    impl_->pictures->clear();
    impl_->pictures->addItem(label(QStringLiteral("lucent.expert.photo")), QStringLiteral("photo"));
    impl_->pictures->addItem(label(QStringLiteral("lucent.expert.art")), QStringLiteral("art"));
    impl_->rotate->clear();
    for (int degrees : {0, 90, 180, 270}) {
        impl_->rotate->addItem(QString::number(degrees), degrees);
    }
    impl_->language->clear();
    impl_->language->addItem(QStringLiteral("English"), QStringLiteral("en"));
    impl_->language->addItem(QStringLiteral("Русский"), QStringLiteral("ru"));
    impl_->language->addItem(QStringLiteral("Deutsch"), QStringLiteral("de"));
    impl_->thumbs->setText(label(QStringLiteral("lucent.expert.thumbnails")));
    impl_->welcome->setText(label(QStringLiteral("lucent.expert.welcome")));
    select(impl_->colour, jsonText(objectFromValues(impl_->state.values), "viewer.colour", QStringLiteral("srgb")));
    select(impl_->hdr, jsonText(objectFromValues(impl_->state.values), "viewer.hdr", QStringLiteral("auto")));
    select(impl_->format, jsonText(objectFromValues(impl_->state.values), "export.format", QStringLiteral("avif")));
    const auto found = impl_->state.values.find("enlarge.preset");
    QString pictures;
    if (found != impl_->state.values.end() && found->second.kind == Json::Kind::String) {
        pictures = qs(found->second.text);
    }
    if (pictures == QStringLiteral("art") || pictures == QStringLiteral("photo")) {
        select(impl_->pictures, pictures);
    } else {
        impl_->pictures->setCurrentIndex(-1);
    }
    const auto quality = impl_->state.values.find("export.quality");
    if (quality != impl_->state.values.end() && quality->second.isNumber()) {
        impl_->quality->setValue(static_cast<int>(std::lround(quality->second.asNumber())));
    }
    select(impl_->language, catalogLanguage());
    const int rotationIndex = impl_->rotate->findData(impl_->rotation);
    if (rotationIndex >= 0) {
        impl_->rotate->setCurrentIndex(rotationIndex);
    }
    impl_->thumbs->setChecked(impl_->historyOn);
    impl_->welcome->setChecked(QSettings().value(QStringLiteral("welcome/enabled"), true).toBool());
    impl_->applying = false;
    setLive(impl_->live);
}

void Chrome::chooseChip(const QString &id) {
    if (!chooseOption(impl_->presets, impl_->state, "view", ss(id))) {
        refreshChips();
        return;
    }
    persistState();
    refreshChips();
    applyZoom();
    if (id == QStringLiteral("enhance")) {
        startJob();
    }
}

void Chrome::resetView() {
    if (!resetGroup(impl_->presets, impl_->state, "view")) {
        return;
    }
    persistState();
    refreshChips();
    refreshExpert();
    applyZoom();
}

void Chrome::applyZoom() {
    const auto found = impl_->state.values.find("viewer.zoom");
    if (found == impl_->state.values.end() || !impl_->cb.zoom) {
        return;
    }
    const Json &zoom = found->second;
    if (zoom.kind == Json::Kind::String && zoom.text == "fit") {
        impl_->cb.zoom(ZoomMode::Fit);
        return;
    }
    if (zoom.kind == Json::Kind::String && zoom.text == "100") {
        impl_->cb.zoom(ZoomMode::OneToOne);
        return;
    }
    if (impl_->cb.setScale) {
        double percent = 100.0;
        if (zoom.isNumber()) {
            percent = zoom.asNumber();
        } else if (zoom.kind == Json::Kind::String) {
            percent = qs(zoom.text).toDouble();
        }
        impl_->cb.setScale(std::max(0.05, percent / 100.0));
    }
}

void Chrome::commitZoomSpin() {
    if (impl_->applying) {
        return;
    }
    const int percent = impl_->zoomSpin->value();
    editValue(impl_->presets, impl_->state, "viewer.zoom", Json::fromString(std::to_string(percent)));
    persistState();
    refreshChips();
    if (impl_->cb.setScale) {
        impl_->cb.setScale(percent / 100.0);
    }
}

void Chrome::commitCombo(QComboBox *box, const char *key) {
    if (impl_->applying || box == nullptr) {
        return;
    }
    editValue(impl_->presets, impl_->state, key, Json::fromString(ss(box->currentData().toString())));
    persistState();
    refreshChips();
}

void Chrome::noteZoom(ZoomMode mode, double scale) {
    Json value = Json::fromString("fit");
    if (mode == ZoomMode::OneToOne) {
        value = Json::fromString("100");
    } else if (mode == ZoomMode::Free) {
        value = Json::fromString(std::to_string(std::max(1, static_cast<int>(std::lround(scale * 100.0)))));
    } else if (mode != ZoomMode::Fit) {
        value = Json::fromString(std::to_string(std::max(1, static_cast<int>(std::lround(scale * 100.0)))));
    }
    editValue(impl_->presets, impl_->state, "viewer.zoom", value);
    if (impl_->stateTimer != nullptr) {
        impl_->stateTimer->start();
    }
    setLive(impl_->live);
}

void Chrome::setLive(const LiveView &live) {
    impl_->live = live;
    if (impl_->zoomValue == nullptr) {
        return;
    }
    impl_->applying = true;
    if (live.mode == ZoomMode::Fit) {
        impl_->zoomValue->setText(label(QStringLiteral("simple.viewer.fit")));
    } else {
        impl_->zoomValue->setText(QString::number(static_cast<int>(std::lround(live.scale * 100.0))) + QStringLiteral("%"));
    }
    impl_->zoomSpin->setValue(std::clamp(static_cast<int>(std::lround(live.scale * 100.0)), 5, 800));
    impl_->cropX->setValue(live.cropX);
    impl_->cropY->setValue(live.cropY);
    impl_->cropW->setValue(live.cropW);
    impl_->cropH->setValue(live.cropH);
    impl_->frames->setText(QString::number(live.frame + 1) + QStringLiteral(" / ") + QString::number(std::max(1, live.frameCount)));
    impl_->info->setText(live.info);
    impl_->profile->setText(live.profile.isEmpty() ? label(QStringLiteral("lucent.colour.srgb")) : live.profile);
    impl_->applying = false;
}

void Chrome::resizeExpert(int delta) {
    if (!panelOpen()) {
        return;
    }
    impl_->expertWidth = std::clamp(impl_->expertWidth + delta, kExpertMin, kExpertMax);
    persistFlag("ui.expert_width", Json::fromInt(impl_->expertWidth));
    relayout();
    if (impl_->cb.galleryVisible) {
        impl_->cb.galleryVisible(impl_->historyOn);
    }
}

void Chrome::relayout() {
    if (impl_->host == nullptr || impl_->bar == nullptr) {
        return;
    }
    const int width = impl_->host->width();
    const int height = impl_->host->height();
    const bool history = impl_->historyOn && height > kViewerMinH;
    const bool running = impl_->job != nullptr && impl_->job->state() != QProcess::NotRunning;
    const int statusH = running ? kStatus : 0;
    const int historyH = history ? kHistory : 0;
    const int expertW = panelOpen() ? impl_->expertWidth : 0;
    int y = kPageTop;
    impl_->bar->setFixedHeight(kTop);
    impl_->bar->setGeometry(kPageLeft, y, std::max(0, width - kPageLeft - kPageRight), kTop);
    impl_->bar->show();
    impl_->presetRow->show();
    if (impl_->bar->layout() != nullptr) {
        impl_->bar->layout()->activate();
    }
    y += kTop + kBarGap;
    const int presetH = 44;
    impl_->presetRow->setGeometry(kPageLeft, y, std::max(0, width - kPageLeft - kPageRight), presetH);
    y += presetH + kRowGap;
    const int bottom = height - kPageBottom - historyH - statusH;
    const int resultW = std::max(0, width - kPageLeft - kPageRight - expertW - (expertW > 0 ? kExpertGap : 0));
    impl_->result = QRect(kPageLeft, y, resultW, std::max(0, bottom - y));
    impl_->expert->setFixedWidth(expertW);
    impl_->expert->setGeometry(impl_->result.right() + (expertW > 0 ? kExpertGap : 0), impl_->result.top(), expertW, impl_->result.height());
    impl_->expert->setVisible(true);
    impl_->split->setGeometry(impl_->expert->x() - 3, impl_->result.top(), 6, impl_->result.height());
    impl_->split->setVisible(expertW > 0);
    impl_->statusRow->setGeometry(kPageLeft, impl_->result.bottom(), std::max(0, width - kPageLeft - kPageRight), statusH);
    impl_->statusRow->setVisible(statusH > 0);
    impl_->gallery->setGeometry(kPageLeft, impl_->statusRow->geometry().bottom(), std::max(0, width - kPageLeft - kPageRight), historyH);
    impl_->gallery->setVisible(historyH > 0);
    impl_->compare->setGeometry(impl_->result.left(), impl_->result.bottom() - 56, std::min(360, impl_->result.width()), 48);
    const auto raiseNative = [](QWidget *widget) {
        if (widget == nullptr) {
            return;
        }
        widget->winId();
        widget->raise();
    };
    raiseNative(impl_->compare);
    raiseNative(impl_->bar);
    raiseNative(impl_->presetRow);
    raiseNative(impl_->expert);
    raiseNative(impl_->split);
    raiseNative(impl_->statusRow);
    raiseNative(impl_->gallery);
}

QRect Chrome::resultRect() const { return impl_->result; }

int Chrome::galleryHeight() const {
    if (impl_->gallery == nullptr || !impl_->gallery->isVisible()) {
        return 0;
    }
    return kHistory;
}

void Chrome::setFolder(const QStringList &files, int index) {
    impl_->rotation = 0;
    if (impl_->gallery != nullptr) {
        impl_->gallery->setFiles(files, index);
    }
    refreshExpert();
}

void Chrome::setCurrent(int index) {
    if (impl_->gallery != nullptr) {
        impl_->gallery->setCurrent(index);
    }
}

void Chrome::mouseAt(const QPoint &) {}

void Chrome::retranslate() {
    delete impl_->bar;
    impl_->bar = nullptr;
    buildBar();
    refreshChips();
    refreshExpert();
    relayout();
}

void Chrome::saveGeometry() {
    if (!impl_->geometryReady || !qgetenv("LUCENT_SCREENSHOT").isEmpty()) {
        return;
    }
    QWidget *window = impl_->host->window();
    Json geo;
    geo.kind = Json::Kind::Object;
    const Json existing = readDocument(appSettingsPath(kAppId));
    const Json *prior = existing.field("ui.geometry");
    if (prior != nullptr && prior->kind == Json::Kind::Object) {
        geo = *prior;
    }
    if (!window->isFullScreen() && !window->isMaximized()) {
        const QRect rect = window->geometry();
        geo.set("x", Json::fromInt(rect.x()));
        geo.set("y", Json::fromInt(rect.y()));
        geo.set("width", Json::fromInt(rect.width()));
        geo.set("height", Json::fromInt(rect.height()));
    }
    geo.set("maximized", Json::fromBool(window->isMaximized()));
    geo.set("fullscreen", Json::fromBool(window->isFullScreen()));
    persistFlag("ui.geometry", geo);
}

QString Chrome::exportSuffix() const {
    const auto found = impl_->state.values.find("export.format");
    if (found != impl_->state.values.end() && found->second.kind == Json::Kind::String) {
        const QString suffix = qs(found->second.text);
        if (suffix == QStringLiteral("jxl") || suffix == QStringLiteral("webp") || suffix == QStringLiteral("avif")) {
            return suffix;
        }
    }
    return QStringLiteral("avif");
}

int Chrome::exportQuality() const {
    const auto found = impl_->state.values.find("export.quality");
    if (found != impl_->state.values.end() && found->second.isNumber()) {
        return std::clamp(static_cast<int>(std::lround(found->second.asNumber())), 1, 100);
    }
    return 60;
}

void Chrome::startJob() {
    if (impl_->job != nullptr && impl_->job->state() != QProcess::NotRunning) {
        return;
    }
    QStringList command = enlargeCliCommand();
    if (command.isEmpty() || !impl_->cb.path) {
        return;
    }
    const QString input = impl_->cb.path();
    if (input.isEmpty()) {
        return;
    }
    const QFileInfo info(input);
    impl_->jobOutput = info.absolutePath() + QLatin1Char('/') + info.completeBaseName() + QStringLiteral(".enhanced.") + exportSuffix();
    const QString program = command.takeFirst();
    command.append(QStringLiteral("--input"));
    command.append(input);
    command.append(QStringLiteral("--output"));
    command.append(impl_->jobOutput);
    command.append(QStringLiteral("--scale"));
    command.append(QStringLiteral("2"));
    const auto preset = impl_->state.values.find("enlarge.preset");
    const QString presetId = preset != impl_->state.values.end() && preset->second.kind == Json::Kind::String && !preset->second.text.empty()
        ? qs(preset->second.text)
        : QStringLiteral("photo");
    command.append(QStringLiteral("--preset"));
    command.append(presetId == QStringLiteral("art") ? presetId : QStringLiteral("photo"));
    command.append(QStringLiteral("--quality-tier"));
    command.append(QStringLiteral("fast"));
    command.append(QStringLiteral("--format"));
    command.append(exportSuffix());
    command.append(QStringLiteral("--quality"));
    command.append(QString::number(exportQuality()));
    command.append(QStringLiteral("--json-progress"));
    if (impl_->job == nullptr) {
        impl_->job = new QProcess(impl_->host);
        QObject::connect(impl_->job, &QProcess::readyReadStandardOutput, impl_->host, [this] { readJob(); });
        QObject::connect(impl_->job, &QProcess::finished, impl_->host, [this](int code, QProcess::ExitStatus) { finishJob(code); });
    }
    impl_->jobCancel = false;
    impl_->jobBuffer.clear();
    impl_->progress->setRange(0, 0);
    impl_->progressText->setText(stageSentence(QStringLiteral("queued")));
    impl_->compare->hide();
    impl_->job->start(program, command);
    relayout();
}

void Chrome::readJob() {
    impl_->jobBuffer += impl_->job->readAllStandardOutput();
    while (true) {
        const int newline = impl_->jobBuffer.indexOf('\n');
        if (newline < 0) {
            break;
        }
        const QByteArray line = impl_->jobBuffer.left(newline);
        impl_->jobBuffer = impl_->jobBuffer.mid(newline + 1);
        const QJsonObject object = QJsonDocument::fromJson(line).object();
        if (object.value(QStringLiteral("type")).toString() != QStringLiteral("progress")) {
            continue;
        }
        const bool hasPct = object.contains(QStringLiteral("pct"));
        const int step = hasPct ? object.value(QStringLiteral("pct")).toInt() : 0;
        const int total = hasPct ? 100 : 0;
        if (total > 0) {
            impl_->progress->setRange(0, total);
            impl_->progress->setValue(std::clamp(step, 0, total));
        } else {
            impl_->progress->setRange(0, 0);
        }
        QString text = stageSentence(object.value(QStringLiteral("stage")).toString());
        if (total > 0) {
            text = QString::number(step) + QStringLiteral(" / ") + QString::number(total) + QStringLiteral("  ") + text;
        }
        const double eta = object.value(QStringLiteral("eta_s")).toDouble();
        const QString left = object.contains(QStringLiteral("eta_s")) ? etaText(eta) : QString();
        if (!left.isEmpty()) {
            text += QStringLiteral("  ") + left;
        }
        impl_->progressText->setText(text);
    }
}

void Chrome::finishJob(int code) {
    const bool cancelled = impl_->jobCancel;
    impl_->jobCancel = false;
    if (cancelled || code != 0 || !QFileInfo::exists(impl_->jobOutput)) {
        if (cancelled || code != 0) {
            QFile::remove(impl_->jobOutput);
        }
        impl_->compare->hide();
    } else {
        impl_->compare->show();
        impl_->compare->raise();
    }
    relayout();
}

void Chrome::cancelJob() {
    impl_->jobCancel = true;
    if (impl_->job != nullptr) {
        impl_->job->kill();
    }
}

void Chrome::openFile() {
    const QString path = QFileDialog::getOpenFileName(impl_->host, label(QStringLiteral("simple.viewer.open")));
    if (!path.isEmpty() && impl_->cb.openPath) {
        impl_->cb.openPath(path);
    }
}

void Chrome::setExpert(bool enabled) {
    if (qgetenv("LUCENT_SCREENSHOT").isEmpty()) {
        std::string error;
        writeExpertMode(enabled, error);
    }
    impl_->expertOn = enabled;
    impl_->shotOverride = !qgetenv("LUCENT_SCREENSHOT").isEmpty();
    impl_->shotExpert = enabled;
    relayout();
    if (impl_->cb.galleryVisible) {
        impl_->cb.galleryVisible(impl_->historyOn);
    }
}

void Chrome::addKeptActions(QMenu &menu) {
    const auto call = [](const std::function<void()> &fn) {
        if (fn) {
            fn();
        }
    };
    menu.addAction(label(QStringLiteral("lucent.action.save_as")), [this, call] { call(impl_->cb.saveAs); });
    menu.addAction(label(QStringLiteral("lucent.action.save_copy")), [this, call] { call(impl_->cb.saveCopy); });
    menu.addAction(label(QStringLiteral("lucent.action.export")), [this, call] { call(impl_->cb.exportImage); });
    menu.addAction(label(QStringLiteral("lucent.action.download")), [this, call] { call(impl_->cb.download); });
    QMenu *zoom = menu.addMenu(label(QStringLiteral("lucent.expert.zoom")));
    zoom->addAction(label(QStringLiteral("simple.viewer.fit")), [this] {
        if (impl_->cb.zoom) {
            impl_->cb.zoom(ZoomMode::Fit);
        }
    });
    zoom->addAction(label(QStringLiteral("simple.viewer.full")), [this] {
        if (impl_->cb.zoom) {
            impl_->cb.zoom(ZoomMode::OneToOne);
        }
    });
    zoom->addAction(label(QStringLiteral("lucent.action.fill")), [this] {
        if (impl_->cb.zoom) {
            impl_->cb.zoom(ZoomMode::Fill);
        }
    });
    zoom->addAction(label(QStringLiteral("lucent.action.fit_width")), [this] {
        if (impl_->cb.zoom) {
            impl_->cb.zoom(ZoomMode::FitWidth);
        }
    });
    zoom->addAction(label(QStringLiteral("lucent.action.fit_height")), [this] {
        if (impl_->cb.zoom) {
            impl_->cb.zoom(ZoomMode::FitHeight);
        }
    });
    menu.addAction(label(QStringLiteral("lucent.action.rotate_left")), [this] {
        if (impl_->cb.rotate) {
            impl_->cb.rotate(false);
        }
    });
    menu.addAction(label(QStringLiteral("lucent.action.rotate_right")), [this] {
        if (impl_->cb.rotate) {
            impl_->cb.rotate(true);
        }
    });
    menu.addAction(label(QStringLiteral("lucent.action.flip_h")), [this] {
        if (impl_->cb.flip) {
            impl_->cb.flip(true);
        }
    });
    menu.addAction(label(QStringLiteral("lucent.action.flip_v")), [this] {
        if (impl_->cb.flip) {
            impl_->cb.flip(false);
        }
    });
    menu.addAction(label(QStringLiteral("lucent.action.crop")), [this, call] { call(impl_->cb.crop); });
    menu.addAction(label(QStringLiteral("lucent.action.trash")), [this, call] { call(impl_->cb.trash); });
    menu.addAction(label(QStringLiteral("lucent.expert.thumbnails")), [this] { toggleGallery(); });
    menu.addAction(label(QStringLiteral("lucent.action.frameless")), [this, call] { call(impl_->cb.frameless); });
    menu.addAction(label(QStringLiteral("lucent.action.window_fit")), [this, call] { call(impl_->cb.windowFit); });
    menu.addAction(label(QStringLiteral("lucent.action.tools")), [this, call] { call(impl_->cb.tools); });
}

void Chrome::openMenu() {
    QMenu menu(impl_->host);
    QAction *expert = menu.addAction(label(QStringLiteral("command.view.toggle_expert_mode")));
    expert->setCheckable(true);
    expert->setChecked(panelOpen());
    QObject::connect(expert, &QAction::toggled, impl_->host, [this](bool on) { setExpert(on); });
    menu.addAction(label(QStringLiteral("lucent.action.fullscreen")), [this] {
        if (impl_->cb.fullscreen) {
            impl_->cb.fullscreen();
        }
    });
    menu.addAction(label(QStringLiteral("lucent.action.slideshow")), [this] {
        if (impl_->cb.slideshow) {
            impl_->cb.slideshow();
        }
    });
    addKeptActions(menu);
    menu.addAction(label(QStringLiteral("lucent.action.about")), [this] { openAbout(); });
    menu.addAction(label(QStringLiteral("lucent.action.updates")), [this] {
        if (impl_->cb.checkUpdate) {
            impl_->cb.checkUpdate();
        }
    });
    if (qgetenv("LUCENT_SCREENSHOT") == "menu") {
        menu.popup(impl_->bar->mapToGlobal(QPoint(impl_->bar->width() - 120, impl_->bar->height())));
        QTimer::singleShot(150, &menu, [&menu] { saveGrab(&menu, QStringLiteral("menu")); });
        QEventLoop loop;
        QTimer::singleShot(800, &loop, &QEventLoop::quit);
        loop.exec();
        return;
    }
    menu.exec(impl_->bar->mapToGlobal(QPoint(impl_->bar->width() - 120, impl_->bar->height())));
}

void Chrome::contextMenu(const QPoint &globalPos) {
    QMenu menu(impl_->host);
    menu.addAction(label(QStringLiteral("simple.viewer.open")), [this] { openFile(); });
    menu.addAction(label(QStringLiteral("simple.viewer.previous")), [this] {
        if (impl_->cb.step) {
            impl_->cb.step(-1);
        }
    });
    menu.addAction(label(QStringLiteral("simple.viewer.next")), [this] {
        if (impl_->cb.step) {
            impl_->cb.step(1);
        }
    });
    menu.addSeparator();
    menu.addAction(label(QStringLiteral("simple.viewer.save")), [this] {
        if (impl_->cb.save) {
            impl_->cb.save();
        }
    });
    addKeptActions(menu);
    menu.addAction(label(QStringLiteral("lucent.action.copy_image")), [this] { copyImage(); });
    menu.addAction(label(QStringLiteral("lucent.action.copy_path")), [this] { copyPath(); });
    menu.addAction(label(QStringLiteral("lucent.tools.reveal")), [this] { reveal(); });
    menu.addAction(label(QStringLiteral("lucent.action.fullscreen")), [this] {
        if (impl_->cb.fullscreen) {
            impl_->cb.fullscreen();
        }
    });
    menu.addAction(label(QStringLiteral("lucent.action.slideshow")), [this] {
        if (impl_->cb.slideshow) {
            impl_->cb.slideshow();
        }
    });
    if (qgetenv("LUCENT_SCREENSHOT") == "context") {
        menu.popup(globalPos);
        QTimer::singleShot(150, &menu, [&menu] { saveGrab(&menu, QStringLiteral("context")); });
        QEventLoop loop;
        QTimer::singleShot(800, &loop, &QEventLoop::quit);
        loop.exec();
        return;
    }
    menu.exec(globalPos);
}

void Chrome::openAbout() {
    QMessageBox box(impl_->host);
    box.setWindowTitle(label(QStringLiteral("lucent.action.about")));
    box.setText(QStringLiteral("VOXPrint AI Lucent"));
    box.setInformativeText(QString::fromUtf8(LUCENT_BUILD_LABEL) + QStringLiteral("\nApache License 2.0"));
    if (qgetenv("LUCENT_SCREENSHOT") == "about") {
        box.show();
        box.raise();
        QTimer::singleShot(200, &box, [&box] { saveGrab(&box, QStringLiteral("about")); });
        QEventLoop loop;
        QTimer::singleShot(800, &loop, &QEventLoop::quit);
        loop.exec();
        return;
    }
    box.exec();
}

void Chrome::openSettings() {
    setExpert(true);
    if (qgetenv("LUCENT_SCREENSHOT") == "settings") {
        QTimer::singleShot(200, impl_->host, [this] { saveGrab(impl_->host, QStringLiteral("settings")); });
    }
}

void Chrome::showSettings() { openSettings(); }

void Chrome::setGalleryVisible(bool visible) {
    impl_->historyOn = visible;
    persistFlag("ui.history_visible", Json::fromBool(visible));
    if (impl_->thumbs != nullptr) {
        impl_->applying = true;
        impl_->thumbs->setChecked(visible);
        impl_->applying = false;
    }
    relayout();
}

void Chrome::toggleGallery() { setGalleryVisible(!impl_->historyOn); }

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
    QProcess::startDetached(QStringLiteral("explorer.exe"), {QStringLiteral("/select,"), QDir::toNativeSeparators(path)});
#else
    QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
#endif
}

void Chrome::prepareShots() {
    const QByteArray mode = qgetenv("LUCENT_SCREENSHOT");
    if (mode.isEmpty() || mode == "splash") {
        return;
    }
    relayout();
    if (mode == "toolbar") {
        QTimer::singleShot(200, impl_->host, [this] { saveGrab(impl_->bar, QStringLiteral("toolbar")); });
    } else if (mode == "about") {
        QTimer::singleShot(200, impl_->host, [this] { openAbout(); });
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
    } else if (mode == "simple") {
        impl_->shotOverride = true;
        impl_->shotExpert = false;
        relayout();
        QTimer::singleShot(400, impl_->host, [this] { saveGrab(impl_->host, QStringLiteral("simple")); });
    } else if (mode == "expert") {
        impl_->shotOverride = true;
        impl_->shotExpert = true;
        relayout();
        QTimer::singleShot(400, impl_->host, [this] { saveGrab(impl_->host, QStringLiteral("expert")); });
    }
}

}  // namespace lucent
