#pragma once

#include "core/storage.h"
#include "modal_backdrop.h"

#include <QLabel>
#include <QVector>

class QHBoxLayout;

namespace tetris {

class StatsModal : public ModalBackdrop {
    Q_OBJECT
public:
    StatsModal(QWidget *parent, QWidget *background);
    void open(const Records &records);

private:
    QVector<QLabel *> endlessScores_;
    QVector<QLabel *> classicScores_;
    QLabel *endlessEmpty_ = nullptr;
    QLabel *classicEmpty_ = nullptr;

    QWidget *buildCard();
    QHBoxLayout *buildHeader();
    QWidget *buildScoreSection(const QString &title, const QString &prefix,
                               QVector<QLabel *> &scores, QLabel **emptyState);
    void updateScores(const std::vector<std::uint64_t> &scores, const QVector<QLabel *> &labels,
                      QLabel *emptyState);
};

}
