// Copyright 2026 Aleksandr Mitroshenkov
// SPDX-License-Identifier: Apache-2.0

#include "hotkeys.h"

#include "phrases.h"
#include "theme.h"

#include <QApplication>
#include <QDialog>
#include <QDir>
#include <QHash>
#include <QKeySequence>
#include <QLabel>
#include <QListWidget>
#include <QScrollBar>
#include <QSettings>
#include <QTimer>
#include <QVBoxLayout>

namespace lucent {
namespace {

struct Binding {
    Action action;
    int key = 0;
    Qt::KeyboardModifiers mods = Qt::NoModifier;
};

QList<Binding> gBindings;

Qt::KeyboardModifiers significantMods(Qt::KeyboardModifiers mods) {
    return mods & (Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier);
}

const char *actionId(Action action) {
    switch (action) {
    case Action::Next:
        return "next";
    case Action::Previous:
        return "previous";
    case Action::Fit:
        return "fit";
    case Action::Fill:
        return "fill";
    case Action::OneToOne:
        return "one-to-one";
    case Action::FitWidth:
        return "fit-width";
    case Action::FitHeight:
        return "fit-height";
    case Action::Fullscreen:
        return "fullscreen";
    case Action::Frameless:
        return "frameless";
    case Action::WindowFit:
        return "window-fit";
    case Action::Slideshow:
        return "slideshow";
    case Action::NextFrame:
        return "next-frame";
    case Action::PreviousFrame:
        return "previous-frame";
    case Action::Tools:
        return "tools";
    case Action::Hotkeys:
        return "hotkeys";
    case Action::Language:
        return "language";
    case Action::Quit:
        return "quit";
    case Action::Open:
        return "open";
    case Action::RotateLeft:
        return "rotate-left";
    case Action::RotateRight:
        return "rotate-right";
    case Action::FlipHorizontal:
        return "flip-horizontal";
    case Action::FlipVertical:
        return "flip-vertical";
    case Action::Crop:
        return "crop";
    case Action::SaveCopy:
        return "save-copy";
    case Action::SaveAs:
        return "save-as";
    case Action::Trash:
        return "trash";
    case Action::Enlarge:
        return "enlarge";
    case Action::Settings:
        return "settings";
    case Action::CopyImage:
        return "copy-image";
    case Action::CopyPath:
        return "copy-path";
    case Action::Reveal:
        return "reveal";
    case Action::Gallery:
        return "gallery";
    }
    const Action missed = action;
    Q_UNUSED(missed);
    return "unknown";
}

Text labelId(Action action) {
    switch (action) {
    case Action::Next:
        return Text::Next;
    case Action::Previous:
        return Text::Previous;
    case Action::Fit:
        return Text::Fit;
    case Action::Fill:
        return Text::Fill;
    case Action::OneToOne:
        return Text::OneToOne;
    case Action::FitWidth:
        return Text::FitWidth;
    case Action::FitHeight:
        return Text::FitHeight;
    case Action::Fullscreen:
        return Text::Fullscreen;
    case Action::Frameless:
        return Text::Frameless;
    case Action::WindowFit:
        return Text::WindowFit;
    case Action::Slideshow:
        return Text::Slideshow;
    case Action::NextFrame:
        return Text::Frame;
    case Action::PreviousFrame:
        return Text::Frame;
    case Action::Tools:
        return Text::Tools;
    case Action::Hotkeys:
        return Text::Hotkeys;
    case Action::Language:
        return Text::Language;
    case Action::Quit:
        return Text::Quit;
    case Action::Open:
        return Text::Open;
    case Action::RotateLeft:
        return Text::RotateLeft;
    case Action::RotateRight:
        return Text::RotateRight;
    case Action::FlipHorizontal:
        return Text::FlipHorizontal;
    case Action::FlipVertical:
        return Text::FlipVertical;
    case Action::Crop:
        return Text::Crop;
    case Action::SaveCopy:
        return Text::SaveCopy;
    case Action::SaveAs:
        return Text::SaveAs;
    case Action::Trash:
        return Text::MoveToTrash;
    case Action::Enlarge:
        return Text::Enlarge;
    case Action::Settings:
        return Text::Settings;
    case Action::CopyImage:
        return Text::CopyImage;
    case Action::CopyPath:
        return Text::CopyPath;
    case Action::Reveal:
        return Text::RevealInFolder;
    case Action::Gallery:
        return Text::Gallery;
    }
    const Action missed = action;
    Q_UNUSED(missed);
    return Text::Free;
}

void setDefaults() {
    gBindings = {
        {Action::Next, Qt::Key_Right, Qt::NoModifier},
        {Action::Previous, Qt::Key_Left, Qt::NoModifier},
        {Action::Fit, Qt::Key_2, Qt::NoModifier},
        {Action::Fill, Qt::Key_3, Qt::NoModifier},
        {Action::OneToOne, Qt::Key_1, Qt::NoModifier},
        {Action::FitWidth, Qt::Key_4, Qt::NoModifier},
        {Action::FitHeight, Qt::Key_5, Qt::NoModifier},
        {Action::Fullscreen, Qt::Key_F11, Qt::NoModifier},
        {Action::Frameless, Qt::Key_F, Qt::ControlModifier | Qt::ShiftModifier},
        {Action::WindowFit, Qt::Key_W, Qt::ControlModifier | Qt::ShiftModifier},
        {Action::Slideshow, Qt::Key_S, Qt::NoModifier},
        {Action::NextFrame, Qt::Key_Period, Qt::NoModifier},
        {Action::PreviousFrame, Qt::Key_Comma, Qt::NoModifier},
        {Action::Tools, Qt::Key_I, Qt::NoModifier},
        {Action::Hotkeys, Qt::Key_K, Qt::ControlModifier},
        {Action::Language, Qt::Key_L, Qt::ControlModifier},
        {Action::Quit, Qt::Key_Q, Qt::NoModifier},
        {Action::Quit, Qt::Key_Escape, Qt::NoModifier},
        {Action::Open, Qt::Key_O, Qt::ControlModifier},
        {Action::RotateLeft, Qt::Key_Left, Qt::ControlModifier},
        {Action::RotateRight, Qt::Key_Right, Qt::ControlModifier},
        {Action::FlipHorizontal, Qt::Key_H, Qt::ControlModifier},
        {Action::FlipVertical, Qt::Key_V, Qt::ControlModifier},
        {Action::Crop, Qt::Key_X, Qt::ControlModifier | Qt::ShiftModifier},
        {Action::SaveCopy, Qt::Key_S, Qt::ControlModifier},
        {Action::SaveAs, Qt::Key_S, Qt::ControlModifier | Qt::ShiftModifier},
        {Action::Trash, Qt::Key_Delete, Qt::NoModifier},
        {Action::Enlarge, Qt::Key_E, Qt::ControlModifier},
        {Action::Settings, Qt::Key_Comma, Qt::ControlModifier},
        {Action::CopyImage, Qt::Key_C, Qt::ControlModifier},
        {Action::CopyPath, Qt::Key_C, Qt::ControlModifier | Qt::ShiftModifier},
        {Action::Reveal, Qt::Key_R, Qt::ControlModifier},
        {Action::Gallery, Qt::Key_G, Qt::NoModifier},
    };
}

QString sequenceText(int key, Qt::KeyboardModifiers mods) {
    return QKeySequence(static_cast<int>(mods) | key).toString(QKeySequence::NativeText);
}

void rebind(Action action, int key, Qt::KeyboardModifiers mods);

class HotkeyDialog : public QDialog {
public:
    explicit HotkeyDialog(QWidget *parent) : QDialog(parent) {
        setWindowTitle(text(Text::Hotkeys));
        setStyleSheet(parent != nullptr ? parent->styleSheet() : QString());
        auto *layout = new QVBoxLayout(this);
        hint_ = new QLabel(text(Text::PressKey), this);
        list_ = new QListWidget(this);
        list_->setObjectName(QStringLiteral("hotkeyList"));
        list_->viewport()->setObjectName(QStringLiteral("hotkeyViewport"));
        list_->verticalScrollBar()->setObjectName(QStringLiteral("hotkeyBar"));
        polishScrollBar(list_->verticalScrollBar());
        layout->addWidget(hint_);
        layout->addWidget(list_);
        const Action actions[] = {
            Action::Next,       Action::Previous, Action::Fit,        Action::Fill,      Action::OneToOne,
            Action::FitWidth,   Action::FitHeight, Action::Fullscreen, Action::Frameless, Action::WindowFit,
            Action::Slideshow,  Action::NextFrame, Action::PreviousFrame, Action::Tools, Action::Language,
            Action::Quit,       Action::Open,      Action::RotateLeft,    Action::RotateRight, Action::FlipHorizontal,
            Action::FlipVertical, Action::Crop,    Action::SaveCopy,      Action::SaveAs, Action::Trash,
            Action::Enlarge,    Action::Settings,  Action::CopyImage,     Action::CopyPath, Action::Reveal,
            Action::Gallery,
        };
        for (const Action action : actions) {
            auto *item = new QListWidgetItem(itemText(action), list_);
            item->setData(Qt::UserRole, static_cast<int>(action));
        }
        list_->setCurrentRow(0);
        resize(420, 480);
        if (qgetenv("LUCENT_SCREENSHOT") == "hotkeys") {
            QTimer::singleShot(200, this, [this] {
                const QString dir = qEnvironmentVariable("LUCENT_SCREENSHOT_DIR", QStringLiteral("/tmp/lucent-shots"));
                QDir().mkpath(dir);
                grab().save(dir + QStringLiteral("/hotkeys.png"));
                QTimer::singleShot(0, qApp, &QApplication::quit);
            });
        }
    }

protected:
    void keyPressEvent(QKeyEvent *event) override {
        if (event->key() == Qt::Key_Escape && significantMods(event->modifiers()) == Qt::NoModifier) {
            accept();
            return;
        }
        if (event->key() == Qt::Key_unknown || event->key() == Qt::Key_Control || event->key() == Qt::Key_Shift
            || event->key() == Qt::Key_Alt || event->key() == Qt::Key_Meta) {
            return;
        }
        QListWidgetItem *item = list_->currentItem();
        if (item == nullptr) {
            return;
        }
        const auto action = static_cast<Action>(item->data(Qt::UserRole).toInt());
        rebind(action, event->key(), significantMods(event->modifiers()));
        item->setText(itemText(action));
        event->accept();
    }

private:
    static QString itemText(Action action) {
        QString keys;
        for (const Binding &binding : gBindings) {
            if (binding.action != action) {
                continue;
            }
            if (!keys.isEmpty()) {
                keys += QStringLiteral(", ");
            }
            keys += sequenceText(binding.key, binding.mods);
        }
        return text(labelId(action)) + QStringLiteral("  —  ") + keys;
    }

