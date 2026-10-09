#include "piece_preview.h"

#include <QMouseEvent>
#include <QPainter>
#include <algorithm>
#include <array>

namespace tetris {

QColor pieceColor(const Theme &theme, PieceType piece) {
    static const std::array<QString, 8> keys{"board", "piece_i", "piece_o", "piece_t",
        "piece_s", "piece_z", "piece_j", "piece_l"};
    return theme.color(keys.at(static_cast<std::size_t>(piece)));
}

PiecePreview::PiecePreview(QWidget *parent) : QWidget(parent) {
    setMinimumHeight(44);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    setAccessibleName("Piece preview");
}

void PiecePreview::setPiece(PieceType piece) {
    if (piece_ == piece)
        return;
    piece_ = piece;
    update();
}

void PiecePreview::setTheme(const Theme &theme) {
    theme_ = theme;
    update();
}

void PiecePreview::setInteractive(bool interactive) {
    interactive_ = interactive;
    setCursor(interactive ? Qt::PointingHandCursor : Qt::ArrowCursor);
}

void PiecePreview::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    if (piece_ == PieceType::Empty) {
        painter.setPen(theme_.color("muted"));
        painter.drawText(rect(), Qt::AlignCenter, "—");
        return;
    }
    const auto cells = Game::pieceCells(piece_);
    int left = 4, top = 4, right = 0, bottom = 0;
    for (const auto &cell : cells) {
        left = std::min(left, cell.x);
        top = std::min(top, cell.y);
        right = std::max(right, cell.x);
        bottom = std::max(bottom, cell.y);
    }
    const int size = std::clamp(std::min(width() / 5, height() / 3), 12, 26);
    const QPointF origin((width() - (right - left + 1) * size) / 2.0,
        (height() - (bottom - top + 1) * size) / 2.0);
    painter.setPen(Qt::NoPen);
    painter.setBrush(pieceColor(theme_, piece_));
    for (const auto &cell : cells) {
        const QRectF block(origin.x() + (cell.x - left) * size + 1.5,
            origin.y() + (cell.y - top) * size + 1.5, size - 3, size - 3);
        painter.drawRoundedRect(block, 3, 3);
    }
}

void PiecePreview::mousePressEvent(QMouseEvent *event) {
    if (interactive_ && event->button() == Qt::LeftButton)
        emit activated();
    else
        QWidget::mousePressEvent(event);
}

}
