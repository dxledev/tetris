#include "window.h"
#include "piece_preview.h"
#include "preferences.h"
#include "stats.h"

#include <QApplication>
#include <QCloseEvent>
#include <QFontMetrics>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPainter>
#include <QProgressBar>
#include <QPushButton>
#include <QResizeEvent>
#include <QVBoxLayout>
#include <algorithm>
#include <random>

namespace tetris {
namespace {

QLabel *label(const QString &text, const QString &name = "muted") {
    auto *widget = new QLabel(text);
    widget->setObjectName(name);
    return widget;
}

QPushButton *button(const QString &text, const QString &name = "secondary") {
    auto *widget = new QPushButton(text);
    widget->setObjectName(name);
    widget->setFocusPolicy(Qt::NoFocus);
    widget->setCursor(Qt::PointingHandCursor);
    return widget;
}

QFrame *card() {
    auto *widget = new QFrame;
    widget->setObjectName("card");
    return widget;
}

QVBoxLayout *cardLayout(QWidget *widget) {
    auto *layout = new QVBoxLayout(widget);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(9);
    return layout;
}

QString formatTime(std::uint64_t milliseconds) {
    const auto seconds = milliseconds / 1000;
    return QString("%1:%2").arg(seconds / 60, 2, 10, QChar('0'))
        .arg(seconds % 60, 2, 10, QChar('0'));
}

QPixmap brandPixmap(const Theme &theme, qreal scale) {
    QPixmap result(qRound(38 * scale), qRound(38 * scale));
    result.setDevicePixelRatio(scale);
    result.fill(Qt::transparent);
    QPainter painter(&result);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(theme.color("accent"));
    for (const auto &cell : Game::pieceCells(PieceType::T))
        painter.drawRoundedRect(QRectF(cell.x * 12 + 1, cell.y * 12 + 8, 10, 10), 2.5, 2.5);
    return result;
}

void setText(QLabel *widget, const QString &text) {
    if (widget->text() != text)
        widget->setText(text);
}

}

Window::Window(QString configDirectory, Theme theme, Preferences preferences,
    std::optional<std::uint32_t> seed, QWidget *parent)
    : QMainWindow(parent), configDirectory_(std::move(configDirectory)),
      preferences_(std::move(preferences)), seed_(seed),
      game_(seed ? *seed : std::random_device{}(), gameSettings()),
      themeWatcher_(new ThemeWatcher(configDirectory_ + "/theme.json", std::move(theme), this)) {
    setWindowTitle("Tetris");
    setObjectName("tetrisWindow");
    setMinimumSize(640, 600);
    resize(preferences_.width, preferences_.height);
    setFont(QFont("Inter", 10));
    game_.pause();
    buildInterface();
    applyTheme();
    connect(themeWatcher_, &ThemeWatcher::themeChanged, this, [this] { applyTheme(); });
    connect(themeWatcher_, &ThemeWatcher::rejected, this, [this](const QString &error) {
        message("Colors could not be updated. Your last palette is still active.", true);
        themeName_->setAccessibleDescription(error);
    });
    ticker_.setTimerType(Qt::PreciseTimer);
    ticker_.setInterval(16);
    connect(&ticker_, &QTimer::timeout, this, &Window::tick);
    try {
        records_ = loadRecords(configDirectory_);
    } catch (const std::exception &) {
        message("Your saved records could not be read.", true);
    }
    refresh();
}

const Theme &Window::theme() const { return themeWatcher_->theme(); }
const Game &Window::game() const { return game_; }
const Board *Window::board() const { return board_; }

GameSettings Window::gameSettings() const {
    GameSettings settings;
    settings.gravityEnabled = preferences_.mode != "practice";
    settings.lockDelayMs = preferences_.lockDelayMs;
    settings.maxLockResets = preferences_.maxLockResets;
    settings.gravityInitialMs = preferences_.gravityInitialMs;
    settings.gravityStepMs = preferences_.gravityStepMs;
    settings.minGravityMs = preferences_.minimumGravityMs;
    settings.linesPerLevel = preferences_.linesPerLevel;
    return settings;
}

void Window::buildInterface() {
    auto *root = new QWidget;
    root->setObjectName("root");
    setCentralWidget(root);
    auto *rootLayout = new QVBoxLayout(root);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    auto *content = new QWidget;
    content->setObjectName("content");
    rootLayout->addWidget(content);
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(24, 20, 24, 18);
    layout->setSpacing(14);
    layout->addLayout(buildHeader());
    layout->addLayout(buildModeBar());
    auto *body = new QHBoxLayout;
    body->setSpacing(16);
    body->addWidget(buildLeftPanel());
    body->addWidget(buildBoardCard(), 1);
    body->addWidget(buildRightPanel());
    layout->addLayout(body, 1);
    auto *footer = new QHBoxLayout;
    status_ = label("Press Enter to begin, or choose a mode.", "status");
    status_->setMinimumHeight(20);
    footer->addWidget(status_, 1);
    themeName_ = label({}, "themeName");
    themeName_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    footer->addWidget(themeName_);
    layout->addLayout(footer);
    settingsModal_ = new SettingsModal(root, content);
    controlsModal_ = new ControlsModal(root, content);
    statsModal_ = new StatsModal(root, content);
    connect(settingsModal_, &SettingsModal::applied, this, [this](const Preferences &preferences) {
        preferences_ = preferences;
        game_.setSettings(gameSettings());
        board_->setGuides(preferences_.showGhost, preferences_.showGrid);
        saveSettings();
    });
    connect(settingsModal_, &SettingsModal::dismissed, this, &Window::finishModal);
    connect(controlsModal_, &ControlsModal::dismissed, this, &Window::finishModal);
    connect(statsModal_, &StatsModal::dismissed, this, &Window::finishModal);
}

QHBoxLayout *Window::buildHeader() {
    auto *layout = new QHBoxLayout;
    layout->setSpacing(12);
    brand_ = new QLabel;
    brand_->setFixedSize(38, 38);
    layout->addWidget(brand_);
    auto *title = new QVBoxLayout;
    title->setSpacing(1);
    title->addWidget(label("Tetris", "wordmark"));
    title->addWidget(label("Make room for the next piece.", "subtitle"));
    layout->addLayout(title);
    layout->addStretch();
    auto *controls = button("?", "controlsButton");
    controls->setFixedWidth(36);
    controls->setAccessibleName("Show controls");
    controls->setAccessibleDescription("Controls · F1");
    connect(controls, &QPushButton::clicked, this, &Window::openControls);
    layout->addWidget(controls);
    auto *stats = button("Stats", "statsButton");
    stats->setAccessibleDescription("Show your top ten Endless and Classic scores");
    connect(stats, &QPushButton::clicked, this, &Window::openStats);
    layout->addWidget(stats);
    auto *settings = button("Settings");
    settings->setObjectName("settingsButton");
    connect(settings, &QPushButton::clicked, this, &Window::editSettings);
    layout->addWidget(settings);
    auto *newButton = button("+  New game", "primary");
    newButton->setAccessibleDescription("Start a new game · Ctrl+N");
    connect(newButton, &QPushButton::clicked, this, &Window::newGame);
    layout->addWidget(newButton);
    return layout;
}

QHBoxLayout *Window::buildModeBar() {
    auto *layout = new QHBoxLayout;
    auto *hint = label("ONE MORE LINE", "eyebrow");
    layout->addWidget(hint);
    layout->addStretch();
    const QStringList modes{"endless", "classic", "practice"};
    for (int index = 0; index < modes.size(); ++index) {
        auto text = modes[index];
        text[0] = text[0].toUpper();
        auto *tab = button(text, "modeTab");
        tab->setCheckable(true);
        tab->setChecked(preferences_.mode == modes[index]);
        tab->setAccessibleDescription(index == 0 ? "Keep going as the pace increases"
            : index == 1 ? "Clear 40 lines as quickly as you can" : "Place pieces at your own pace");
        modeButtons_[index] = tab;
        connect(tab, &QPushButton::clicked, this, [this, mode = modes[index]] { setMode(mode); });
        layout->addWidget(tab);
    }
    return layout;
}

QWidget *Window::buildLeftPanel() {
    auto *panel = new QWidget;
    panel->setObjectName("leftPanel");
    panel->setFixedWidth(178);
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(14);
    auto *scoreCard = card();
    auto *stats = cardLayout(scoreCard);
    stats->addWidget(label("SCORE", "eyebrow"));
    score_ = label("0", "score");
    stats->addWidget(score_);
    best_ = label("Best  0", "best");
    stats->addWidget(best_);
    auto *numbers = new QGridLayout;
    numbers->setHorizontalSpacing(18);
    numbers->addWidget(label("LEVEL", "eyebrow"), 0, 0);
    numbers->addWidget(label("LINES", "eyebrow"), 0, 1);
    level_ = label("01", "number");
    lines_ = label("0", "number");
    numbers->addWidget(level_, 1, 0);
    numbers->addWidget(lines_, 1, 1);
    stats->addLayout(numbers);
    layout->addWidget(scoreCard);
    auto *holdCard = card();
    auto *holdLayout = cardLayout(holdCard);
    auto *holdHeader = new QHBoxLayout;
    holdHeader->addWidget(label("HOLD", "eyebrow"));
    holdHeader->addStretch();
    holdHeader->addWidget(label("C / Shift", "tiny"));
    holdLayout->addLayout(holdHeader);
    hold_ = new PiecePreview;
    hold_->setMinimumHeight(50);
    hold_->setInteractive(true);
    hold_->setAccessibleName("Hold or swap the current piece");
    connect(hold_, &PiecePreview::activated, this, [this] { if (game_.hold()) refresh(); });
    holdLayout->addWidget(hold_);
    layout->addWidget(holdCard);
    auto *goal = card();
    auto *goalLayout = cardLayout(goal);
    goalLayout->addWidget(label("YOUR PACE", "eyebrow"));
    progressText_ = label({}, "tiny");
    progressText_->setWordWrap(true);
    goalLayout->addWidget(progressText_);
    progress_ = new QProgressBar;
    progress_->setTextVisible(false);
    progress_->setFixedHeight(5);
    goalLayout->addWidget(progress_);
    layout->addWidget(goal);
    layout->addStretch();
    return panel;
}

QWidget *Window::buildRightPanel() {
    auto *panel = new QWidget;
    panel->setObjectName("rightPanel");
    panel->setFixedWidth(156);
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(14);
    auto *nextCard = card();
    auto *nextLayout = cardLayout(nextCard);
    nextLayout->addWidget(label("UP NEXT", "eyebrow"));
    for (int index = 0; index < Game::PreviewCount; ++index) {
        next_[index] = new PiecePreview;
        next_[index]->setMinimumHeight(index == 0 ? 50 : 34);
        next_[index]->setMaximumHeight(index == 0 ? 86 : 68);
        nextLayout->addWidget(next_[index], index == 0 ? 2 : 1);
    }
    layout->addWidget(nextCard, 1);
    auto *timeCard = card();
    auto *timeLayout = cardLayout(timeCard);
    timeLayout->addWidget(label("TIME", "eyebrow"));
    time_ = label("00:00", "number");
    timeLayout->addWidget(time_);
    layout->addWidget(timeCard);
    auto *hint = label("← →  Move\n↑ / X  Rotate\nSpace  Drop", "shortcut");
    hint->setWordWrap(true);
    layout->addWidget(hint);
    return panel;
}

QWidget *Window::buildBoardCard() {
    auto *panel = card();
    panel->setObjectName("boardCard");
    auto *layout = cardLayout(panel);
    auto *header = new QHBoxLayout;
    boardBadge_ = label(preferences_.mode.toUpper(), "badge");
    header->addWidget(boardBadge_);
    header->addStretch();
    pause_ = button("Start", "pauseButton");
    pause_->setMinimumWidth(64);
    connect(pause_, &QPushButton::clicked, this, &Window::togglePause);
    header->addWidget(pause_);
    layout->addLayout(header);
    board_ = new Board(&game_);
    board_->setGuides(preferences_.showGhost, preferences_.showGrid);
    connect(board_, &Board::activated, this, &Window::begin);
    layout->addWidget(board_, 1);
    auto *hint = label("10 across. One line at a time.", "tiny");
    hint->setAlignment(Qt::AlignCenter);
    layout->addWidget(hint);
    return panel;
}

void Window::applyTheme() {
    const auto &current = theme();
    auto palette = this->palette();
    palette.setColor(QPalette::Window, current.color("background"));
    palette.setColor(QPalette::WindowText, current.color("text"));
    palette.setColor(QPalette::Base, current.color("surface"));
    palette.setColor(QPalette::Text, current.color("text"));
    palette.setColor(QPalette::Button, current.color("surface_alt"));
    palette.setColor(QPalette::ButtonText, current.color("text"));
    palette.setColor(QPalette::Highlight, current.color("accent"));
    qApp->setPalette(palette);
    QString stylesheet = R"(
        QWidget { color: @text; font-size: 12px; }
        QWidget#root, QMainWindow { background: @background; }
        QLabel { background: transparent; border: none; }
        QFrame#card, QFrame#boardCard, QFrame#modalCard { background: @surface; border: 1px solid @border; border-radius: 12px; }
        QScrollArea#modalScroll, QWidget#modalBody { background: transparent; border: none; }
        QLabel#wordmark { color: @text; font-size: 25px; font-weight: 600; }
        QLabel#subtitle, QLabel#muted, QLabel#best, QLabel#status { color: @muted; }
        QLabel#eyebrow { color: @muted; font-size: 10px; font-weight: 600; }
        QLabel#heading { color: @text; font-size: 19px; font-weight: 600; }
        QLabel#score { color: @text; font-size: 34px; font-weight: 600; }
        QLabel#number { color: @text; font-size: 23px; font-weight: 500; }
        QLabel#tiny, QLabel#shortcut { color: @muted; font-size: 10px; }
        QLabel#themeName { color: @muted; font-size: 10px; }
        QLabel#badge { color: @accent; font-size: 10px; font-weight: 600; }
        QFrame#scoreRow { background: @surface_alt; border: none; border-radius: 7px; }
        QFrame#scoreLeaderRow { background: @selection; border: 1px solid @accent; border-radius: 7px; }
        QLabel[scoreRole="value"] { color: @text; font-size: 17px; font-weight: 500; }
        QLabel[scoreRole="leader"] { color: @accent; font-size: 19px; font-weight: 600; }
        QPushButton { background: @surface; border: 1px solid @border; border-radius: 7px; padding: 8px 12px; }
        QPushButton:hover { background: @surface_alt; border-color: @accent; }
        QPushButton:pressed { background: @selection; }
        QFrame#modalCard QPushButton:hover { background: @accent; color: @background; border-color: @accent; }
        QFrame#modalCard QPushButton:pressed { background: @selection; color: @text; border-color: @accent; }
        QPushButton#primary { background: @accent; color: @background; border-color: @accent; font-weight: 600; }
        QPushButton#primary:hover { background: @text; border-color: @text; }
        QPushButton#modeTab { border: none; background: transparent; color: @muted; padding: 8px 14px; }
        QPushButton#modeTab:checked { background: @selection; color: @accent; }
        QPushButton#pauseButton { border: none; background: @surface_alt; color: @text; font-size: 10px; padding: 6px 8px; }
        QProgressBar { background: @surface_alt; border: none; border-radius: 2px; }
        QProgressBar::chunk { background: @accent; border-radius: 2px; }
        QSpinBox { background: @surface; border: 1px solid @border; border-radius: 5px; padding: 6px; }
        QSpinBox::up-button, QSpinBox::down-button { background: @surface_alt; border: none; width: 18px; }
        QCheckBox { spacing: 10px; }
        QCheckBox::indicator { width: 16px; height: 16px; background: @surface; border: 1px solid @border; border-radius: 4px; }
        QCheckBox::indicator:checked { background: @accent; border-color: @accent; }
        QScrollBar:vertical { background: @surface; width: 8px; margin: 0; border: none; }
        QScrollBar::handle:vertical { background: @border; min-height: 20px; border-radius: 4px; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; background: transparent; }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }
    )";
    auto keys = colorKeys();
    std::sort(keys.begin(), keys.end(), [](const QString &left, const QString &right) { return left.size() > right.size(); });
    for (const auto &key : keys)
        stylesheet.replace("@" + key, current.hex(key));
    setStyleSheet(stylesheet);
    board_->setTheme(current);
    hold_->setTheme(current);
    for (auto *preview : next_)
        preview->setTheme(current);
    brand_->setPixmap(brandPixmap(current, devicePixelRatioF()));
    themeName_->setText("●  " + QFontMetrics(themeName_->font()).elidedText(current.name, Qt::ElideRight, 130));
    themeName_->setAccessibleDescription("Palette: " + current.name + "\n" + themeWatcher_->path());
    settingsModal_->setTint(current.color("background"));
    controlsModal_->setTint(current.color("background"));
    statsModal_->setTint(current.color("background"));
}

