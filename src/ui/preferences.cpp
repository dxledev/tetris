#include "preferences.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QShortcut>
#include <QSpinBox>
#include <QVBoxLayout>
#include <functional>

namespace tetris {
namespace {

class ModalScrollArea : public QScrollArea {
public:
    QSize sizeHint() const override {
        return widget() ? widget()->sizeHint() : QScrollArea::sizeHint();
    }
};

QCheckBox *checkBox(QFormLayout *layout, const QString &text, const QString &name) {
    auto *widget = new QCheckBox(text);
    widget->setObjectName(name);
    layout->addRow(widget);
    return widget;
}

QSpinBox *spinBox(QFormLayout *layout, const QString &text, const QString &name, int minimum, int maximum) {
    auto *widget = new QSpinBox;
    widget->setObjectName(name);
    widget->setRange(minimum, maximum);
    widget->setSuffix(" ms");
    widget->setContextMenuPolicy(Qt::NoContextMenu);
    widget->findChild<QLineEdit *>()->setContextMenuPolicy(Qt::NoContextMenu);
    layout->addRow(text, widget);
    return widget;
}

QVBoxLayout *cardLayout(QWidget *card, const QString &title) {
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(18);
    auto *heading = new QLabel(title);
    heading->setObjectName("heading");
    layout->addWidget(heading);
    return layout;
}

void addScrollableBody(QVBoxLayout *layout, QWidget *body) {
    auto *scroll = new ModalScrollArea;
    scroll->setObjectName("modalScroll");
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->verticalScrollBar()->setContextMenuPolicy(Qt::NoContextMenu);
    scroll->horizontalScrollBar()->setContextMenuPolicy(Qt::NoContextMenu);
    scroll->setWidget(body);
    body->setObjectName("modalBody");
    scroll->setMinimumHeight(100);
    layout->addWidget(scroll, 1);
}

void addEnterShortcut(QWidget *parent, const std::function<void()> &action) {
    for (auto key : {Qt::Key_Return, Qt::Key_Enter}) {
        auto *shortcut = new QShortcut(QKeySequence(key), parent);
        shortcut->setContext(Qt::WidgetWithChildrenShortcut);
        QObject::connect(shortcut, &QShortcut::activated, parent, action);
    }
}

QWidget *controlsBody() {
    auto *body = new QWidget;
    auto *form = new QFormLayout(body);
    form->setContentsMargins(0, 0, 0, 0);
    form->setVerticalSpacing(12);
    form->setHorizontalSpacing(24);
    const QList<QPair<QString, QString>> controls{
        {"Move", "←  →"}, {"Soft drop", "↓"}, {"Hard drop", "Space"},
        {"Rotate clockwise", "↑  /  X"}, {"Rotate counterclockwise", "Z"},
        {"Hold piece", "C  /  Shift"}, {"Start or resume", "Enter"},
        {"Pause", "P  /  Escape"}, {"New game", "Ctrl + N"}};
    for (const auto &entry : controls)
        form->addRow(entry.first, new QLabel(entry.second));
    return body;
}

}

SettingsModal::SettingsModal(QWidget *parent, QWidget *background) : ModalBackdrop(parent, background) {
    setObjectName("settingsModal");
    setAccessibleName("Tetris settings");
    setCard(buildCard());
    addEnterShortcut(this, [this] { save(); });
}

QWidget *SettingsModal::buildFields() {
    auto *body = new QWidget;
    auto *form = new QFormLayout(body);
    form->setContentsMargins(0, 0, 0, 0);
    form->setVerticalSpacing(14);
    ghost_ = checkBox(form, "Show landing guide", "prefsGhostToggle");
    grid_ = checkBox(form, "Show board grid", "prefsGridToggle");
    autoPause_ = checkBox(form, "Pause when the window loses focus", "prefsAutoPauseToggle");
    moveDelay_ = spinBox(form, "Hold-to-move delay", "prefsMoveDelaySpin", 50, 1000);
    moveRepeat_ = spinBox(form, "Movement repeat", "prefsMoveRepeatSpin", 10, 250);
    softDrop_ = spinBox(form, "Soft drop repeat", "prefsSoftDropSpin", 10, 250);
    lockDelay_ = spinBox(form, "Piece lock delay", "prefsLockDelaySpin", 100, 2000);
    return body;
}

QWidget *SettingsModal::buildCard() {
    auto *card = new QFrame(this);
    card->setMinimumWidth(430);
    auto *layout = cardLayout(card, "Make it feel right.");
    addScrollableBody(layout, buildFields());
    auto *buttons = new QDialogButtonBox;
    auto *saveButton = buttons->addButton("Save", QDialogButtonBox::AcceptRole);
    saveButton->setObjectName("modalSaveButton");
    saveButton->setCursor(Qt::PointingHandCursor);
    auto *cancelButton = buttons->addButton("Cancel", QDialogButtonBox::RejectRole);
    cancelButton->setObjectName("modalCancelButton");
    cancelButton->setCursor(Qt::PointingHandCursor);
    connect(buttons, &QDialogButtonBox::accepted, this, &SettingsModal::save);
    connect(buttons, &QDialogButtonBox::rejected, this, &SettingsModal::dismiss);
    layout->addWidget(buttons);
    return card;
}

void SettingsModal::open(const Preferences &preferences) {
    draft_ = preferences;
    ghost_->setChecked(preferences.showGhost);
    grid_->setChecked(preferences.showGrid);
    autoPause_->setChecked(preferences.autoPause);
    moveDelay_->setValue(preferences.moveDelayMs);
    moveRepeat_->setValue(preferences.moveRepeatMs);
    softDrop_->setValue(preferences.softDropRepeatMs);
    lockDelay_->setValue(preferences.lockDelayMs);
    ModalBackdrop::open();
}

void SettingsModal::save() {
    for (auto *spin : {moveDelay_, moveRepeat_, softDrop_, lockDelay_})
        spin->interpretText();
    draft_.showGhost = ghost_->isChecked();
    draft_.showGrid = grid_->isChecked();
    draft_.autoPause = autoPause_->isChecked();
    draft_.moveDelayMs = moveDelay_->value();
    draft_.moveRepeatMs = moveRepeat_->value();
    draft_.softDropRepeatMs = softDrop_->value();
    draft_.lockDelayMs = lockDelay_->value();
    emit applied(draft_);
    dismiss();
}

ControlsModal::ControlsModal(QWidget *parent, QWidget *background) : ModalBackdrop(parent, background) {
    setObjectName("controlsModal");
    setAccessibleName("Tetris controls");
    auto *card = new QFrame(this);
    card->setMinimumWidth(420);
    auto *layout = cardLayout(card, "A few keys. Endless possibilities.");
    addScrollableBody(layout, controlsBody());
    auto *note = new QLabel("Clear full rows to score. Save a piece in Hold, and use the outline to see where your next drop will land.");
    note->setObjectName("muted");
    note->setWordWrap(true);
    layout->addWidget(note);
    auto *close = new QPushButton("Close");
    close->setObjectName("controlsCloseButton");
    close->setCursor(Qt::PointingHandCursor);
    connect(close, &QPushButton::clicked, this, &ControlsModal::dismiss);
    layout->addWidget(close, 0, Qt::AlignRight);
    setCard(card);
    addEnterShortcut(this, [this] { dismiss(); });
}

}
