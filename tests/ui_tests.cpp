#include "core/storage.h"
#include "core/theme.h"
#include "ui/window.h"
#include "ui/preferences.h"
#include "ui/stats.h"

#include <QApplication>
#include <QCheckBox>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QLayout>
#include <QLocale>
#include <QProcess>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QtTest>

using namespace tetris;

namespace {

QString executable;

void capture(Window &window, const QString &name) {
    const auto directory = qEnvironmentVariable("TETRIS_TEST_CAPTURE_DIR");
    if (!directory.isEmpty()) {
        QDir().mkpath(directory);
        window.grab().save(directory + "/" + name + ".png");
    }
}

Preferences testPreferences() {
    Preferences result;
    result.autoPause = false;
    return result;
}

}

class UiTests : public QObject {
    Q_OBJECT
private slots:
    void playPauseAndResize();
    void cliColorsReloadImmediately();
    void practiceDisablesAutomaticFalling();
    void embeddedModalsBlockGameplay();
    void modalSaveCancelResizeAndLiveTheme();
    void statsRankScoresPauseAndLiveTheme();
    void statsRecordEachGameOnce();
};

void UiTests::playPauseAndResize() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    Window window(temporary.path(), ensureTheme(temporary.filePath("theme.json")), testPreferences(), 42);
    window.show();
    QTest::qWait(80);
    QCOMPARE(window.windowTitle(), QString("Tetris"));
    QVERIFY(window.game().isPaused());
    capture(window, "ready");
    QTest::keyClick(&window, Qt::Key_Return);
    QVERIFY(window.game().isRunning());
    const auto cells = window.game().activeCells();
    QTest::keyClick(&window, Qt::Key_Left);
    QCOMPARE(window.game().activeCells().front().x, cells.front().x - 1);
    QTest::keyClick(&window, Qt::Key_C);
    QVERIFY(window.game().heldPiece() != PieceType::Empty);
    QTest::keyClick(&window, Qt::Key_Space);
    QVERIFY(window.game().score() > 0);
    for (int index = 0; index < 7; ++index) {
        for (int move = 0; move < (index % 3 == 0 ? 3 : 2); ++move)
            QTest::keyClick(&window, index % 2 == 0 ? Qt::Key_Left : Qt::Key_Right);
        if (index % 2 != 0)
            QTest::keyClick(&window, Qt::Key_Up);
        QTest::keyClick(&window, Qt::Key_Space);
    }
    capture(window, "playing");
    QTest::keyClick(&window, Qt::Key_P);
    QVERIFY(window.game().isPaused());
    const auto elapsed = window.game().elapsedMilliseconds();
    QTest::qWait(100);
    QCOMPARE(window.game().elapsedMilliseconds(), elapsed);
    QTest::keyClick(&window, Qt::Key_Return);
    QVERIFY(window.game().isRunning());
    window.resize(640, 600);
    QTest::qWait(60);
    const auto field = window.board()->playfieldRect();
    QCOMPARE(field.height(), field.width() * 2);
    QVERIFY(field.width() >= 140);
    capture(window, "compact");
    window.close();
    QVERIFY(QFileInfo::exists(temporary.filePath("settings.json")));
    QVERIFY(loadRecords(temporary.path()).endlessScore > 0);
}

void UiTests::cliColorsReloadImmediately() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    Window window(temporary.path(), ensureTheme(temporary.filePath("theme.json")), testPreferences(), 42);
    window.show();
    QTest::qWait(80);
    QTest::keyClick(&window, Qt::Key_Return);
    QProcess process;
    process.start(executable, {"--config-dir", temporary.path(), "theme", "set", "accent=#ff1234", "piece_i=#ee7722"});
    QVERIFY(process.waitForFinished(5000));
    QCOMPARE(process.exitCode(), 0);
    QTRY_COMPARE_WITH_TIMEOUT(window.theme().color("accent"), QColor("#ff1234"), 2000);
    QCOMPARE(window.theme().color("piece_i"), QColor("#ee7722"));
    QCOMPARE(loadTheme(temporary.filePath("theme.json")), window.theme());
    QVERIFY(window.game().isRunning());
    QVERIFY(!window.styleSheet().contains("@"));
    QVERIFY(!window.styleSheet().contains("_alt"));
    saveTheme(temporary.filePath("theme.json"), preset("paper"));
    QTRY_COMPARE_WITH_TIMEOUT(window.theme().name, QString("paper"), 2000);
    capture(window, "paper");
    QFile malformed(temporary.filePath("theme.json"));
    QVERIFY(malformed.open(QIODevice::WriteOnly | QIODevice::Truncate));
    malformed.write("broken JSON");
    malformed.close();
    QTest::qWait(100);
    QCOMPARE(window.theme().name, QString("paper"));
    window.close();
}