void Window::refresh() {
    setText(score_, QString::number(game_.score()));
    const auto best = preferences_.mode == "classic" ? records_.classicScore : records_.endlessScore;
    setText(best_, preferences_.mode == "practice" ? "Just practice."
        : preferences_.mode == "classic" ? "Best time  " + (records_.classicTimeMs == 0 ? QString("—") : formatTime(records_.classicTimeMs))
        : "Best  " + QString::number(std::max(best, game_.score())));
    setText(level_, QString("%1").arg(game_.level(), 2, 10, QChar('0')));
    setText(lines_, QString::number(game_.lines()));
    setText(time_, formatTime(game_.elapsedMilliseconds()));
    hold_->setPiece(game_.heldPiece());
    const auto &pieces = game_.nextPieces();
    for (int index = 0; index < Game::PreviewCount; ++index)
        next_[index]->setPiece(pieces.at(index));
    progress_->setRange(0, preferences_.mode == "classic" ? 40 : preferences_.linesPerLevel);
    progress_->setValue(preferences_.mode == "classic" ? std::min(40, game_.lines()) : game_.lines() % preferences_.linesPerLevel);
    setText(progressText_, preferences_.mode == "classic" ? QString("%1 / 40 lines").arg(game_.lines())
        : preferences_.mode == "practice" ? "Place each piece in your own time."
        : QString("%1 lines to level %2").arg(preferences_.linesPerLevel - game_.lines() % preferences_.linesPerLevel).arg(game_.level() + 1));
    progress_->setVisible(preferences_.mode != "practice");
    const auto state = !started_ ? BoardState::Ready : completed_ ? BoardState::Complete
        : game_.isGameOver() ? BoardState::GameOver : game_.isPaused() ? BoardState::Paused : BoardState::Playing;
    board_->setState(state);
    board_->update();
    pause_->setText(state == BoardState::Ready ? "Start" : state == BoardState::Playing ? "Pause"
        : state == BoardState::Paused ? "Resume" : "Again");
}

