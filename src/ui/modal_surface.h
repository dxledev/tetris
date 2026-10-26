#pragma once

#include <QOpenGLWidget>
#include <QPixmap>

namespace tetris {

// GPU-composited backdrop: draws the unblurred background, then crossfades the
// blurred overlay over it, so the window never shows an empty frame.
class ModalSurface : public QOpenGLWidget {
    Q_OBJECT
public:
    explicit ModalSurface(QWidget *parent);

    static bool available();
    void prewarm();
    void setFrames(const QPixmap &base, const QPixmap &overlay);
    void setProgress(qreal progress);
    void clearFrames();
    bool hasFrames() const { return !base_.isNull() && !overlay_.isNull(); }

protected:
    void paintGL() override;

private:
    QPixmap base_, overlay_;
    qreal progress_ = 0;
};

}
