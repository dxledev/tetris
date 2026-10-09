#include "stats.h"

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLocale>
#include <QPushButton>
#include <QShortcut>
#include <QVBoxLayout>

namespace tetris {
namespace {

QLabel *makeLabel(const QString &text, const QString &name) {
    auto *label = new QLabel(text);
    label->setObjectName(name);
    return label;
}

}

StatsModal::StatsModal(QWidget *parent, QWidget *background) : ModalBackdrop(parent, background) {
    setObjectName("statsModal");
    setAccessibleName("Tetris statistics");
    setCard(buildCard());

    for (auto key : {Qt::Key_Return, Qt::Key_Enter}) {
        auto *shortcut = new QShortcut(QKeySequence(key), this);
        shortcut->setContext(Qt::WidgetWithChildrenShortcut);
        connect(shortcut, &QShortcut::activated, this, &StatsModal::dismiss);
    }
}

QWidget *StatsModal::buildCard() {
    auto *card = new QFrame(this);
    card->setMinimumWidth(480);
    card->setMaximumWidth(560);

    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(18);
    layout->addLayout(buildHeader());

    auto *sections = new QHBoxLayout;
    sections->setContentsMargins(0, 0, 0, 0);
    sections->setSpacing(16);
    sections->addWidget(buildScoreSection("Endless", "endless", endlessScores_, &endlessEmpty_));
    sections->addWidget(buildScoreSection("Classic", "classic", classicScores_, &classicEmpty_));
    layout->addLayout(sections);
    return card;
}

QHBoxLayout *StatsModal::buildHeader() {
    auto *layout = new QHBoxLayout;
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);
    auto *heading = makeLabel("High Scores", "heading");
    heading->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    layout->addWidget(heading);
    layout->addStretch();
    auto *close = new QPushButton("Close");
    close->setObjectName("statsCloseButton");
    close->setCursor(Qt::PointingHandCursor);
    connect(close, &QPushButton::clicked, this, &StatsModal::dismiss);
    layout->addWidget(close);
    return layout;
}

QWidget *StatsModal::buildScoreSection(const QString &title, const QString &prefix,
                                       QVector<QLabel *> &scores, QLabel **emptyState) {
    auto *section = new QFrame;
    auto *layout = new QVBoxLayout(section);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);
    layout->addWidget(makeLabel(title, "heading"));

    auto *empty = makeLabel("No scores yet.", "muted");
    *emptyState = empty;
    layout->addWidget(empty);

    auto *rows = new QVBoxLayout;
    rows->setContentsMargins(0, 0, 0, 0);
    rows->setSpacing(4);
    for (int index = 0; index < static_cast<int>(LeaderboardSize); ++index) {
        const bool leader = index == 0;
        auto *row = new QFrame;
        row->setObjectName(leader ? "scoreLeaderRow" : "scoreRow");
        auto *rowLayout = new QGridLayout(row);
        rowLayout->setContentsMargins(8, 5, 8, 5);
        auto *rank = makeLabel(QString::number(index + 1), "muted");
        auto *value = makeLabel("—", prefix + "Score" + QString::number(index + 1));
        value->setProperty("scoreRole", leader ? "leader" : "value");
        value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        rowLayout->addWidget(rank, 0, 0);
        rowLayout->addWidget(value, 0, 1);
        rowLayout->setColumnStretch(1, 1);
        rows->addWidget(row);
        scores.push_back(value);
    }
    layout->addLayout(rows);
    return section;
}

void StatsModal::open(const Records &records) {
    updateScores(records.endlessScores, endlessScores_, endlessEmpty_);
    updateScores(records.classicScores, classicScores_, classicEmpty_);
    ModalBackdrop::open();
}

void StatsModal::updateScores(const std::vector<std::uint64_t> &scores, const QVector<QLabel *> &labels,
                              QLabel *emptyState) {
    emptyState->setVisible(scores.empty());
    for (int index = 0; index < labels.size(); ++index) {
        const QString text = index < static_cast<int>(scores.size())
            ? QLocale().toString(static_cast<qulonglong>(scores[static_cast<std::size_t>(index)]))
            : QStringLiteral("—");
        labels[index]->setText(text);
    }
}

}