void UiTests::practiceDisablesAutomaticFalling() {
    QTemporaryDir temporary;
    auto preferences = testPreferences();
    preferences.mode = "practice";
    Window window(temporary.path(), ensureTheme(temporary.filePath("theme.json")), preferences, 11);
    window.show();
    QTest::keyClick(&window, Qt::Key_Return);
    const auto cells = window.game().activeCells();
    QTest::qWait(1100);
    QCOMPARE(window.game().activeCells(), cells);
    QTest::keyClick(&window, Qt::Key_Space);
    QVERIFY(window.game().score() > 0);
    QTest::keyClick(&window, Qt::Key_N, Qt::ControlModifier);
    QCOMPARE(window.game().score(), std::uint64_t(0));
    window.close();
    QCOMPARE(loadPreferences(temporary.path()).mode, QString("practice"));
    QCOMPARE(loadRecords(temporary.path()).endlessScore, std::uint64_t(0));
}

void UiTests::embeddedModalsBlockGameplay() {
    QTemporaryDir temporary;
    Window window(temporary.path(), ensureTheme(temporary.filePath("theme.json")), testPreferences(), 42);
    window.show();
    QTest::qWait(80);
    auto *settings = window.findChild<SettingsModal *>("settingsModal");
    auto *controls = window.findChild<ControlsModal *>("controlsModal");
    auto *content = window.findChild<QWidget *>("content");
    auto *settingsButton = window.findChild<QPushButton *>("settingsButton");
    QVERIFY(settings && controls && content && settingsButton);
    QVERIFY(!settings->isVisible());
    QVERIFY(!controls->isVisible());
    QTest::keyClick(&window, Qt::Key_F1);
    QVERIFY(controls->isVisible());
    QVERIFY(!controls->isWindow());
    QCOMPARE(controls->window(), &window);
    QVERIFY(!content->isEnabled());
    capture(window, "controls-modal");
    QTest::mouseClick(controls->findChild<QPushButton *>("controlsCloseButton"), Qt::LeftButton);
    QVERIFY(!controls->isVisible());
    QVERIFY(content->isEnabled());
    QVERIFY(window.game().isPaused());
    QTest::keyClick(&window, Qt::Key_Return);
    QTest::keyClick(&window, Qt::Key_Space);
    const auto score = window.game().score();
    QTest::mouseClick(settingsButton, Qt::LeftButton);
    QVERIFY(settings->isVisible());
    QVERIFY(!settings->isWindow());
    QCOMPARE(settings->window(), &window);
    QVERIFY(window.game().isPaused());
    QVERIFY(!content->isEnabled());
    const auto elapsed = window.game().elapsedMilliseconds();
    QTest::keyClick(&window, Qt::Key_Space);
    QTest::keyClick(&window, Qt::Key_N, Qt::ControlModifier);
    QTest::qWait(100);
    QCOMPARE(window.game().score(), score);
    QCOMPARE(window.game().elapsedMilliseconds(), elapsed);
    for (int index = 0; index < 16; ++index) {
        auto *focus = QApplication::focusWidget();
        QVERIFY(focus && (focus == settings || settings->isAncestorOf(focus)));
        QTest::keyClick(focus, Qt::Key_Tab);
    }
    for (auto *widget : QApplication::topLevelWidgets())
        if (widget->isVisible())
            QCOMPARE(widget, &window);
    QVERIFY(!QApplication::activeModalWidget());
    QVERIFY(!QApplication::activePopupWidget());
    capture(window, "settings-modal");
    QTest::keyClick(settings->findChild<QSpinBox *>("prefsMoveDelaySpin"), Qt::Key_Escape);
    QVERIFY(!settings->isVisible());
    QVERIFY(content->isEnabled());
    QVERIFY(window.game().isRunning());
    QTest::keyClick(&window, Qt::Key_P);
    QTest::keyClick(&window, Qt::Key_F1);
    QVERIFY(controls->isVisible());
    QTest::mouseClick(controls, Qt::LeftButton, Qt::NoModifier, QPoint(5, 5));
    QVERIFY(!controls->isVisible());
    QVERIFY(window.game().isPaused());
    window.close();
}

