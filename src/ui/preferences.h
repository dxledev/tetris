#pragma once

#include "core/storage.h"
#include "modal_backdrop.h"

class QCheckBox;
class QSpinBox;

namespace tetris {

class SettingsModal : public ModalBackdrop {
    Q_OBJECT
public:
    SettingsModal(QWidget *parent, QWidget *background);
    void open(const Preferences &preferences);

signals:
    void applied(const tetris::Preferences &preferences);

private:
    Preferences draft_;
    QCheckBox *ghost_ = nullptr;
    QCheckBox *grid_ = nullptr;
    QCheckBox *autoPause_ = nullptr;
    QSpinBox *moveDelay_ = nullptr;
    QSpinBox *moveRepeat_ = nullptr;
    QSpinBox *softDrop_ = nullptr;
    QSpinBox *lockDelay_ = nullptr;

    QWidget *buildCard();
    QWidget *buildFields();
    void save();
};

class ControlsModal : public ModalBackdrop {
    Q_OBJECT
public:
    ControlsModal(QWidget *parent, QWidget *background);
};

}
