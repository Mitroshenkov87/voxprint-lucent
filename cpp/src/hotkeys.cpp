// Copyright 2026 Aleksandr Mitroshenkov
// SPDX-License-Identifier: Apache-2.0

#include "hotkeys.h"

#include "phrases.h"

#include <QDialog>
#include <QHash>
#include <QKeySequence>
#include <QLabel>
#include <QListWidget>
#include <QSettings>
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
    }
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
    }
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
        layout->addWidget(hint_);
        layout->addWidget(list_);
        const Action actions[] = {
            Action::Next,       Action::Previous, Action::Fit,        Action::Fill,      Action::OneToOne,
            Action::FitWidth,   Action::FitHeight, Action::Fullscreen, Action::Frameless, Action::WindowFit,
            Action::Slideshow,  Action::NextFrame, Action::PreviousFrame, Action::Tools, Action::Language,
            Action::Quit,
        };
        for (const Action action : actions) {
            auto *item = new QListWidgetItem(itemText(action), list_);
            item->setData(Qt::UserRole, static_cast<int>(action));
        }
        list_->setCurrentRow(0);
        resize(420, 480);
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
            Action::Quit,
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