void Window::tick() {
    const auto cells = game_.activeCells();
    const auto score = game_.score();
    const auto seconds = game_.elapsedMilliseconds() / 1000;
    game_.advance(static_cast<std::uint64_t>(std::max<qint64>(0, elapsed_.restart())));
    repeatInput(static_cast<qint64>(game_.elapsedMilliseconds()));
    if (preferences_.mode == "classic" && game_.lines() >= 40) {
        completed_ = true;
        pauseGame();
        recordBest();
        message("40 lines cleared in " + formatTime(game_.elapsedMilliseconds()) + ". Well played.");
    } else if (game_.isGameOver()) {
        ticker_.stop();
        resetInput();
        recordBest();
        message("Game over. Press Enter for another round.");
    }
    if (cells != game_.activeCells() || score != game_.score() || seconds != game_.elapsedMilliseconds() / 1000
        || game_.isGameOver() || completed_)
        refresh();
}

void Window::repeatInput(qint64 now) {
    if (direction_ != 0 && now >= horizontalDue_) {
        direction_ < 0 ? game_.moveLeft() : game_.moveRight();
        horizontalDue_ = now + preferences_.moveRepeatMs;
    }
    if (pressedKeys_.contains(Qt::Key_Down) && now >= dropDue_) {
        game_.softDrop();
        dropDue_ = now + preferences_.softDropRepeatMs;
    }
}