    QLabel *hint_ = nullptr;
    QListWidget *list_ = nullptr;
};

void rebind(Action action, int key, Qt::KeyboardModifiers mods) {
    QList<Binding> kept;
    for (const Binding &binding : gBindings) {
        if (binding.action != action) {
            kept.append(binding);
        }
    }
    kept.append(Binding{action, key, mods});
    gBindings = kept;
    QSettings settings;
    settings.beginGroup(QStringLiteral("hotkeys"));
    settings.setValue(QString::fromLatin1(actionId(action)), sequenceText(key, mods));
    settings.endGroup();
}

}  // namespace

QString actionLabel(Action action) {
    return text(labelId(action));
}

void loadHotkeys() {
    setDefaults();
    QSettings settings;
    settings.beginGroup(QStringLiteral("hotkeys"));
    const QStringList keys = settings.childKeys();
    for (const QString &stored : keys) {
        Action matched = Action::Quit;
        bool known = false;
        const Action actions[] = {
            Action::Next,        Action::Previous,     Action::Fit,           Action::Fill,
            Action::OneToOne,    Action::FitWidth,     Action::FitHeight,     Action::Fullscreen,
            Action::Frameless,   Action::WindowFit,    Action::Slideshow,     Action::NextFrame,
            Action::PreviousFrame, Action::Tools,      Action::Hotkeys,       Action::Language,
            Action::Quit,       Action::Open,         Action::RotateLeft, Action::RotateRight,   Action::FlipHorizontal,
            Action::FlipVertical, Action::Crop,       Action::SaveCopy,   Action::SaveAs,        Action::Trash,
            Action::Enlarge,    Action::Settings,     Action::CopyImage,  Action::CopyPath,      Action::Reveal,
            Action::Gallery,
        };
        for (const Action action : actions) {
            if (stored == QLatin1String(actionId(action))) {
                matched = action;
                known = true;
                break;
            }
        }
        if (!known) {
            continue;
        }
        const QKeySequence sequence(settings.value(stored).toString());
        if (sequence.isEmpty()) {
            continue;
        }
        QList<Binding> kept;
        for (const Binding &binding : gBindings) {
            if (binding.action != matched) {
                kept.append(binding);
            }
        }
        const QKeyCombination combo = sequence[0];
        const int key = combo.key();
        const Qt::KeyboardModifiers mods = significantMods(combo.keyboardModifiers());
        kept.append(Binding{matched, key, mods});
        gBindings = kept;
    }
    settings.endGroup();
}

Action actionFor(const QKeyEvent *event, bool *found) {
    const int key = event->key();
    const Qt::KeyboardModifiers mods = significantMods(event->modifiers());
    for (const Binding &binding : gBindings) {
        if (binding.key == key && binding.mods == mods) {
            if (found != nullptr) {
                *found = true;
            }
            return binding.action;
        }
    }
    if (found != nullptr) {
        *found = false;
    }
    return Action::Quit;
}

void editHotkeys(QWidget *parent) {
    HotkeyDialog dialog(parent);
    dialog.exec();
}

}  // namespace lucent
