// Copyright 2026 Aleksandr Mitroshenkov
// SPDX-License-Identifier: Apache-2.0

#include "chrome.h"

#include "dry_run.h"
#include "gallery.h"
#include "hotkeys.h"
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
#include <QDialogButtonBox>
#include <QDir>
#include <QEvent>
#include <QEventLoop>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
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
#include <QPaintEvent>
#include <QProcess>
#include <QResizeEvent>
#include <QProgressBar>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QSpinBox>
#include <QStyle>
#include <QStyleOptionButton>
#include <QStyleOptionComboBox>
#include <QStylePainter>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <vector>

namespace lucent {
namespace {

constexpr int kTop = 40;
constexpr int kChip = 44;
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

void saveScreen(QWidget *window, QWidget *popup, const QString &name) {
    if (window == nullptr) {
        return;
    }
    QScreen *screen = window->screen();
    if (screen == nullptr) {
        screen = QGuiApplication::primaryScreen();
    }
    if (screen == nullptr) {
        return;
    }
    const QPixmap shot = screen->grabWindow(0);
    QRect bounds = window->frameGeometry();
    if (popup != nullptr && popup->isVisible()) {
        bounds = bounds.united(popup->frameGeometry());
    }
    const QRect local = bounds.translated(-screen->geometry().topLeft());
    shot.copy(local.intersected(QRect(QPoint(0, 0), shot.size())))
        .save(shotDir() + QLatin1Char('/') + name + QStringLiteral(".png"));
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

constexpr int kExpertStackWidth = 300;

class WrapLabel : public QLabel {
public:
    explicit WrapLabel(const QString &text, QWidget *parent = nullptr) : QLabel(text, parent) {
        setWordWrap(true);
        setMinimumWidth(0);
        setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    }

    QSize minimumSizeHint() const override { return QSize(longestWord(), fontMetrics().height()); }

private:
    int longestWord() const {
        int width = fontMetrics().horizontalAdvance(QStringLiteral("M"));
        const QStringList words = text().split(QLatin1Char(' '), Qt::SkipEmptyParts);
        for (const QString &word : words) {
            width = std::max(width, fontMetrics().horizontalAdvance(word));
        }
        return width;
    }
};

class FieldCombo : public QComboBox {
public:
    explicit FieldCombo(QWidget *parent = nullptr) : QComboBox(parent) {
        setMinimumContentsLength(1);
        setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        setMinimumWidth(0);
        setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    }

    QSize minimumSizeHint() const override {
        QSize hint = QComboBox::minimumSizeHint();
        hint.setWidth(36);
        return hint;
    }

protected:
    void paintEvent(QPaintEvent *) override {
        QStylePainter painter(this);
        QStyleOptionComboBox option;
        initStyleOption(&option);
        const QRect field = style()->subControlRect(QStyle::CC_ComboBox, &option, QStyle::SC_ComboBoxEditField, this);
        option.currentText = fontMetrics().elidedText(option.currentText, Qt::ElideRight, std::max(0, field.width()));
        painter.drawComplexControl(QStyle::CC_ComboBox, option);
        painter.drawControl(QStyle::CE_ComboBoxLabel, option);
    }
};

class ShrinkSpin : public QSpinBox {
public:
    explicit ShrinkSpin(QWidget *parent = nullptr) : QSpinBox(parent) {
        setMinimumWidth(0);
        setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    }

    QSize minimumSizeHint() const override {
        QSize hint = QSpinBox::minimumSizeHint();
        hint.setWidth(44);
        return hint;
    }
};

class ShrinkButton : public QPushButton {
public:
    explicit ShrinkButton(const QString &text, QWidget *parent = nullptr) : QPushButton(text, parent) {
        setMinimumWidth(0);
        setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    }

    QSize minimumSizeHint() const override {
        QSize hint = QPushButton::minimumSizeHint();
        hint.setWidth(1);
        return hint;
    }

protected:
    void paintEvent(QPaintEvent *) override {
        QStylePainter painter(this);
        QStyleOptionButton option;
        initStyleOption(&option);
        const int frame = style()->pixelMetric(QStyle::PM_DefaultFrameWidth, &option, this);
        const int margin = style()->pixelMetric(QStyle::PM_ButtonMargin, &option, this);
        option.text = fontMetrics().elidedText(text(), Qt::ElideRight, std::max(0, width() - (frame + margin) * 2));
        painter.drawControl(QStyle::CE_PushButton, option);
    }
};

class ExpertRow : public QWidget {
public:
    ExpertRow(QLabel *caption, QWidget *field, QWidget *parent) : QWidget(parent), caption_(caption), field_(field) {
        setMinimumWidth(0);
        setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        caption_->setParent(this);
        field_->setParent(this);
        box_ = new QBoxLayout(QBoxLayout::TopToBottom, this);
        box_->setContentsMargins(0, 2, 0, 2);
        box_->setSpacing(4);
        box_->addWidget(caption_);
        box_->addWidget(field_);
    }

    void setStacked(bool stacked) {
        if (stacked_ == stacked) {
            return;
        }
        stacked_ = stacked;
        box_->setDirection(stacked ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
        box_->setStretch(0, stacked ? 0 : 1);
        box_->setStretch(1, 1);
        updateGeometry();
    }

private:
    QLabel *caption_ = nullptr;
    QWidget *field_ = nullptr;
    QBoxLayout *box_ = nullptr;
    bool stacked_ = true;
};

class CheckLine : public QWidget {
public:
    CheckLine(QCheckBox *box, QLabel *caption, QWidget *parent) : QWidget(parent), box_(box), caption_(caption) {
        setMinimumWidth(0);
        setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        box_->setParent(this);
        box_->setText(QString());
        caption_->setParent(this);
        auto *row = new QHBoxLayout(this);
        row->setContentsMargins(0, 2, 0, 2);
        row->setSpacing(8);
        row->addWidget(box_, 0, Qt::AlignTop);
        row->addWidget(caption_, 1);
        caption_->installEventFilter(this);
    }

    QSize minimumSizeHint() const override {
        QSize hint = QWidget::minimumSizeHint();
        hint.setWidth(24);
        return hint;
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override {
        if (watched == caption_ && event->type() == QEvent::MouseButtonRelease) {
            box_->toggle();
            return true;
        }
        return QWidget::eventFilter(watched, event);
    }

private:
    QCheckBox *box_ = nullptr;
    QLabel *caption_ = nullptr;
};

class CropGrid : public QWidget {
public:
    CropGrid(QSpinBox *x, QSpinBox *y, QSpinBox *widthBox, QSpinBox *heightBox, QWidget *parent) : QWidget(parent) {
        setMinimumWidth(0);
        setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        x->setPrefix(QStringLiteral("x "));
        y->setPrefix(QStringLiteral("y "));
        widthBox->setPrefix(QStringLiteral("w "));
        heightBox->setPrefix(QStringLiteral("h "));
        auto *grid = new QGridLayout(this);
        grid->setContentsMargins(0, 0, 0, 0);
        grid->setHorizontalSpacing(6);
        grid->setVerticalSpacing(4);
        grid->addWidget(x, 0, 0);
        grid->addWidget(y, 0, 1);
        grid->addWidget(widthBox, 1, 0);
        grid->addWidget(heightBox, 1, 1);
        grid->setColumnStretch(0, 1);
        grid->setColumnStretch(1, 1);
    }

    QSize minimumSizeHint() const override { return QSize(96, QWidget::minimumSizeHint().height()); }
};

class ExpertPage : public QWidget {
public:
    explicit ExpertPage(QWidget *parent = nullptr) : QWidget(parent) {
        setObjectName(QStringLiteral("expertPage"));
        setMinimumWidth(0);
        setSizePolicy(QSizePolicy::Ignored, QSizePolicy::MinimumExpanding);
        layout_ = new QVBoxLayout(this);
        layout_->setContentsMargins(12, 10, 12, 10);
        layout_->setSpacing(6);
    }

    void addRow(ExpertRow *row) {
        rows_.push_back(row);
        layout_->addWidget(row);
    }

    void addItem(QWidget *widget) { layout_->addWidget(widget); }

    QSize sizeHint() const override {
        QSize hint = QWidget::sizeHint();
        hint.setWidth(0);
        return hint;
    }

    QSize minimumSizeHint() const override {
        QSize hint = QWidget::minimumSizeHint();
        hint.setWidth(0);
        return hint;
    }

protected:
    void resizeEvent(QResizeEvent *event) override {
        QWidget::resizeEvent(event);
        const bool stacked = width() < kExpertStackWidth;
        for (ExpertRow *row : rows_) {
            row->setStacked(stacked);
        }
    }

private:
    QVBoxLayout *layout_ = nullptr;
    std::vector<ExpertRow *> rows_;
};

class ViewportFit : public QObject {
public:
    ViewportFit(QScrollArea *scroll, ExpertPage *page, QObject *parent)
        : QObject(parent), scroll_(scroll), page_(page) {}

    bool eventFilter(QObject *object, QEvent *event) override {
        if (object == scroll_->viewport() && event->type() == QEvent::Resize) {
            const int width = scroll_->viewport()->width();
            if (width > 0 && page_->width() != width) {
                page_->setFixedWidth(width);
            }
        }
        return QObject::eventFilter(object, event);
    }

private:
    QScrollArea *scroll_ = nullptr;
    ExpertPage *page_ = nullptr;
};

class ElideLabel : public QLabel {
public:
    explicit ElideLabel(QWidget *parent = nullptr) : QLabel(parent) {
        setMinimumWidth(0);
        setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    }

    void setFullText(const QString &text) {
        full_ = text;
        applyElide();
    }

protected:
    void resizeEvent(QResizeEvent *event) override {
        QLabel::resizeEvent(event);
        applyElide();
    }

private:
    void applyElide() {
        const int width = std::max(0, this->width());
        const QString shown = fontMetrics().elidedText(full_, Qt::ElideRight, width > 0 ? width : 100000);
        if (text() != shown) {
            QLabel::setText(shown);
        }
    }

    QString full_;
};

void clearLayout(QLayout *layout) {
    if (layout == nullptr) {
        return;
    }
    while (QLayoutItem *item = layout->takeAt(0)) {
        if (item->widget() != nullptr) {
            item->widget()->deleteLater();
        }
        delete item;
    }
}

QString percentText(double scale) {
    return QString::number(static_cast<int>(std::lround(scale * 100.0))) + QStringLiteral("%");
}

QString zoomText(ZoomMode mode, double scale) {
    switch (mode) {
    case ZoomMode::Fit:
        return label(QStringLiteral("simple.viewer.fit"));
    case ZoomMode::Fill:
        return label(QStringLiteral("lucent.action.fill"));
    case ZoomMode::FitWidth:
        return label(QStringLiteral("lucent.action.fit_width"));
    case ZoomMode::FitHeight:
        return label(QStringLiteral("lucent.action.fit_height"));
    case ZoomMode::OneToOne:
    case ZoomMode::Free:
        return percentText(scale);
    }
    return percentText(scale);
}

}  // namespace

struct Chrome::Impl {
    QWidget *host = nullptr;
    ChromeCallbacks cb;
    PresetFile presets;
    SettingState state;
    bool presetsOk = false;
    bool historyOn = true;
    bool infoOn = true;
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
    std::vector<QPushButton *> barButtons;
    QPushButton *moreButton = nullptr;
    QWidget *presetRow = nullptr;
    QWidget *viewRow = nullptr;
    QHBoxLayout *viewChips = nullptr;
    QWidget *enhanceRow = nullptr;
    QHBoxLayout *enhanceChips = nullptr;
    QLabel *customLabel = nullptr;
    QPushButton *resetButton = nullptr;
    ElideLabel *featureText = nullptr;
    QPushButton *installButton = nullptr;
    QPushButton *infoToggle = nullptr;
    QLabel *infoLine = nullptr;
    QPushButton *cancelButton = nullptr;
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
    std::vector<std::pair<QString, QLabel *>> expertLabels;
    std::vector<std::pair<QString, QPushButton *>> expertButtons;
    ResizeSpec resize;
    QComboBox *resizeUnit = nullptr;
    QSpinBox *resizeWidth = nullptr;
    QSpinBox *resizeHeight = nullptr;
    QSpinBox *resizePercent = nullptr;
    QCheckBox *resizeAspect = nullptr;
    QComboBox *resizeSample = nullptr;
    QSpinBox *dialogWidth = nullptr;
    QSpinBox *dialogHeight = nullptr;
    QSpinBox *dialogPercent = nullptr;
    QComboBox *dialogUnit = nullptr;
    QCheckBox *dialogAspect = nullptr;
    QComboBox *dialogSample = nullptr;
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
    setOptionAvailable(impl_->presets, "enhance", "enhance", enlarge);
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
    impl_->infoOn = jsonBool(stored, "ui.info_visible", true);
    impl_->expertWidth = std::clamp(jsonInt(stored, "ui.expert_width", kExpertDefault), kExpertMin, kExpertMax);
    if (!qgetenv("LUCENT_SCREENSHOT").isEmpty()) {
        bool forcedOk = false;
        const int forced = qEnvironmentVariableIntValue("LUCENT_EXPERT_WIDTH", &forcedOk);
        if (forcedOk) {
            impl_->expertWidth = std::clamp(forced, kExpertMin, kExpertMax);
        }
    }
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
    impl_->barButtons.clear();
    impl_->bar = new QWidget(impl_->host);
    impl_->bar->setObjectName(QStringLiteral("topBar"));
    auto *row = new QHBoxLayout(impl_->bar);
    row->setContentsMargins(8, 0, 8, 0);
    row->setSpacing(6);
    const auto add = [this, row](const QString &text, const QString &tip, const std::function<void()> &fn) {
        auto *button = new QPushButton(text, impl_->bar);
        if (!tip.isEmpty()) {
            button->setToolTip(tip);
        }
        QObject::connect(button, &QPushButton::clicked, impl_->host, fn);
        row->addWidget(button);
        impl_->barButtons.push_back(button);
        return button;
    };
    const auto labeled = [this, add](const char *key, const std::function<void()> &fn, const char *tipKey = nullptr) {
        const QString text = label(QString::fromUtf8(key));
        const QString tip = tipKey == nullptr ? text : label(QString::fromUtf8(tipKey));
        return add(text, tip, fn);
    };
    labeled("simple.viewer.open", [this] { openFile(); });
    labeled("simple.viewer.save", [this] {
        if (impl_->cb.save) {
            impl_->cb.save();
        }
    });
    labeled("simple.viewer.previous", [this] {
        if (impl_->cb.step) {
            impl_->cb.step(-1);
        }
    });
    labeled("simple.viewer.next", [this] {
        if (impl_->cb.step) {
            impl_->cb.step(1);
        }
    });
    auto *rotate = add(QStringLiteral("↻"), label(QStringLiteral("lucent.action.rotate_right")), [this] {
        if (impl_->cb.rotate) {
            impl_->cb.rotate(true);
        }
    });
    rotate->setAccessibleName(rotate->toolTip());
    labeled("lucent.action.crop", [this] {
        if (impl_->cb.crop) {
            impl_->cb.crop();
        }
    });
    labeled("lucent.action.delete", [this] {
        if (impl_->cb.trash) {
            impl_->cb.trash();
        }
    }, "lucent.action.trash");
    labeled("lucent.action.slideshow", [this] {
        if (impl_->cb.slideshow) {
            impl_->cb.slideshow();
        }
    });
    labeled("lucent.action.fullscreen", [this] {
        if (impl_->cb.fullscreen) {
            impl_->cb.fullscreen();
        }
    });
    row->addStretch(1);
    impl_->moreButton = new QPushButton(label(QStringLiteral("lucent.action.more")), impl_->bar);
    QObject::connect(impl_->moreButton, &QPushButton::clicked, impl_->host, [this] { openMenu(); });
    row->addWidget(impl_->moreButton);
}

void Chrome::fitToolbar() {
    if (impl_->bar == nullptr || impl_->moreButton == nullptr || impl_->bar->layout() == nullptr) {
        return;
    }
    for (QPushButton *button : impl_->barButtons) {
        button->setVisible(true);
    }
    impl_->moreButton->setVisible(true);
    impl_->bar->layout()->activate();
    const QMargins margins = impl_->bar->layout()->contentsMargins();
    const int spacing = impl_->bar->layout()->spacing();
    const int inner = std::max(0, impl_->bar->width() - margins.left() - margins.right());
    const auto widthOf = [](const QWidget *widget) {
        return std::max(widget->sizeHint().width(), widget->minimumSizeHint().width());
    };
    int used = widthOf(impl_->moreButton);
    int visible = 1;
    for (QPushButton *button : impl_->barButtons) {
        used += widthOf(button);
        visible += 1;
    }
    used += spacing * std::max(0, visible - 1);
    for (auto it = impl_->barButtons.rbegin(); it != impl_->barButtons.rend() && used > inner; ++it) {
        used -= widthOf(*it) + spacing;
        (*it)->setVisible(false);
    }
}

void Chrome::buildPresets() {
    impl_->presetRow = new QWidget(impl_->host);
    auto *stack = new QVBoxLayout(impl_->presetRow);
    stack->setContentsMargins(0, 0, 0, 0);
    stack->setSpacing(kRowGap);

    impl_->viewRow = new QWidget(impl_->presetRow);
    auto *view = new QHBoxLayout(impl_->viewRow);
    view->setContentsMargins(0, 0, 0, 0);
    view->setSpacing(8);
    impl_->viewChips = new QHBoxLayout;
    impl_->viewChips->setContentsMargins(0, 0, 0, 0);
    impl_->viewChips->setSpacing(8);
    view->addLayout(impl_->viewChips, 1);
    impl_->customLabel = new QLabel(impl_->viewRow);
    impl_->customLabel->setObjectName(QStringLiteral("presetLabel"));
    impl_->resetButton = new QPushButton(impl_->viewRow);
    QObject::connect(impl_->resetButton, &QPushButton::clicked, impl_->host, [this] { resetView(); });
    view->addWidget(impl_->customLabel);
    view->addWidget(impl_->resetButton);

    impl_->enhanceRow = new QWidget(impl_->presetRow);
    auto *enhance = new QHBoxLayout(impl_->enhanceRow);
    enhance->setContentsMargins(0, 0, 0, 0);
    enhance->setSpacing(8);
    impl_->enhanceChips = new QHBoxLayout;
    impl_->enhanceChips->setContentsMargins(0, 0, 0, 0);
    impl_->enhanceChips->setSpacing(8);
    enhance->addLayout(impl_->enhanceChips, 0);
    impl_->featureText = new ElideLabel(impl_->enhanceRow);
    impl_->featureText->setObjectName(QStringLiteral("featureText"));
    impl_->installButton = new QPushButton(impl_->enhanceRow);
    QObject::connect(impl_->installButton, &QPushButton::clicked, impl_->host, [] {
        QDesktopServices::openUrl(QUrl(enlargeReleasesUrl()));
    });
    enhance->addWidget(impl_->featureText, 1);
    enhance->addWidget(impl_->installButton, 0);

    stack->addWidget(impl_->viewRow);
    stack->addWidget(impl_->enhanceRow);
}

void Chrome::buildExpert() {
    impl_->expert = new QFrame(impl_->host);
    impl_->expert->setObjectName(QStringLiteral("expertPanel"));
    auto *scroll = new QScrollArea(impl_->expert);
    scroll->setObjectName(QStringLiteral("expertScroll"));
    scroll->viewport()->setObjectName(QStringLiteral("expertViewport"));
    scroll->verticalScrollBar()->setObjectName(QStringLiteral("expertBar"));
    scroll->horizontalScrollBar()->setObjectName(QStringLiteral("expertBarH"));
    polishScrollBar(scroll->verticalScrollBar());
    polishScrollBar(scroll->horizontalScrollBar());
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    auto *page = new ExpertPage(scroll);
    scroll->setWidget(page);
    scroll->viewport()->installEventFilter(new ViewportFit(scroll, page, scroll));
    auto *wrap = new QVBoxLayout(impl_->expert);
    wrap->setContentsMargins(0, 0, 0, 0);
    wrap->addWidget(scroll);

    const auto caption = [this](const char *key) {
        auto *text = new WrapLabel(label(QString::fromUtf8(key)));
        impl_->expertLabels.emplace_back(QString::fromUtf8(key), text);
        return text;
    };
    const auto namedRow = [this, page, caption](const char *key, QWidget *field) {
        page->addRow(new ExpertRow(caption(key), field, page));
    };
    const auto commandButton = [this, page](const char *key) {
        auto *button = new ShrinkButton(label(QString::fromUtf8(key)), page);
        impl_->expertButtons.emplace_back(QString::fromUtf8(key), button);
        page->addItem(button);
        return button;
    };

    impl_->zoomValue = new WrapLabel(QString(), page);
    impl_->zoomSpin = new ShrinkSpin(page);
    impl_->zoomSpin->setRange(5, 800);
    impl_->zoomSpin->setSuffix(QStringLiteral("%"));
    impl_->colour = new FieldCombo(page);
    impl_->profile = new WrapLabel(QString(), page);
    impl_->profile->setObjectName(QStringLiteral("status"));
    impl_->hdr = new FieldCombo(page);
    impl_->cropX = new ShrinkSpin(page);
    impl_->cropY = new ShrinkSpin(page);
    impl_->cropW = new ShrinkSpin(page);
    impl_->cropH = new ShrinkSpin(page);
    for (QSpinBox *box : {impl_->cropX, impl_->cropY, impl_->cropW, impl_->cropH}) {
        box->setRange(0, 100000);
    }
    impl_->rotate = new FieldCombo(page);
    impl_->format = new FieldCombo(page);
    impl_->quality = new ShrinkSpin(page);
    impl_->quality->setRange(1, 100);
    impl_->frames = new WrapLabel(QString(), page);
    impl_->info = new WrapLabel(QString(), page);
    impl_->info->setObjectName(QStringLiteral("status"));
    impl_->pictures = new FieldCombo(page);
    impl_->slideshow = new ShrinkSpin(page);
    impl_->slideshow->setRange(1, 60);
    impl_->slideshow->setValue(std::max(1, QSettings().value(QStringLiteral("slideshow/ms"), 3000).toInt() / 1000));
    impl_->thumbs = new QCheckBox(page);
    impl_->welcome = new QCheckBox(page);
    impl_->language = new FieldCombo(page);
    impl_->resizeUnit = new FieldCombo(page);
    impl_->resizeWidth = new ShrinkSpin(page);
    impl_->resizeHeight = new ShrinkSpin(page);
    impl_->resizePercent = new ShrinkSpin(page);
    impl_->resizeAspect = new QCheckBox(page);
    impl_->resizeSample = new FieldCombo(page);
    for (QSpinBox *box : {impl_->resizeWidth, impl_->resizeHeight}) {
        box->setRange(1, 100000);
    }
    impl_->resizePercent->setRange(1, 1000);
    impl_->resizePercent->setSuffix(QStringLiteral("%"));

    namedRow("lucent.expert.zoom", impl_->zoomValue);
    page->addItem(impl_->zoomSpin);
    namedRow("lucent.expert.colour", impl_->colour);
    namedRow("lucent.expert.icc", impl_->profile);
    namedRow("lucent.expert.hdr", impl_->hdr);
    namedRow("lucent.expert.crop", new CropGrid(impl_->cropX, impl_->cropY, impl_->cropW, impl_->cropH, page));
    namedRow("lucent.action.resize", impl_->resizeUnit);
    namedRow("lucent.resize.width", impl_->resizeWidth);
    namedRow("lucent.resize.height", impl_->resizeHeight);
    namedRow("lucent.resize.percent", impl_->resizePercent);
    page->addItem(new CheckLine(impl_->resizeAspect, caption("lucent.resize.keep_aspect"), page));
    namedRow("lucent.resize.resample", impl_->resizeSample);
    auto *resizeApply = commandButton("lucent.resize.apply");
    QObject::connect(resizeApply, &QPushButton::clicked, impl_->host, [this] { applyResize(); });
    namedRow("lucent.expert.rotate", impl_->rotate);
    namedRow("lucent.expert.format", impl_->format);
    namedRow("lucent.expert.quality", impl_->quality);
    namedRow("lucent.expert.frames", impl_->frames);
    auto *previous = commandButton("lucent.tools.prev");
    auto *play = commandButton("lucent.tools.play");
    auto *next = commandButton("lucent.tools.next");
    namedRow("lucent.expert.info", impl_->info);
    namedRow("lucent.expert.pictures", impl_->pictures);

    const auto action = [this, commandButton](const char *key, const std::function<void()> &fn) {
        QObject::connect(commandButton(key), &QPushButton::clicked, impl_->host, fn);
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
    namedRow("lucent.expert.slideshow", impl_->slideshow);
    page->addItem(new CheckLine(impl_->thumbs, caption("lucent.expert.thumbnails"), page));
    page->addItem(new CheckLine(impl_->welcome, caption("lucent.expert.welcome"), page));
    namedRow("lucent.expert.language", impl_->language);

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
    QObject::connect(impl_->resizeUnit, &QComboBox::currentIndexChanged, impl_->host, [this](int index) {
        if (impl_->applying) {
            return;
        }
        if (index == 1) {
            editResize(resizeFromPercent(impl_->resize, impl_->resize.percent));
        } else if (index == 0) {
            editResize(resizeFromWidth(impl_->resize, impl_->resize.width));
        }
    });
    QObject::connect(impl_->resizeWidth, &QSpinBox::valueChanged, impl_->host, [this](int value) {
        if (impl_->applying || impl_->resize.byPercent) {
            return;
        }
        editResize(resizeFromWidth(impl_->resize, value));
    });
    QObject::connect(impl_->resizeHeight, &QSpinBox::valueChanged, impl_->host, [this](int value) {
        if (impl_->applying || impl_->resize.byPercent) {
            return;
        }
        editResize(resizeFromHeight(impl_->resize, value));
    });
    QObject::connect(impl_->resizePercent, &QSpinBox::valueChanged, impl_->host, [this](int value) {
        if (impl_->applying || !impl_->resize.byPercent) {
            return;
        }
        editResize(resizeFromPercent(impl_->resize, value));
    });
    QObject::connect(impl_->resizeAspect, &QCheckBox::toggled, impl_->host, [this](bool on) {
        if (impl_->applying) {
            return;
        }
        editResize(resizeKeepAspect(impl_->resize, on));
    });
    QObject::connect(impl_->resizeSample, &QComboBox::currentIndexChanged, impl_->host, [this](int index) {
        if (impl_->applying) {
            return;
        }
        impl_->resize.smooth = index != 1;
        syncResizeFields();
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
    impl_->infoToggle = new QPushButton(label(QStringLiteral("lucent.expert.info")), impl_->statusRow);
    impl_->infoToggle->setObjectName(QStringLiteral("infoToggle"));
    impl_->infoToggle->setCheckable(true);
    impl_->infoToggle->setChecked(impl_->infoOn);
    impl_->infoLine = new QLabel(impl_->statusRow);
    impl_->infoLine->setObjectName(QStringLiteral("status"));
    impl_->infoLine->setMinimumWidth(0);
    impl_->infoLine->setVisible(impl_->infoOn);
    impl_->progressText = new QLabel(impl_->statusRow);
    impl_->progressText->setObjectName(QStringLiteral("progressText"));
    impl_->progress = new QProgressBar(impl_->statusRow);
    impl_->progress->setObjectName(QStringLiteral("progress"));
    impl_->progress->setTextVisible(false);
    impl_->cancelButton = new QPushButton(label(QStringLiteral("mdl.cancel")), impl_->statusRow);
    QObject::connect(impl_->infoToggle, &QPushButton::toggled, impl_->host, [this](bool on) {
        if (impl_->applying) {
            return;
        }
        impl_->infoOn = on;
        persistFlag("ui.info_visible", Json::fromBool(on));
        if (impl_->infoLine != nullptr) {
            impl_->infoLine->setVisible(on);
        }
    });
    QObject::connect(impl_->cancelButton, &QPushButton::clicked, impl_->host, [this] { cancelJob(); });
    row->addWidget(impl_->infoToggle);
    row->addWidget(impl_->infoLine, 1);
    row->addWidget(impl_->progressText, 1);
    row->addWidget(impl_->progress, 1);
    row->addWidget(impl_->cancelButton);
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
    if (impl_->viewChips == nullptr || impl_->enhanceChips == nullptr) {
        return;
    }
    clearLayout(impl_->viewChips);
    clearLayout(impl_->enhanceChips);
    const auto chipButton = [](QWidget *parent, const QString &caption, bool available, bool checked) {
        auto *chip = new QPushButton(caption, parent);
        chip->setProperty("chip", true);
        chip->setProperty("available", available ? QStringLiteral("true") : QStringLiteral("false"));
        chip->setCheckable(available);
        chip->setChecked(available && checked);
        chip->setMinimumWidth(0);
        return chip;
    };
    const auto groupLabel = [](QWidget *parent, const QString &text) {
        auto *label = new QLabel(text, parent);
        label->setObjectName(QStringLiteral("presetLabel"));
        return label;
    };
    impl_->viewChips->addWidget(groupLabel(impl_->viewRow, label(QStringLiteral("simple.preset.view"))));
    const PresetGroup *view = findGroup(impl_->presets, "view");
    const ChipStatus viewStatus = impl_->presetsOk ? chipFor(impl_->presets, impl_->state, "view") : ChipStatus{};
    if (view != nullptr) {
        for (const PresetOption &option : view->options) {
            const QString name = label(qs(option.labelKey));
            QString caption = name;
            const bool stored = viewStatus.optionId == option.id && viewStatus.kind != ChipKind::Custom;
            if (stored && viewStatus.kind == ChipKind::Modified) {
                caption = qs(formatChip(ChipKind::Modified, ss(name), ss(label(QStringLiteral("simple.preset.changed"))),
                                         ss(label(QStringLiteral("simple.preset.custom")))));
            }
            const bool checked = stored && (viewStatus.kind == ChipKind::Exact || viewStatus.kind == ChipKind::Modified);
            auto *chip = chipButton(impl_->viewRow, caption, option.available, checked);
            const QString id = qs(option.id);
            QObject::connect(chip, &QPushButton::clicked, impl_->host, [this, id] { chooseChip(id); });
            impl_->viewChips->addWidget(chip);
        }
    }
    impl_->viewChips->addStretch(1);
    const bool custom = viewStatus.kind == ChipKind::Custom;
    const bool changed = viewStatus.kind != ChipKind::Exact;
    impl_->customLabel->setVisible(custom);
    impl_->customLabel->setText(label(QStringLiteral("simple.preset.custom")));
    const QString storedName = label(qs(viewStatus.labelKey));
    impl_->resetButton->setVisible(changed && !viewStatus.optionId.empty());
    impl_->resetButton->setText(labelWithName(label(QStringLiteral("simple.preset.reset")), storedName));

    impl_->enhanceChips->addWidget(groupLabel(impl_->enhanceRow, label(QStringLiteral("lucent.preset.enhance"))));
    const PresetGroup *enhanceGroup = findGroup(impl_->presets, "enhance");
    const PresetOption *enhance = enhanceGroup == nullptr ? nullptr : findOption(*enhanceGroup, "enhance");
    const ChipStatus enhanceStatus = impl_->presetsOk ? chipFor(impl_->presets, impl_->state, "enhance") : ChipStatus{};
    if (enhance != nullptr) {
        const bool checked = enhanceStatus.optionId == enhance->id && enhanceStatus.kind == ChipKind::Exact;
        auto *chip = chipButton(impl_->enhanceRow, label(qs(enhance->labelKey)), enhance->available, checked);
        QObject::connect(chip, &QPushButton::clicked, impl_->host, [this] { chooseEnhance(); });
        impl_->enhanceChips->addWidget(chip);
    }
    const PresetGroup *pictures = findGroup(impl_->presets, "pictures");
    const ChipStatus pictureStatus = impl_->presetsOk ? chipFor(impl_->presets, impl_->state, "pictures") : ChipStatus{};
    if (pictures != nullptr) {
        for (const PresetOption &option : pictures->options) {
            const bool checked = pictureStatus.optionId == option.id && pictureStatus.kind == ChipKind::Exact;
            auto *chip = chipButton(impl_->enhanceRow, label(qs(option.labelKey)), option.available, checked);
            const QString id = qs(option.id);
            QObject::connect(chip, &QPushButton::clicked, impl_->host, [this, id] { choosePicture(id); });
            impl_->enhanceChips->addWidget(chip);
        }
    }
    const bool missing = enhance != nullptr && !enhance->available;
    impl_->featureText->setVisible(missing);
    impl_->installButton->setVisible(missing);
    if (enhance != nullptr) {
        impl_->featureText->setFullText(label(qs(enhance->reasonKey)));
        impl_->installButton->setText(label(qs(enhance->actionLabelKey)));
    }
}

void Chrome::refreshExpert() {
    impl_->applying = true;
    for (const auto &item : impl_->expertLabels) {
        item.second->setText(label(item.first));
    }
    for (const auto &item : impl_->expertButtons) {
        item.second->setText(label(item.first));
    }
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
    impl_->resizeUnit->clear();
    impl_->resizeUnit->addItem(label(QStringLiteral("lucent.resize.pixels")));
    impl_->resizeUnit->addItem(label(QStringLiteral("lucent.resize.by_percent")));
    impl_->resizeSample->clear();
    impl_->resizeSample->addItem(label(QStringLiteral("lucent.resize.smooth")));
    impl_->resizeSample->addItem(label(QStringLiteral("lucent.resize.nearest")));
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
    refreshExpert();
    applyZoom();
}

void Chrome::choosePicture(const QString &id) {
    if (!chooseOption(impl_->presets, impl_->state, "pictures", ss(id))) {
        refreshChips();
        return;
    }
    persistState();
    refreshChips();
    refreshExpert();
}

void Chrome::chooseEnhance() {
    if (!chooseOption(impl_->presets, impl_->state, "enhance", "enhance")) {
        refreshChips();
        return;
    }
    persistState();
    refreshChips();
    refreshExpert();
    startJob();
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
    if (zoom.kind == Json::Kind::String && zoom.text == "fill") {
        impl_->cb.zoom(ZoomMode::Fill);
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
    switch (mode) {
    case ZoomMode::Fit:
        value = Json::fromString("fit");
        break;
    case ZoomMode::Fill:
        value = Json::fromString("fill");
        break;
    case ZoomMode::OneToOne:
        value = Json::fromString("100");
        break;
    case ZoomMode::FitWidth:
    case ZoomMode::FitHeight:
    case ZoomMode::Free:
        value = Json::fromString(std::to_string(std::max(1, static_cast<int>(std::lround(scale * 100.0)))));
        break;
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
    impl_->zoomValue->setText(zoomText(live.mode, live.scale));
    if (impl_->infoLine != nullptr) {
        impl_->infoLine->setText(live.info);
        impl_->infoLine->setVisible(impl_->infoOn);
    }
    impl_->zoomSpin->setValue(std::clamp(static_cast<int>(std::lround(live.scale * 100.0)), 5, 800));
    impl_->cropX->setValue(live.cropX);
    impl_->cropY->setValue(live.cropY);
    impl_->cropW->setValue(live.cropW);
    impl_->cropH->setValue(live.cropH);
    impl_->frames->setText(QString::number(live.frame + 1) + QStringLiteral(" / ") + QString::number(std::max(1, live.frameCount)));
    impl_->info->setText(live.info);
    impl_->profile->setText(live.profile.isEmpty() ? label(QStringLiteral("lucent.colour.srgb")) : live.profile);
    if (live.imageW > 0 && live.imageH > 0
        && (live.imageW != impl_->resize.sourceWidth || live.imageH != impl_->resize.sourceHeight)) {
        impl_->resize = resizeFromSource(live.imageW, live.imageH, impl_->resize.keepAspect, impl_->resize.byPercent, impl_->resize.smooth);
    }
    syncResizeFields();
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
    const int statusH = kStatus;
    const int historyH = history ? kHistory : 0;
    const int expertW = panelOpen() ? impl_->expertWidth : 0;
    int y = kPageTop;
    impl_->bar->setFixedHeight(kTop);
    impl_->bar->setGeometry(kPageLeft, y, std::max(0, width - kPageLeft - kPageRight), kTop);
    impl_->bar->show();
    fitToolbar();
    impl_->presetRow->show();
    if (impl_->progressText != nullptr) {
        impl_->progressText->setVisible(running);
    }
    if (impl_->progress != nullptr) {
        impl_->progress->setVisible(running);
    }
    if (impl_->cancelButton != nullptr) {
        impl_->cancelButton->setVisible(running);
    }
    y += kTop + kBarGap;
    const int presetH = kChip + kRowGap + kChip;
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
    impl_->barButtons.clear();
    impl_->moreButton = nullptr;
    delete impl_->bar;
    impl_->bar = nullptr;
    buildBar();
    refreshChips();
    if (impl_->infoToggle != nullptr) {
        impl_->applying = true;
        impl_->infoToggle->setText(label(QStringLiteral("lucent.expert.info")));
        impl_->infoToggle->setChecked(impl_->infoOn);
        impl_->applying = false;
    }
    if (impl_->cancelButton != nullptr) {
        impl_->cancelButton->setText(label(QStringLiteral("mdl.cancel")));
    }
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

void Chrome::editResize(const ResizeSpec &spec) {
    impl_->resize = spec;
    syncResizeFields();
}

void Chrome::syncResizeFields() {
    const bool wasApplying = impl_->applying;
    impl_->applying = true;
    const ResizeSpec &spec = impl_->resize;
    const auto spins = [&](QSpinBox *width, QSpinBox *height, QSpinBox *percent) {
        if (width != nullptr) {
            width->setValue(spec.width);
            width->setEnabled(!spec.byPercent);
        }
        if (height != nullptr) {
            height->setValue(spec.height);
            height->setEnabled(!spec.byPercent);
        }
        if (percent != nullptr) {
            percent->setValue(spec.percent);
            percent->setEnabled(spec.byPercent);
        }
    };
    const auto unit = [&](QComboBox *box) {
        if (box == nullptr || box->count() < 2) {
            return;
        }
        box->setCurrentIndex(spec.byPercent ? 1 : 0);
    };
    const auto sample = [&](QComboBox *box) {
        if (box == nullptr || box->count() < 2) {
            return;
        }
        box->setCurrentIndex(spec.smooth ? 0 : 1);
    };
    const auto aspect = [&](QCheckBox *box) {
        if (box != nullptr) {
            box->setChecked(spec.keepAspect);
        }
    };
    spins(impl_->resizeWidth, impl_->resizeHeight, impl_->resizePercent);
    spins(impl_->dialogWidth, impl_->dialogHeight, impl_->dialogPercent);
    unit(impl_->resizeUnit);
    unit(impl_->dialogUnit);
    sample(impl_->resizeSample);
    sample(impl_->dialogSample);
    aspect(impl_->resizeAspect);
    aspect(impl_->dialogAspect);
    impl_->applying = wasApplying;
}

void Chrome::applyResize() {
    if (impl_->cb.resizeImage) {
        impl_->cb.resizeImage(impl_->resize.width, impl_->resize.height, impl_->resize.smooth);
    }
}

void Chrome::openResizeDialog() {
    QDialog dialog(impl_->host);
    struct ClearDialogFields {
        Impl *impl;
        ~ClearDialogFields() {
            impl->dialogWidth = nullptr;
            impl->dialogHeight = nullptr;
            impl->dialogPercent = nullptr;
            impl->dialogUnit = nullptr;
            impl->dialogAspect = nullptr;
            impl->dialogSample = nullptr;
        }
    } clear{impl_};
    dialog.setWindowTitle(label(QStringLiteral("lucent.action.resize")));
    dialog.setMinimumWidth(360);
    auto *layout = new QVBoxLayout(&dialog);
    auto *formHost = new QWidget(&dialog);
    auto *form = new QFormLayout(formHost);
    form->setContentsMargins(0, 0, 0, 0);
    impl_->dialogUnit = new QComboBox(formHost);
    impl_->dialogUnit->addItem(label(QStringLiteral("lucent.resize.pixels")));
    impl_->dialogUnit->addItem(label(QStringLiteral("lucent.resize.by_percent")));
    impl_->dialogWidth = new QSpinBox(formHost);
    impl_->dialogHeight = new QSpinBox(formHost);
    impl_->dialogPercent = new QSpinBox(formHost);
    impl_->dialogAspect = new QCheckBox(label(QStringLiteral("lucent.resize.keep_aspect")), formHost);
    impl_->dialogSample = new QComboBox(formHost);
    impl_->dialogSample->addItem(label(QStringLiteral("lucent.resize.smooth")));
    impl_->dialogSample->addItem(label(QStringLiteral("lucent.resize.nearest")));
    for (QSpinBox *box : {impl_->dialogWidth, impl_->dialogHeight}) {
        box->setRange(1, 100000);
    }
    impl_->dialogPercent->setRange(1, 1000);
    impl_->dialogPercent->setSuffix(QStringLiteral("%"));
    form->addRow(label(QStringLiteral("lucent.action.resize")), impl_->dialogUnit);
    form->addRow(label(QStringLiteral("lucent.resize.width")), impl_->dialogWidth);
    form->addRow(label(QStringLiteral("lucent.resize.height")), impl_->dialogHeight);
    form->addRow(label(QStringLiteral("lucent.resize.percent")), impl_->dialogPercent);
    form->addRow(QString(), impl_->dialogAspect);
    form->addRow(label(QStringLiteral("lucent.resize.resample")), impl_->dialogSample);
    layout->addWidget(formHost);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    QObject::connect(impl_->dialogUnit, &QComboBox::currentIndexChanged, &dialog, [this](int index) {
        if (impl_->applying) {
            return;
        }
        if (index == 1) {
            editResize(resizeFromPercent(impl_->resize, impl_->resize.percent));
        } else if (index == 0) {
            editResize(resizeFromWidth(impl_->resize, impl_->resize.width));
        }
    });
    QObject::connect(impl_->dialogWidth, &QSpinBox::valueChanged, &dialog, [this](int value) {
        if (impl_->applying || impl_->resize.byPercent) {
            return;
        }
        editResize(resizeFromWidth(impl_->resize, value));
    });
    QObject::connect(impl_->dialogHeight, &QSpinBox::valueChanged, &dialog, [this](int value) {
        if (impl_->applying || impl_->resize.byPercent) {
            return;
        }
        editResize(resizeFromHeight(impl_->resize, value));
    });
    QObject::connect(impl_->dialogPercent, &QSpinBox::valueChanged, &dialog, [this](int value) {
        if (impl_->applying || !impl_->resize.byPercent) {
            return;
        }
        editResize(resizeFromPercent(impl_->resize, value));
    });
    QObject::connect(impl_->dialogAspect, &QCheckBox::toggled, &dialog, [this](bool on) {
        if (impl_->applying) {
            return;
        }
        editResize(resizeKeepAspect(impl_->resize, on));
    });
    QObject::connect(impl_->dialogSample, &QComboBox::currentIndexChanged, &dialog, [this](int index) {
        if (impl_->applying || index < 0) {
            return;
        }
        impl_->resize.smooth = index != 1;
        syncResizeFields();
    });
    const bool wasApplying = impl_->applying;
    impl_->applying = true;
    syncResizeFields();
    impl_->applying = wasApplying;
    if (qgetenv("LUCENT_SCREENSHOT") == "resize") {
        dialog.show();
        dialog.raise();
        QTimer::singleShot(200, &dialog, [&dialog] { saveGrab(&dialog, QStringLiteral("resize")); });
        QEventLoop loop;
        QTimer::singleShot(800, &loop, &QEventLoop::quit);
        loop.exec();
        return;
    }
    const ResizeSpec saved = impl_->resize;
    if (dialog.exec() == QDialog::Accepted) {
        applyResize();
        return;
    }
    editResize(saved);
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
    menu.addAction(label(QStringLiteral("lucent.action.resize")), [this] { openResizeDialog(); });
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
    if (qgetenv("LUCENT_SCREENSHOT") == "menu" || qgetenv("LUCENT_SCREENSHOT") == "overflow") {
        menu.setObjectName(QStringLiteral("overflowMenu"));
        menu.popup(impl_->bar->mapToGlobal(QPoint(impl_->bar->width() - 120, impl_->bar->height())));
        const bool wholeWindow = qgetenv("LUCENT_SCREENSHOT") == "overflow";
        QTimer::singleShot(250, &menu, [this, &menu, wholeWindow] {
            if (wholeWindow) {
                saveScreen(impl_->host, &menu, QStringLiteral("overflow"));
            } else {
                saveGrab(&menu, QStringLiteral("menu"));
            }
        });
        QEventLoop loop;
        QTimer::singleShot(900, &loop, &QEventLoop::quit);
        loop.exec();
        return;
    }
    menu.exec(impl_->bar->mapToGlobal(QPoint(impl_->bar->width() - 120, impl_->bar->height())));
}

void Chrome::contextMenu(const QPoint &globalPos) {
    QMenu menu(impl_->host);
    menu.addAction(label(QStringLiteral("simple.viewer.open")), [this] { openFile(); });
    QAction *previousImage = menu.addAction(label(QStringLiteral("simple.viewer.previous")), [this] {
        if (impl_->cb.step) {
            impl_->cb.step(-1);
        }
    });
    previousImage->setToolTip(previousImage->text());
    QAction *nextImage = menu.addAction(label(QStringLiteral("simple.viewer.next")), [this] {
        if (impl_->cb.step) {
            impl_->cb.step(1);
        }
    });
    nextImage->setToolTip(nextImage->text());
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
    } else if (mode == "menu" || mode == "overflow") {
        QTimer::singleShot(400, impl_->host, [this] { openMenu(); });
    } else if (mode == "hotkeys") {
        QTimer::singleShot(400, impl_->host, [this] { editHotkeys(impl_->host); });
    } else if (mode == "settings") {
        QTimer::singleShot(200, impl_->host, [this] { openSettings(); });
    } else if (mode == "welcome") {
        QTimer::singleShot(800, impl_->host, [this] { saveGrab(impl_->host, QStringLiteral("welcome")); });
    } else if (mode == "simple") {
        impl_->shotOverride = true;
        impl_->shotExpert = false;
        relayout();
        QTimer::singleShot(400, impl_->host, [this] { saveGrab(impl_->host, QStringLiteral("simple")); });
    } else if (mode == "resize") {
        QTimer::singleShot(200, impl_->host, [this] { openResizeDialog(); });
    } else if (mode == "expert") {
        impl_->shotOverride = true;
        impl_->shotExpert = true;
        relayout();
        QTimer::singleShot(400, impl_->host, [this] { saveGrab(impl_->host, QStringLiteral("expert")); });
    }
}

}  // namespace lucent
