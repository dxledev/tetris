#include "modal_backdrop.h"

#include "modal_motion.h"
#include "modal_surface.h"

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
#include <QTimer>
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
    setAttribute(Qt::WA_NoSystemBackground);
    tint_ = QColor(0, 0, 0, 105);
    if (ModalSurface::available()) {
        // Created before the window is first shown so it never swaps the compositor mid-session.
        surface_ = new ModalSurface(this);
        surface_->setGeometry(rect());
    }
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
    motion_ = new ModalMotion(card_);
    card_->setGraphicsEffect(motion_);
    connect(motion_, &ModalMotion::progressChanged, this, [this](qreal progress) {
        if (surface_)
            surface_->setProgress(progress);
        if (!usesSurface())
            update();
    });
    connect(motion_, &ModalMotion::finished, this, [this](bool open) {
        if (!open && closing_)
            finishDismiss();
    });
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
        if (closing_) {
            closing_ = false;
            if (card_)
                card_->setAttribute(Qt::WA_TransparentForMouseEvents, false);
            if (motion_)
                motion_->animate(true);
        }
        focusCard();
        return;
    }

    if (motion_)
        motion_->reset();
    beginOpen();
    show();
    raise();
    centerCard();
    focusCard();
    if (motion_)
        motion_->animate(true);
}

void ModalBackdrop::beginOpen() {
    previousFocus_ = QApplication::focusWidget();
    backgroundWasEnabled_ = background_ && background_->isEnabled();
    backgroundUpdatesWereEnabled_ = background_ && background_->updatesEnabled();
    if (parentWidget())
        setGeometry(parentWidget()->rect());
    captureBackground();
    open_ = true;
    if (background_)
        background_->setEnabled(false);
    freezeBackground();
    if (qApp)
        qApp->installEventFilter(this);
}

void ModalBackdrop::dismiss() {
    if (!open_ || closing_)
        return;
    if (!isVisible() || !motion_) {
        finishDismiss();
        return;
    }
    closing_ = true;
    if (card_)
        card_->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    motion_->animate(false);
}

void ModalBackdrop::finishDismiss() {
    closing_ = false;
    // Hand the page back while the opaque backdrop still covers it, so the
    // restored content is repainted in the same frame the backdrop disappears.
    restoreBackground();
    hide();
    emit dismissed();
}

void ModalBackdrop::freezeBackground() {
    if (!background_ || backgroundFrozen_)
        return;
    background_->setUpdatesEnabled(false);
    backgroundFrozen_ = true;
}

void ModalBackdrop::restoreBackground() {
    if (!background_)
        return;
    if (open_ && background_->isEnabled() != backgroundWasEnabled_)
        background_->setEnabled(backgroundWasEnabled_);
    if (backgroundFrozen_)
        background_->setUpdatesEnabled(backgroundUpdatesWereEnabled_);
    backgroundFrozen_ = false;
}

bool ModalBackdrop::usesSurface() const {
    return surface_ && surface_->isValid() && surface_->hasFrames();
}

void ModalBackdrop::paintEvent(QPaintEvent *) {
    if (usesSurface() || backgroundFrame_.isNull() || snapshot_.isNull())
        return;
    QPainter painter(this);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.drawPixmap(QPointF(), backgroundFrame_);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter.setOpacity(motion_ ? motion_->progress() : 1.0);
    painter.drawPixmap(QPointF(), snapshot_);
}

void ModalBackdrop::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    if (!open_) {
        beginOpen();
        if (motion_)
            motion_->animate(true);
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
    closing_ = false;
    if (card_)
        card_->setAttribute(Qt::WA_TransparentForMouseEvents, false);
    if (motion_)
        motion_->reset();
    restoreBackgroundAndFocus();
}

void ModalBackdrop::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    if (surface_)
        surface_->setGeometry(rect());
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
    if (surface_ && watched == parentWidget() && event->type() == QEvent::Show)
        QTimer::singleShot(0, this, [this] { surface_->prewarm(); });

    auto *widget = qobject_cast<QWidget *>(watched);
    if (closing_ && widget && (widget == this || isAncestorOf(widget))) {
        switch (event->type()) {
        case QEvent::ShortcutOverride:
            event->accept();
            return true;
        case QEvent::KeyPress:
        case QEvent::KeyRelease:
        case QEvent::Wheel:
            return true;
        default:
            break;
        }
    }
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

    // render() draws nothing while updates are disabled, so lift the freeze for the capture.
    const bool wasFrozen = backgroundFrozen_;
    if (wasFrozen)
        background_->setUpdatesEnabled(true);

    const qreal devicePixelRatio = background_->devicePixelRatioF();
    QPixmap snapshot(background_->size() * devicePixelRatio);
    snapshot.setDevicePixelRatio(devicePixelRatio);
    snapshot.fill(tint_.isValid() ? QColor::fromRgb(tint_.rgb()) : Qt::transparent);
    QPainter painter(&snapshot);
    background_->render(&painter, QPoint(), QRegion(), QWidget::DrawChildren);
    painter.end();

    if (background_->isEnabled() != wasEnabled)
        background_->setEnabled(wasEnabled);
    if (wasFrozen)
        background_->setUpdatesEnabled(false);
    backgroundFrame_ = snapshot;
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
    // Fold the tint into the overlay so it fades in with the blur instead of popping.
    const qreal devicePixelRatio = snapshot.devicePixelRatio();
    QPixmap overlay(snapshot.size());
    overlay.setDevicePixelRatio(devicePixelRatio);
    overlay.fill(Qt::transparent);
    QPainter overlayPainter(&overlay);
    overlayPainter.setRenderHint(QPainter::SmoothPixmapTransform);
    const QRect target(QPoint(), snapshot.deviceIndependentSize().toSize());
    overlayPainter.drawPixmap(target, blurred);
    overlayPainter.fillRect(target, tint_);
    overlayPainter.end();
    snapshot_ = std::move(overlay);
    if (surface_)
        surface_->setFrames(backgroundFrame_, snapshot_);
    update();
}

void ModalBackdrop::clearSnapshot() {
    snapshot_ = QPixmap();
    backgroundFrame_ = QPixmap();
    if (surface_)
        surface_->clearFrames();
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
    restoreBackground();
    open_ = false;
    if (previousFocus_ && previousFocus_->isVisible() && previousFocus_->isEnabled())
        previousFocus_->setFocus(Qt::OtherFocusReason);
    previousFocus_.clear();
}

}
