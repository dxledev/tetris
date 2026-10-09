#pragma once

#include <QColor>
#include <QPixmap>
#include <QPointer>
#include <QWidget>

namespace tetris {

class ModalBackdrop : public QWidget {
    Q_OBJECT
public:
    explicit ModalBackdrop(QWidget *parent, QWidget *background);

    void setCard(QWidget *card);
    void setTint(const QColor &color);
    void open();
    void dismiss();

signals:
    void dismissed();

protected:
    QWidget *card_ = nullptr;

    void paintEvent(QPaintEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QPointer<QWidget> background_;
    QPixmap snapshot_;
    QColor tint_;
    QPointer<QWidget> previousFocus_;
    bool backgroundWasEnabled_ = true;
    bool open_ = false;
    bool capturing_ = false;

    void captureBackground();
    void storeBlurredSnapshot(const QPixmap &snapshot);
    void clearSnapshot();
    void centerCard();
    void focusCard();
    void restoreBackgroundAndFocus();
};

}