void UiTests::modalSaveCancelResizeAndLiveTheme() {
    QTemporaryDir temporary;
    Window window(temporary.path(), ensureTheme(temporary.filePath("theme.json")), testPreferences(), 42);
    window.show();
    QTest::qWait(80);
    auto *settings = window.findChild<SettingsModal *>("settingsModal");
    auto *button = window.findChild<QPushButton *>("settingsButton");
    QVERIFY(settings && button);
    QTest::mouseClick(button, Qt::LeftButton);
    auto *delay = settings->findChild<QSpinBox *>("prefsMoveDelaySpin");
    auto *ghost = settings->findChild<QCheckBox *>("prefsGhostToggle");
    QVERIFY(delay && ghost);
    delay->setValue(345);
    ghost->setChecked(false);
    QTest::mouseClick(settings->findChild<QPushButton *>("modalSaveButton"), Qt::LeftButton);
    QVERIFY(!settings->isVisible());
    QCOMPARE(loadPreferences(temporary.path()).moveDelayMs, 345);
    QVERIFY(!loadPreferences(temporary.path()).showGhost);
    QVERIFY(window.game().isPaused());
    QTest::mouseClick(button, Qt::LeftButton);
    QCOMPARE(delay->value(), 345);
    QVERIFY(!ghost->isChecked());
    delay->setValue(600);
    QTest::mouseClick(settings->findChild<QPushButton *>("modalCancelButton"), Qt::LeftButton);
    QCOMPARE(loadPreferences(temporary.path()).moveDelayMs, 345);
    QTest::mouseClick(button, Qt::LeftButton);
    window.resize(640, 600);
    QTest::qWait(80);
    auto *card = settings->findChild<QWidget *>("modalCard");
    QVERIFY(card);
    QCOMPARE(settings->geometry(), window.centralWidget()->rect());
    QVERIFY(settings->rect().contains(card->geometry()));
    QCOMPARE(card->geometry().center().x(), settings->rect().center().x());
    QCOMPARE(card->geometry().center().y(), settings->rect().center().y());
    auto *scroll = settings->findChild<QScrollArea *>("modalScroll");
    auto *lockDelay = settings->findChild<QSpinBox *>("prefsLockDelaySpin");
    QVERIFY(scroll && lockDelay);
    QVERIFY(scroll->viewport()->rect().contains(QRect(lockDelay->mapTo(scroll->viewport(), QPoint()), lockDelay->size())));
    capture(window, "settings-modal-compact");
    saveTheme(temporary.filePath("theme.json"), preset("paper"));
    QTRY_COMPARE_WITH_TIMEOUT(window.theme().name, QString("paper"), 2000);
    QVERIFY(settings->isVisible());
    QVERIFY(!window.findChild<QWidget *>("content")->isEnabled());
    capture(window, "settings-modal-paper");
    delay->setFocus();
    QTest::mouseClick(delay, Qt::RightButton);
    QVERIFY(!QApplication::activePopupWidget());
    QTest::keyClick(delay, Qt::Key_Return);
    QVERIFY(!settings->isVisible());
    QVERIFY(window.findChild<QWidget *>("content")->isEnabled());
    window.close();
}

