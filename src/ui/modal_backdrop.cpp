#include "modal_backdrop.h"

#include <QApplication>
#include <QEvent>
#include <QGraphicsBlurEffect>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QKeyEvent>
#include <QLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QScopedValueRollback>
#include <QShowEvent>
#include <QHideEvent>
#include <algorithm>
#include <utility>

namespace tetris {
namespace {

QPixmap extendEdges(const QPixmap &snapshot, int padding) {
    const auto source = snapshot.toImage().convertToFormat(QImage::Format_RGB32);
    QImage extended(source.size() + QSize(padding * 2, padding * 2), QImage::Format_RGB32);
    for (int y = 0; y < extended.height(); ++y) {
        const auto *row = reinterpret_cast<const QRgb *>(source.constScanLine(std::clamp(y - padding, 0, source.height() - 1)));
        auto *destination = reinterpret_cast<QRgb *>(extended.scanLine(y));
        std::fill_n(destination, padding, row[0]);
        std::copy_n(row, source.width(), destination + padding);
        std::fill_n(destination + padding + source.width(), padding, row[source.width() - 1]);
    }
    return QPixmap::fromImage(extended);
}

}

ModalBackdrop::ModalBackdrop(QWidget *parent, QWidget *background)
    : QWidget(parent), background_(background) {
    setObjectName("modalBackdrop");
    setWindowFlags(Qt::Widget);
    setFocusPolicy(Qt::StrongFocus);
    tint_ = QColor(0, 0, 0, 105);
    if (parentWidget())
        parentWidget()->installEventFilter(this);
    hide();
}

void ModalBackdrop::setCard(QWidget *card) {
    if (card_ == card)
        return;
    if (card_)
        card_->hide();
    card_ = card;
    if (!card_)
        return;
    card_->setParent(this);
    card_->setObjectName("modalCard");
    centerCard();
    card_->setVisible(isVisible());
    focusCard();
}

void ModalBackdrop::setTint(const QColor &color) {
    tint_ = color;
    tint_.setAlpha(105);
    if (open_)
        captureBackground();
    update();
}

void ModalBackdrop::open() {
    if (open_) {
        if (parentWidget())
            setGeometry(parentWidget()->rect());
        raise();
        focusCard();
        return;
    }

    previousFocus_ = QApplication::focusWidget();
    backgroundWasEnabled_ = background_ && background_->isEnabled();
    if (parentWidget())
        setGeometry(parentWidget()->rect());
    captureBackground();
    open_ = true;
    if (background_)
        background_->setEnabled(false);
    if (qApp)
        qApp->installEventFilter(this);
    show();
    raise();
    centerCard();
    focusCard();
}

void ModalBackdrop::dismiss() {
    if (!open_ && !isVisible())
        return;
    hide();
    emit dismissed();
}

void ModalBackdrop::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.drawPixmap(rect(), snapshot_);
    painter.fillRect(rect(), tint_);
}

void ModalBackdrop::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    if (!open_) {
        previousFocus_ = QApplication::focusWidget();
        backgroundWasEnabled_ = background_ && background_->isEnabled();
        open_ = true;
        captureBackground();
        if (background_)
            background_->setEnabled(false);
        if (qApp)
            qApp->installEventFilter(this);
    }
    if (card_)
        card_->show();
    centerCard();
    focusCard();
}

void ModalBackdrop::hideEvent(QHideEvent *event) {
    QWidget::hideEvent(event);
    if (qApp)
        qApp->removeEventFilter(this);
    clearSnapshot();
    restoreBackgroundAndFocus();
}

void ModalBackdrop::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    centerCard();
    if (isVisible())
        captureBackground();
}

