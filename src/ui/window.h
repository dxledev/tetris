#pragma once

#include "board.h"
#include "core/storage.h"
#include "theme_watcher.h"

#include <QElapsedTimer>
#include <QMainWindow>
#include <QSet>
#include <QTimer>
#include <array>
#include <optional>

class QLabel;
class QPushButton;
class QProgressBar;
class QHBoxLayout;
class QFrame;

namespace tetris {

class PiecePreview;
class SettingsModal;
class ControlsModal;
class StatsModal;

class Window : public QMainWindow {
    Q_OBJECT
public:
    Window(QString configDirectory, Theme theme, Preferences preferences = {},
        std::optional<std::uint32_t> seed = {}, QWidget *parent = nullptr);
    const Theme &theme() const;
    const Game &game() const;
    const Board *board() const;

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void changeEvent(QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    QString configDirectory_;
    Preferences preferences_;
    Records records_;
    std::optional<std::uint32_t> seed_;
    Game game_;
    ThemeWatcher *themeWatcher_;
    Board *board_ = nullptr;
    PiecePreview *hold_ = nullptr;
    std::array<PiecePreview *, Game::PreviewCount> next_{};
    std::array<QPushButton *, 3> modeButtons_{};
    QLabel *brand_ = nullptr;
    QLabel *score_ = nullptr;
    QLabel *best_ = nullptr;
    QLabel *level_ = nullptr;
    QLabel *lines_ = nullptr;
    QLabel *time_ = nullptr;
    QLabel *progressText_ = nullptr;
    QLabel *status_ = nullptr;
    QLabel *themeName_ = nullptr;
    QLabel *boardBadge_ = nullptr;
    QProgressBar *progress_ = nullptr;
    QPushButton *pause_ = nullptr;
    SettingsModal *settingsModal_ = nullptr;
    ControlsModal *controlsModal_ = nullptr;
    StatsModal *statsModal_ = nullptr;
    QTimer ticker_;
    QElapsedTimer elapsed_;
    QSet<int> pressedKeys_;
    int direction_ = 0;
    qint64 horizontalDue_ = 0;
    qint64 dropDue_ = 0;
    bool started_ = false;
    bool completed_ = false;
    bool recorded_ = false;
    bool modalOpen_ = false;
    bool resumeAfterModal_ = false;

    void buildInterface();
    QHBoxLayout *buildHeader();
    QHBoxLayout *buildModeBar();
    QWidget *buildLeftPanel();
    QWidget *buildRightPanel();
    QWidget *buildBoardCard();
    void applyTheme();
    void refresh();
    void tick();
    void repeatInput(qint64 now);
    void begin();
    void newGame();
    void resetGame();
    void setMode(const QString &mode);
    void togglePause();
    void pauseGame();
    void resetInput();
    void editSettings();
    void openControls();
    void openStats();
    void prepareModal();
    void finishModal();
    void recordBest();
    void saveSettings();
    void message(const QString &text, bool error = false);
    bool handleAction(int key);
    GameSettings gameSettings() const;
};

}