void UiTests::statsRankScoresPauseAndLiveTheme() {
    QTemporaryDir temporary;
    Records records;
    records.endlessScores = {9000, 8000, 7000, 6000, 5000, 4000, 3000, 2000, 1000, 500};
    records.classicScores = {4500, 4000, 3500, 3000, 2500, 2000, 1500, 1000, 750, 500};
    saveRecords(temporary.path(), records);
    Window window(temporary.path(), ensureTheme(temporary.filePath("theme.json")), testPreferences(), 42);
    window.show();
    QTest::qWait(80);
    auto *stats = window.findChild<StatsModal *>("statsModal");
    auto *statsButton = window.findChild<QPushButton *>("statsButton");
    auto *content = window.findChild<QWidget *>("content");
    QVERIFY(stats && statsButton && content);
    QVERIFY(!stats->isVisible());
    QTest::keyClick(&window, Qt::Key_Return);
    QTest::keyClick(&window, Qt::Key_Space);
    const auto score = window.game().score();
    QTest::mouseClick(statsButton, Qt::LeftButton);
    QVERIFY(stats->isVisible());
    QVERIFY(!stats->isWindow());
    QCOMPARE(stats->window(), &window);
    QVERIFY(window.game().isPaused());
    QVERIFY(!content->isEnabled());
    for (int index = 0; index < 10; ++index) {
        for (const auto &mode : {QString("endless"), QString("classic")}) {
            auto *value = stats->findChild<QLabel *>(mode + "Score" + QString::number(index + 1));
            QVERIFY(value && value->isVisible());
            const auto &scores = mode == "endless" ? records.endlessScores : records.classicScores;
            QCOMPARE(value->text(), QLocale().toString(static_cast<qulonglong>(scores[index])));
            QCOMPARE(value->property("scoreRole").toString(), index == 0 ? QString("leader") : QString("value"));
        }
    }
    capture(window, "stats-modal");
    const auto elapsed = window.game().elapsedMilliseconds();
    QTest::keyClick(&window, Qt::Key_Space);
    QTest::keyClick(&window, Qt::Key_N, Qt::ControlModifier);
    QTest::qWait(100);
    QCOMPARE(window.game().score(), score);
    QCOMPARE(window.game().elapsedMilliseconds(), elapsed);
    for (int index = 0; index < 8; ++index) {
        auto *focus = QApplication::focusWidget();
        QVERIFY(focus && (focus == stats || stats->isAncestorOf(focus)));
        QTest::keyClick(focus, Qt::Key_Tab);
    }
    for (auto *widget : QApplication::topLevelWidgets())
        if (widget->isVisible())
            QCOMPARE(widget, &window);
    QVERIFY(!QApplication::activeModalWidget());
    QVERIFY(!QApplication::activePopupWidget());
    window.resize(640, 600);
    QTest::qWait(80);
    QCOMPARE(window.size(), QSize(640, 600));
    auto *card = stats->findChild<QWidget *>("modalCard");
    auto *close = stats->findChild<QPushButton *>("statsCloseButton");
    auto *heading = stats->findChild<QLabel *>("heading");
    QVERIFY(card && close && heading && close->isVisible());
    QCOMPARE(heading->text(), QString("High Scores"));
    QVERIFY(close->icon().isNull());
    QVERIFY(stats->rect().contains(card->geometry()));
    QCOMPARE(card->geometry().center(), stats->rect().center());
    QVERIFY(card->rect().contains(QRect(close->mapTo(card, QPoint()), close->size())));
    const auto closeRect = QRect(close->mapTo(card, QPoint()), close->size());
    const auto headingRect = QRect(heading->mapTo(card, QPoint()), heading->size());
    const auto margins = card->layout()->contentsMargins();
    const auto contentRect = card->contentsRect().marginsRemoved(margins);
    QCOMPARE(headingRect.left(), contentRect.left());
    QCOMPARE(closeRect.top(), contentRect.top());
    QCOMPARE(closeRect.right(), contentRect.right());
    QCOMPARE(closeRect.center().y(), headingRect.center().y());
    for (const auto &mode : {QString("endless"), QString("classic")}) {
        auto *last = stats->findChild<QLabel *>(mode + "Score10");
        QVERIFY(last && last->isVisible());
        const auto lastRect = QRect(last->mapTo(card, QPoint()), last->size());
        QVERIFY(card->rect().contains(lastRect));
        QVERIFY(lastRect.top() > closeRect.bottom());
    }
    capture(window, "stats-modal-compact");
    saveTheme(temporary.filePath("theme.json"), preset("paper"));
    QTRY_COMPARE_WITH_TIMEOUT(window.theme().name, QString("paper"), 2000);
    QCOMPARE(stats->findChild<QLabel *>("endlessScore1")->palette().color(QPalette::WindowText), window.theme().color("accent"));
    QVERIFY(stats->isVisible());
    capture(window, "stats-modal-paper");
    QTest::keyClick(&window, Qt::Key_Escape);
    QVERIFY(!stats->isVisible());
    QVERIFY(content->isEnabled());
    QVERIFY(window.game().isRunning());
    QTest::mouseClick(statsButton, Qt::LeftButton);
    QTest::mouseClick(close, Qt::LeftButton);
    QVERIFY(!stats->isVisible());
    QVERIFY(window.game().isRunning());
    QTest::keyClick(&window, Qt::Key_P);
    QTest::mouseClick(statsButton, Qt::LeftButton);
    QTest::mouseClick(stats, Qt::LeftButton, Qt::NoModifier, QPoint(5, 5));
    QVERIFY(!stats->isVisible());
    QVERIFY(window.game().isPaused());
    window.close();
}