void Window::begin() {
    if (modalOpen_)
        return;
    if (completed_ || game_.isGameOver()) {
        newGame();
        return;
    }
    started_ = true;
    game_.resume();
    elapsed_.start();
    ticker_.start();
    refresh();
    message(preferences_.mode == "practice" ? "Practice at your own pace. Space places a piece."
        : "↑ / X rotate  ·  Space drop  ·  C hold");
}

void Window::resetGame() {
    ticker_.stop();
    resetInput();
    completed_ = false;
    recorded_ = false;
    game_.setSettings(gameSettings());
    game_.startNewGame(seed_ ? *seed_ : std::random_device{}());
    begin();
}

void Window::newGame() {
    if (modalOpen_)
        return;
    recordBest();
    resetGame();
}

void Window::setMode(const QString &mode) {
    if (modalOpen_)
        return;
    if (mode != preferences_.mode) {
        recordBest();
        preferences_.mode = mode;
        boardBadge_->setText(mode.toUpper());
        resetGame();
        saveSettings();
    }
    const QStringList modes{"endless", "classic", "practice"};
    for (int index = 0; index < modes.size(); ++index)
        modeButtons_[index]->setChecked(mode == modes[index]);
}

void Window::resetInput() {
    pressedKeys_.clear();
    direction_ = 0;
    horizontalDue_ = 0;
    dropDue_ = 0;
}

