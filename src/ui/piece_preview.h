#pragma once

#include "core/game.h"
#include "core/theme.h"
#include <QWidget>

namespace tetris {

QColor pieceColor(const Theme &theme, PieceType piece);

class PiecePreview : public QWidget {
    Q_OBJECT
public:
    explicit PiecePreview(QWidget *parent = nullptr);
    void setPiece(PieceType piece);
    void setTheme(const Theme &theme);
    void setInteractive(bool interactive);

signals:
    void activated();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    PieceType piece_ = PieceType::Empty;
    Theme theme_ = preset("slate");
    bool interactive_ = false;
};

}