void ModalBackdrop::mousePressEvent(QMouseEvent *event) {
    if (card_ && !card_->geometry().contains(event->position().toPoint())) {
        dismiss();
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void ModalBackdrop::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Escape) {
        dismiss();
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

bool ModalBackdrop::eventFilter(QObject *watched, QEvent *event) {
    if (watched == parentWidget() && event->type() == QEvent::Resize && parentWidget())
        setGeometry(parentWidget()->rect());

    auto *widget = qobject_cast<QWidget *>(watched);
    if (open_ && widget && (widget == this || isAncestorOf(widget)) && event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->key() == Qt::Key_Escape) {
            dismiss();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void ModalBackdrop::captureBackground() {
    if (capturing_ || !background_ || background_->size().isEmpty())
        return;
    // Rendering can deliver pending resize events for the overlay.
    QScopedValueRollback<bool> captureGuard(capturing_, true);

    if (background_->layout())
        background_->layout()->activate();
    const bool wasEnabled = background_->isEnabled();
    if (open_ && backgroundWasEnabled_ && !wasEnabled)
        background_->setEnabled(true);

    const qreal devicePixelRatio = background_->devicePixelRatioF();
    QPixmap snapshot(background_->size() * devicePixelRatio);
    snapshot.setDevicePixelRatio(devicePixelRatio);
    snapshot.fill(tint_.isValid() ? QColor::fromRgb(tint_.rgb()) : Qt::transparent);
    QPainter painter(&snapshot);
    background_->render(&painter, QPoint(), QRegion(), QWidget::DrawChildren);
    painter.end();

    if (background_->isEnabled() != wasEnabled)
        background_->setEnabled(wasEnabled);
    storeBlurredSnapshot(snapshot);
}

void ModalBackdrop::storeBlurredSnapshot(const QPixmap &snapshot) {
    const auto logicalSize = snapshot.deviceIndependentSize().toSize();
    auto reducedSize = (logicalSize / 3).expandedTo(QSize(1, 1));
    if (reducedSize.width() > 640 || reducedSize.height() > 640)
        reducedSize.scale(640, 640, Qt::KeepAspectRatio);
    auto reduced = snapshot.scaled(reducedSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    reduced.setDevicePixelRatio(1);
    constexpr int padding = 32;
    QGraphicsScene scene;
    auto *image = scene.addPixmap(extendEdges(reduced, padding));
    auto *blur = new QGraphicsBlurEffect;
    blur->setBlurRadius(5);
    blur->setBlurHints(QGraphicsBlurEffect::QualityHint);
    image->setGraphicsEffect(blur);
    const QRectF bounds(QPointF(), reduced.size());
    const QRectF sourceBounds(QPointF(padding, padding), reduced.size());
    QPixmap blurred(reduced.size());
    blurred.fill(Qt::transparent);
    QPainter painter(&blurred);
    scene.render(&painter, bounds, sourceBounds);
    painter.end();
    snapshot_ = std::move(blurred);
    update();
}

void ModalBackdrop::clearSnapshot() {
    snapshot_ = QPixmap();
}

void ModalBackdrop::centerCard() {
    if (!card_)
        return;
    constexpr int margin = 24;
    const QSize available(qMax(1, width() - margin * 2), qMax(1, height() - margin * 2));
    QSize cardSize = card_->sizeHint();
    if (!cardSize.isValid() || cardSize.isEmpty())
        cardSize = card_->size();
    cardSize = cardSize.expandedTo(card_->minimumSizeHint()).boundedTo(available);
    card_->resize(cardSize);
    card_->move((width() - card_->width()) / 2, (height() - card_->height()) / 2);
}

void ModalBackdrop::focusCard() {
    if (!isVisible())
        return;
    if (card_) {
        const auto descendants = card_->findChildren<QWidget *>();
        for (auto *widget : descendants) {
            if (widget->isEnabled() && widget->isVisibleTo(card_) && widget->focusPolicy() != Qt::NoFocus) {
                widget->setFocus(Qt::OtherFocusReason);
                return;
            }
        }
        if (card_->isEnabled() && card_->focusPolicy() != Qt::NoFocus) {
            card_->setFocus(Qt::OtherFocusReason);
            return;
        }
    }
    setFocus(Qt::OtherFocusReason);
}

void ModalBackdrop::restoreBackgroundAndFocus() {
    open_ = false;
    if (background_ && background_->isEnabled() != backgroundWasEnabled_)
        background_->setEnabled(backgroundWasEnabled_);
    if (previousFocus_ && previousFocus_->isVisible() && previousFocus_->isEnabled())
        previousFocus_->setFocus(Qt::OtherFocusReason);
    previousFocus_.clear();
}

}
