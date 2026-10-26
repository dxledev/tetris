#include "modal_surface.h"

#include <QGuiApplication>
#include <QOpenGLContext>
#include <QPainter>
#include <QSurfaceFormat>

namespace tetris {

bool ModalSurface::available() {
    const auto platform = QGuiApplication::platformName();
    if (qEnvironmentVariable("TETRIS_MODAL_RENDERER") == "software" || platform == "offscreen" || platform == "minimal")
        return false;
    if (platform.startsWith("wayland") && qEnvironmentVariable("QT_WAYLAND_CLIENT_BUFFER_INTEGRATION") == "shm")
        return false;
    QOpenGLContext probe;
    return probe.create();
}

ModalSurface::ModalSurface(QWidget *parent) : QOpenGLWidget(parent) {
    setObjectName("modalSurface");
    setFocusPolicy(Qt::NoFocus);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    auto format = QSurfaceFormat::defaultFormat();
    format.setAlphaBufferSize(8);
    format.setDepthBufferSize(0);
    format.setStencilBufferSize(0);
    format.setSamples(0);
    setFormat(format);
    setUpdateBehavior(QOpenGLWidget::NoPartialUpdate);
}

void ModalSurface::prewarm() {
    if (isValid())
        return;
    const auto previous = size();
    const auto savedBase = base_, savedOverlay = overlay_;
    const auto savedProgress = progress_;
    base_ = QPixmap(1, 1);
    base_.fill(Qt::black);
    overlay_ = QPixmap(1, 1);
    overlay_.fill(Qt::white);
    progress_ = .5;
    resize(1, 1);
    // Compile the textured opacity path before the first modal interaction.
    grabFramebuffer();
    base_ = savedBase;
    overlay_ = savedOverlay;
    progress_ = savedProgress;
    resize(previous);
}

void ModalSurface::setFrames(const QPixmap &base, const QPixmap &overlay) {
    base_ = base;
    overlay_ = overlay;
    update();
}

void ModalSurface::setProgress(qreal progress) {
    if (progress_ == progress)
        return;
    progress_ = progress;
    update();
}

void ModalSurface::clearFrames() {
    base_ = QPixmap();
    overlay_ = QPixmap();
}

void ModalSurface::paintGL() {
    QPainter painter(this);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    if (!hasFrames()) {
        painter.fillRect(rect(), Qt::transparent);
        return;
    }
    painter.drawPixmap(QPointF(), base_);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter.setOpacity(progress_);
    painter.drawPixmap(QPointF(), overlay_);
}

}