void Window::pauseGame() {
    if (!started_ || game_.isPaused())
        return;
    game_.pause();
    ticker_.stop();
    resetInput();
    refresh();
}

void Window::togglePause() {
    if (!started_ || game_.isPaused() || game_.isGameOver() || completed_)
        begin();
    else {
        pauseGame();
        message("Paused. Press Enter when you’re ready.");
    }
}

void Window::editSettings() {
    if (modalOpen_)
        return;
    prepareModal();
    settingsModal_->open(preferences_);
}

void Window::openControls() {
    if (modalOpen_)
        return;
    prepareModal();
    controlsModal_->open();
}

void Window::openStats() {
    if (modalOpen_)
        return;
    prepareModal();
    statsModal_->open(records_);
}

void Window::prepareModal() {
    resumeAfterModal_ = game_.isRunning();
    modalOpen_ = true;
    pauseGame();
}

void Window::finishModal() {
    modalOpen_ = false;
    const bool resume = resumeAfterModal_;
    resumeAfterModal_ = false;
    if (resume && (!preferences_.autoPause || isActiveWindow()))
        begin();
}

void Window::recordBest() {
    if (!started_ || recorded_)
        return;
    auto updated = records_;
    bool changed = recordScore(updated, preferences_.mode, game_.score());
    if (preferences_.mode == "classic" && completed_
        && (updated.classicTimeMs == 0 || game_.elapsedMilliseconds() < updated.classicTimeMs)) {
        updated.classicTimeMs = game_.elapsedMilliseconds();
        changed = true;
    }
    if (!changed) {
        recorded_ = true;
        return;
    }
    try {
        saveRecords(configDirectory_, updated);
        records_ = std::move(updated);
        recorded_ = true;
    } catch (const std::exception &) {
        message("Your record could not be saved.", true);
    }
}