void UiTests::statsRecordEachGameOnce() {
    QTemporaryDir temporary;
    Window window(temporary.path(), ensureTheme(temporary.filePath("theme.json")), testPreferences(), 42);
    window.resize(640, 600);
    window.show();
    QTest::qWait(80);
    auto *statsButton = window.findChild<QPushButton *>("statsButton");
    auto *stats = window.findChild<StatsModal *>("statsModal");
    auto *first = stats->findChild<QLabel *>("endlessScore1");
    QTest::mouseClick(statsButton, Qt::LeftButton);
    QCOMPARE(first->text(), QString("—"));
    auto *card = stats->findChild<QWidget *>("modalCard");
    auto *last = stats->findChild<QLabel *>("classicScore10");
    QVERIFY(card && last && last->isVisible());
    QVERIFY(stats->rect().contains(card->geometry()));
    QVERIFY(card->rect().contains(QRect(last->mapTo(card, QPoint()), last->size())));
    capture(window, "stats-modal-empty-compact");
    QTest::keyClick(QApplication::focusWidget(), Qt::Key_Return);
    QVERIFY(!stats->isVisible());
    QVERIFY(window.game().isPaused());
    QTest::keyClick(&window, Qt::Key_Return);
    QTest::keyClick(&window, Qt::Key_Space);
    const auto endlessScore = window.game().score();
    QVERIFY(endlessScore > 0);
    QTest::keyClick(&window, Qt::Key_N, Qt::ControlModifier);
    auto records = loadRecords(temporary.path());
    QVERIFY(records.endlessScores == std::vector<std::uint64_t>{endlessScore});
    QTest::mouseClick(statsButton, Qt::LeftButton);
    QCOMPARE(first->text(), QLocale().toString(static_cast<qulonglong>(endlessScore)));
    QTest::keyClick(QApplication::focusWidget(), Qt::Key_Return);
    QTest::keyClick(&window, Qt::Key_Space);
    for (auto *tab : window.findChildren<QPushButton *>("modeTab"))
        if (tab->text() == "Classic")
            QTest::mouseClick(tab, Qt::LeftButton);
    records = loadRecords(temporary.path());
    QCOMPARE(records.endlessScores.size(), std::size_t(2));
    QCOMPARE(records.endlessScores[0], endlessScore);
    QCOMPARE(records.endlessScores[1], endlessScore);
    QTest::keyClick(&window, Qt::Key_Space);
    const auto classicScore = window.game().score();
    for (auto *tab : window.findChildren<QPushButton *>("modeTab"))
        if (tab->text() == "Practice")
            QTest::mouseClick(tab, Qt::LeftButton);
    QTest::keyClick(&window, Qt::Key_Space);
    window.close();
    records = loadRecords(temporary.path());
    QCOMPARE(records.endlessScores.size(), std::size_t(2));
    QVERIFY(records.classicScores == std::vector<std::uint64_t>{classicScore});

    Window nextWindow(temporary.path(), loadTheme(temporary.filePath("theme.json")), testPreferences(), 42);
    nextWindow.show();
    QTest::qWait(80);
    QTest::keyClick(&nextWindow, Qt::Key_Return);
    for (int index = 0; index < 32 && nextWindow.game().isRunning(); ++index)
        QTest::keyClick(&nextWindow, Qt::Key_Space);
    QVERIFY(nextWindow.game().isGameOver());
    QTRY_COMPARE_WITH_TIMEOUT(loadRecords(temporary.path()).endlessScores.size(), std::size_t(3), 1000);
    QTest::keyClick(&nextWindow, Qt::Key_Return);
    nextWindow.close();
    QCOMPARE(loadRecords(temporary.path()).endlessScores.size(), std::size_t(3));
}

int main(int argc, char **argv) {
    QApplication application(argc, argv);
    application.setStyle("Fusion");
    if (argc < 2)
        return 2;
    executable = QString::fromLocal8Bit(argv[1]);
    QStringList arguments{QString::fromLocal8Bit(argv[0])};
    for (int index = 2; index < argc; ++index)
        arguments.append(QString::fromLocal8Bit(argv[index]));
    UiTests tests;
    return QTest::qExec(&tests, arguments);
}

#include "ui_tests.moc"
