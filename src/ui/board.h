#pragma once

#include "core/game.h"
#include "core/theme.h"
#include <QRectF>
#include <QWidget>

class QPainter;

namespace tetris {

enum class BoardState { Ready, Playing, Paused, GameOver, Complete };

class Board : public QWidget {
    Q_OBJECT
public:
    explicit Board(const Game *game, QWidget *parent = nullptr);
    void setTheme(const Theme &theme);
    void setState(BoardState state);
    void setGuides(bool ghost, bool grid);
    QRectF playfieldRect() const;

signals:
    void activated();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    const Game *game_;
    Theme theme_ = preset("slate");
    BoardState state_ = BoardState::Ready;
    bool showGhost_ = true;
    bool showGrid_ = true;

    void drawGrid(QPainter &painter, const QRectF &field) const;
    void drawCell(QPainter &painter, const QRectF &field, Point cell, QColor color, bool ghost = false) const;
    void drawPieces(QPainter &painter, const QRectF &field) const;
    void drawOverlay(QPainter &painter, const QRectF &field) const;
};

}