void Window::saveSettings() {
    preferences_.width = std::clamp(width(), 640, 8192);
    preferences_.height = std::clamp(height(), 600, 8192);
    try {
        savePreferences(configDirectory_, preferences_);
    } catch (const std::exception &) {
        message("Your settings could not be saved.", true);
    }
}

void Window::message(const QString &text, bool error) {
    status_->setText(text);
    status_->setStyleSheet("color: " + theme().hex(error ? "danger" : "muted") + ";");
}

bool Window::handleAction(int key) {
    if (key == Qt::Key_Left || key == Qt::Key_Right) {
        direction_ = key == Qt::Key_Left ? -1 : 1;
        direction_ < 0 ? game_.moveLeft() : game_.moveRight();
        horizontalDue_ = game_.elapsedMilliseconds() + preferences_.moveDelayMs;
    } else if (key == Qt::Key_Down) {
        game_.softDrop();
        dropDue_ = game_.elapsedMilliseconds() + preferences_.softDropRepeatMs;
    } else if (key == Qt::Key_Up || key == Qt::Key_X) {
        game_.rotateClockwise();
    } else if (key == Qt::Key_Z) {
        game_.rotateCounterClockwise();
    } else if (key == Qt::Key_Space) {
        game_.hardDrop();
    } else if (key == Qt::Key_C || key == Qt::Key_Shift) {
        game_.hold();
    } else {
        return false;
    }
    return true;
}

