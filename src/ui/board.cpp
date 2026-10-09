#include "board.h"
#include "piece_preview.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <algorithm>

namespace tetris {

Board::Board(const Game *game, QWidget *parent) : QWidget(parent), game_(game) {
    setMinimumSize(160, 320);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setAccessibleName("Tetris playfield");
    setCursor(Qt::PointingHandCursor);
}

void Board::setTheme(const Theme &theme) {
    theme_ = theme;
    update();
}

void Board::setState(BoardState state) {
    if (state_ == state)
        return;
    state_ = state;
    setCursor(state == BoardState::Playing ? Qt::ArrowCursor : Qt::PointingHandCursor);
    update();
}

void Board::setGuides(bool ghost, bool grid) {
    showGhost_ = ghost;
    showGrid_ = grid;
    update();
}

QRectF Board::playfieldRect() const {
    const int cell = std::max(1, std::min((width() - 4) / Game::Width, (height() - 4) / Game::VisibleRows));
    const QSize size(cell * Game::Width, cell * Game::VisibleRows);
    return {(width() - size.width()) / 2.0, (height() - size.height()) / 2.0,
        static_cast<qreal>(size.width()), static_cast<qreal>(size.height())};
}

void Board::drawGrid(QPainter &painter, const QRectF &field) const {
    const auto size = field.width() / Game::Width;
    auto grid = theme_.color("grid");
    grid.setAlpha(42);
    painter.setPen(QPen(grid, 1));
    for (int column = 1; column < Game::Width; ++column)
        painter.drawLine(QPointF(field.left() + column * size, field.top()),
            QPointF(field.left() + column * size, field.bottom()));
    for (int row = 1; row < Game::VisibleRows; ++row)
        painter.drawLine(QPointF(field.left(), field.top() + row * size),
            QPointF(field.right(), field.top() + row * size));
}

void Board::drawCell(QPainter &painter, const QRectF &field, Point cell, QColor color, bool ghost) const {
    if (cell.y < Game::HiddenRows || cell.y >= Game::TotalRows)
        return;
    const auto size = field.width() / Game::Width;
    const QRectF block(field.left() + cell.x * size + 1.5,
        field.top() + (cell.y - Game::HiddenRows) * size + 1.5, size - 3, size - 3);
    if (ghost) {
        auto fill = theme_.color("ghost");
        fill.setAlpha(70);
        color.setAlpha(140);
        painter.setBrush(fill);
        painter.setPen(QPen(color, 1.2));
    } else {
        painter.setBrush(color);
        painter.setPen(Qt::NoPen);
    }
    painter.drawRoundedRect(block, std::min(4.0, size / 6), std::min(4.0, size / 6));
}

void Board::drawPieces(QPainter &painter, const QRectF &field) const {
    for (int row = Game::HiddenRows; row < Game::TotalRows; ++row) {
        for (int column = 0; column < Game::Width; ++column) {
            const auto piece = game_->board()[row][column];
            if (piece != PieceType::Empty)
                drawCell(painter, field, {column, row}, pieceColor(theme_, piece));
        }
    }
    const auto piece = game_->activePiece();
    if (piece == PieceType::Empty || game_->isGameOver())
        return;
    if (showGhost_)
        for (const auto &cell : game_->ghostCells())
            drawCell(painter, field, cell, pieceColor(theme_, piece), true);
    for (const auto &cell : game_->activeCells())
        drawCell(painter, field, cell, pieceColor(theme_, piece));
}

void Board::drawOverlay(QPainter &painter, const QRectF &field) const {
    if (state_ == BoardState::Playing)
        return;
    auto veil = theme_.color("board");
    veil.setAlpha(state_ == BoardState::Ready ? 248 : 225);
    painter.fillRect(field, veil);
    const auto center = field.center();
    const qreal size = std::clamp(field.width() / 12, 12.0, 22.0);
    painter.setBrush(theme_.color("accent"));
    painter.setPen(Qt::NoPen);
    for (const auto &cell : Game::pieceCells(PieceType::T))
        painter.drawRoundedRect(QRectF(center.x() + (cell.x - 1.5) * size,
            center.y() - 84 + cell.y * size, size - 3, size - 3), 3, 3);
    const QString title = state_ == BoardState::Ready ? "Ready to play" : state_ == BoardState::Paused ? "Paused"
        : state_ == BoardState::Complete ? "Well played" : "Game over";
    const QString subtitle = state_ == BoardState::Ready ? "Find your rhythm."
        : state_ == BoardState::Paused ? "Take a moment."
        : state_ == BoardState::Complete ? "40 lines. A clean finish." : "There’s always another round.";
    auto textFont = font();
    textFont.setPixelSize(field.width() < 230 ? 20 : 26);
    textFont.setWeight(QFont::DemiBold);
    painter.setFont(textFont);
    painter.setPen(theme_.color("text"));
    painter.drawText(QRectF(field.left(), center.y() - 8, field.width(), 36), Qt::AlignCenter, title);
    textFont.setPixelSize(field.width() < 230 ? 10 : 12);
    textFont.setWeight(QFont::Normal);
    painter.setFont(textFont);
    painter.setPen(theme_.color("muted"));
    painter.drawText(QRectF(field.left(), center.y() + 34, field.width(), 24), Qt::AlignCenter, subtitle);
    painter.setPen(theme_.color("accent"));
    const auto action = state_ == BoardState::Ready ? "Enter to begin" : state_ == BoardState::Paused ? "Enter to resume" : "Enter to play again";
    painter.drawText(QRectF(field.left(), center.y() + 82, field.width(), 24), Qt::AlignCenter, action);
}

void Board::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const auto field = playfieldRect();
    QPainterPath clip;
    clip.addRoundedRect(field, 8, 8);
    painter.setClipPath(clip);
    painter.fillRect(field, theme_.color("board"));
    if (showGrid_)
        drawGrid(painter, field);
    drawPieces(painter, field);
    drawOverlay(painter, field);
    painter.setClipping(false);
    painter.setPen(QPen(theme_.color("border"), 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(field, 8, 8);
}

void Board::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton && state_ != BoardState::Playing)
        emit activated();
    else
        QWidget::mousePressEvent(event);
}

}