void Window::keyPressEvent(QKeyEvent *event) {
    if (modalOpen_) {
        if (event->key() == Qt::Key_Escape) {
            if (settingsModal_->isVisible())
                settingsModal_->dismiss();
            else if (controlsModal_->isVisible())
                controlsModal_->dismiss();
            else if (statsModal_->isVisible())
                statsModal_->dismiss();
        }
        event->accept();
        return;
    }
    if (event->isAutoRepeat()) {
        event->accept();
        return;
    }
    const int key = event->key();
    if (key == Qt::Key_N && event->modifiers().testFlag(Qt::ControlModifier))
        newGame();
    else if (key == Qt::Key_F1)
        openControls();
    else if (key == Qt::Key_P || key == Qt::Key_Escape)
        togglePause();
    else if (key == Qt::Key_Return || key == Qt::Key_Enter)
        begin();
    else if (game_.isRunning() && event->modifiers() == Qt::NoModifier) {
        if (handleAction(key)) {
            pressedKeys_.insert(key);
            refresh();
        } else {
            QMainWindow::keyPressEvent(event);
            return;
        }
    } else if (game_.isRunning() && key == Qt::Key_Shift) {
        game_.hold();
        refresh();
    } else {
        QMainWindow::keyPressEvent(event);
        return;
    }
    event->accept();
}

void Window::keyReleaseEvent(QKeyEvent *event) {
    if (modalOpen_) {
        event->accept();
        return;
    }
    if (event->isAutoRepeat())
        return;
    pressedKeys_.remove(event->key());
    if (event->key() == Qt::Key_Left || event->key() == Qt::Key_Right) {
        direction_ = pressedKeys_.contains(Qt::Key_Left) ? -1 : pressedKeys_.contains(Qt::Key_Right) ? 1 : 0;
        horizontalDue_ = game_.elapsedMilliseconds() + preferences_.moveRepeatMs;
    }
    QMainWindow::keyReleaseEvent(event);
}

void Window::changeEvent(QEvent *event) {
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::ActivationChange && !isActiveWindow() && preferences_.autoPause && !modalOpen_ && game_.isRunning()) {
        pauseGame();
        message("Paused while you were away. Enter to resume.");
    }
}

void Window::resizeEvent(QResizeEvent *event) {
    QMainWindow::resizeEvent(event);
    const bool compact = width() < 760;
    if (auto *panel = findChild<QWidget *>("leftPanel"))
        panel->setFixedWidth(compact ? 140 : 178);
    if (auto *panel = findChild<QWidget *>("rightPanel"))
        panel->setFixedWidth(compact ? 126 : 156);
    if (auto *content = findChild<QWidget *>("content"))
        content->layout()->setContentsMargins(compact ? 16 : 24, 20, compact ? 16 : 24, 18);
}

void Window::closeEvent(QCloseEvent *event) {
    ticker_.stop();
    recordBest();
    saveSettings();
    event->accept();
}

}
