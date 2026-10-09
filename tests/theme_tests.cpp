#include "core/theme.h"
#include "core/theme_source.h"
#include "ui/theme_watcher.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QSaveFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>
#include <cstdio>

using namespace tetris;

namespace {

bool writeText(const QString &path, const QString &text) {
    if (!QDir().mkpath(QFileInfo(path).absolutePath()))
        return false;
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    const auto bytes = text.toUtf8();
    return file.write(bytes) == bytes.size() && file.commit();
}

bool saveThemeLocked(const QString &path, const Theme &theme) {
    const auto lockPath = themeLockPath(path);
    if (!QDir().mkpath(QFileInfo(lockPath).absolutePath()))
        return false;
    QLockFile lock(lockPath);
    if (!lock.tryLock(1000))
        return false;
    saveTheme(path, theme);
    lock.unlock();
    return true;
}

void verifyPersistedTheme(const QString &path, const Theme &theme) {
    const auto persisted = loadTheme(path);
    QCOMPARE(persisted.name, theme.name);
    for (const auto &key : colorKeys())
        QCOMPARE(persisted.hex(key), theme.hex(key));
    QCOMPARE(persisted.sourceKind, theme.sourceKind);
    QCOMPARE(persisted.sourcePath, theme.sourcePath);
}

QString qmlPalette(const QString &accent, const QString &background = "#131720") {
    return QStringLiteral(R"(pragma Singleton
import QtQuick
Singleton {
    readonly property color background: "%1"
    readonly property color backgroundAlt: "#1c2230"
    readonly property color foreground: "#ebeffa"
    readonly property color foregroundInactive: "#97a3be"
    readonly property color primary: "%2"
    readonly property color border: "#34405a"
    readonly property color danger: "#ee7788"
    readonly property color cyan: "#65c9d8"
    readonly property color yellow: "#e8c56a"
    readonly property color purple: "#c09af4"
    readonly property color green: "#82c98f"
    readonly property color red: "#e77882"
    readonly property color blue: "#7298ee"
    readonly property color orange: "#eea161"
})
)").arg(background, accent);
}

QString paletteJson(const QString &accent, const QString &background = "#131720") {
    const QJsonObject colors{
        {"background", background},
        {"foreground", "#ebeffa"},
        {"primary", accent},
    };
    return QString::fromUtf8(QJsonDocument(QJsonObject{{"colors", colors}}).toJson());
}

QString noctaliaPalette(const QString &accent, const QString &background = "#181a1f") {
    const QJsonObject dark{
        {"mPrimary", accent}, {"mOnPrimary", "#0d1215"}, {"mSurface", background},
        {"mOnSurface", "#eceff2"}, {"mSurfaceVariant", "#20242e"},
        {"mOnSurfaceVariant", "#8f949c"}, {"mOutline", "#2b2f37"},
        {"mError", "#b31414"}, {"blue", "#8b1b1b"},
    };
    return QString::fromUtf8(QJsonDocument(QJsonObject{
        {"mode", "dark"}, {"dark", dark}, {"light", dark},
    }).toJson());
}

bool replaceSymlink(const QString &target, const QString &linkPath) {
    const QString temporaryPath = linkPath + ".new";
    QFile::remove(temporaryPath);
    if (!QFile::link(target, temporaryPath))
        return false;
    return std::rename(QFile::encodeName(temporaryPath).constData(),
                       QFile::encodeName(linkPath).constData()) == 0;
}

void createThemeDirectory(const QString &path, const QString &accent) {
    QDir().mkpath(path);
    writeText(QDir(path).filePath("Colors.qml"), qmlPalette(accent));
}

}

class ThemeTests : public QObject {
    Q_OBJECT
private slots:
    void presetRoundTripAndManualColors();
    void followsSelectorAndRetainsLastValidPalette();
    void followedRefreshPersistsAcrossSourceKinds();
    void rejectsSelfFollowCycles();
    void idleWatcherDoesNotChurnConfigDirectory();
    void watcherRetriesWhenThemeLockIsBusy();
    void importsNoctaliaJson();
};

void ThemeTests::presetRoundTripAndManualColors() {
    QCOMPARE(colorKeys().size(), 19);
    QCOMPARE(presets(), QStringList({"forest", "paper", "rose", "slate"}));

    const auto slate = preset("slate");
    QCOMPARE(slate.colors.size(), colorKeys().size());
    QCOMPARE(parseTheme(slate.toJson()), slate);

    const auto custom = setColors(slate, {"accent=#abcdef", "piece_i=#123456"});
    QCOMPARE(custom.color("accent"), QColor("#abcdef"));
    QCOMPARE(custom.color("piece_i"), QColor("#123456"));
    QVERIFY(custom.sourceKind.isEmpty());
    QVERIFY(custom.sourcePath.isEmpty());

    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QString path = temporary.filePath("config/theme.json");
    saveTheme(path, custom);
    QCOMPARE(loadTheme(path), custom);
    QVERIFY(QFileInfo(path).isFile());

    const QString ensuredPath = temporary.filePath("default/theme.json");
    const auto ensured = ensureTheme(ensuredPath);
    QCOMPARE(ensured.name, QString("slate"));
    QCOMPARE(ensureTheme(ensuredPath), ensured);
}

void ThemeTests::followsSelectorAndRetainsLastValidPalette() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QString themes = temporary.filePath("themes");
    const QString staticOne = QDir(themes).filePath("one");
    const QString staticTwo = QDir(themes).filePath("two");
    const QString dynamic = QDir(themes).filePath(".dynamic/caelestia");
    QDir().mkpath(staticOne);
    QDir().mkpath(staticTwo);
    createThemeDirectory(staticOne, "#aabbcc");
    createThemeDirectory(staticTwo, "#bbccdd");
    createThemeDirectory(dynamic, "#ccddee");

    const QString currentLink = QDir(themes).filePath("current");
    const QString selector = QDir(themes).filePath(".caelestia-use");
    QVERIFY(QFile::link(staticOne, currentLink));
    QVERIFY(QFile::link(currentLink, selector));

    const auto initial = bindSource(preset("slate"), "desktop", selector);
    QCOMPARE(initial.sourcePath, QFileInfo(selector).absoluteFilePath());
    QCOMPARE(initial.color("accent"), QColor("#aabbcc"));

    const QString config = temporary.filePath("app/theme.json");
    saveTheme(config, initial);
    ThemeWatcher watcher(config, loadTheme(config));
    QSignalSpy changes(&watcher, &ThemeWatcher::themeChanged);
    QSignalSpy rejections(&watcher, &ThemeWatcher::rejected);
    QTRY_COMPARE_WITH_TIMEOUT(watcher.theme().color("accent"), QColor("#aabbcc"), 1500);

    QVERIFY(writeText(QDir(staticOne).filePath("Colors.qml"), qmlPalette("#112233")));
    QTRY_COMPARE_WITH_TIMEOUT(watcher.theme().color("accent"), QColor("#112233"), 2000);
    verifyPersistedTheme(config, watcher.theme());

    QVERIFY(replaceSymlink(staticTwo, currentLink));
    QTRY_COMPARE_WITH_TIMEOUT(watcher.theme().color("accent"), QColor("#bbccdd"), 2000);

    QVERIFY(replaceSymlink(dynamic, selector));
    QTRY_COMPARE_WITH_TIMEOUT(watcher.theme().color("accent"), QColor("#ccddee"), 2000);

    const auto lastValid = watcher.theme();
    QVERIFY(writeText(QDir(dynamic).filePath("Colors.qml"), "not a palette"));
    QTRY_VERIFY_WITH_TIMEOUT(!rejections.isEmpty(), 2000);
    QCOMPARE(watcher.theme(), lastValid);

    QVERIFY(writeText(QDir(dynamic).filePath("Colors.qml"), qmlPalette("#445566")));
    QTRY_COMPARE_WITH_TIMEOUT(watcher.theme().color("accent"), QColor("#445566"), 2000);
    verifyPersistedTheme(config, watcher.theme());
    QVERIFY(changes.count() >= 4);

    const auto manual = setColors(preset("paper"), {"accent=#987654"});
    QVERIFY(saveThemeLocked(config, manual));
    QTRY_COMPARE_WITH_TIMEOUT(watcher.theme(), manual, 2000);
}

void ThemeTests::followedRefreshPersistsAcrossSourceKinds() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QString staticDirectory = temporary.filePath("themes/static");
    createThemeDirectory(staticDirectory, "#aabbcc");
    const QString caelestiaFile = temporary.filePath("caelestia/palette.json");
    const QString noctaliaDirectory = temporary.filePath("noctalia");
    QVERIFY(writeText(caelestiaFile, paletteJson("#ccddee")));
    QVERIFY(writeText(QDir(noctaliaDirectory).filePath("noctalia.json"), noctaliaPalette("#ddeeff")));

    const QString config = temporary.filePath("app/theme.json");
    const auto staticTheme = bindSource(preset("slate"), "static", staticDirectory);
    saveTheme(config, staticTheme);
    ThemeWatcher watcher(config, loadTheme(config));
    QTRY_COMPARE_WITH_TIMEOUT(watcher.theme().sourceKind, QString("static"), 1500);

    QVERIFY(writeText(QDir(staticDirectory).filePath("Colors.qml"), qmlPalette("#112233")));
    QTRY_COMPARE_WITH_TIMEOUT(watcher.theme().color("accent"), QColor("#112233"), 2000);
    verifyPersistedTheme(config, watcher.theme());

    const auto caelestiaTheme = bindSource(preset("slate"), "caelestia", caelestiaFile);
    QVERIFY(saveThemeLocked(config, caelestiaTheme));
    QTRY_COMPARE_WITH_TIMEOUT(watcher.theme().sourceKind, QString("caelestia"), 2000);
    QVERIFY(writeText(caelestiaFile, paletteJson("#334455")));
    QTRY_COMPARE_WITH_TIMEOUT(watcher.theme().color("accent"), QColor("#334455"), 2000);
    verifyPersistedTheme(config, watcher.theme());

    const auto noctaliaTheme = bindSource(preset("slate"), "noctalia", noctaliaDirectory);
    QVERIFY(saveThemeLocked(config, noctaliaTheme));
    QTRY_COMPARE_WITH_TIMEOUT(watcher.theme().sourceKind, QString("noctalia"), 2000);
    const QString noctaliaFile = QDir(noctaliaDirectory).filePath("noctalia.json");
    QVERIFY(writeText(noctaliaFile, noctaliaPalette("#556677")));
    QTRY_COMPARE_WITH_TIMEOUT(watcher.theme().color("accent"), QColor("#556677"), 2000);
    verifyPersistedTheme(config, watcher.theme());

    const auto manual = setColors(preset("paper"), {"accent=#987654"});
    const auto previousTheme = watcher.theme();
    QLockFile lock(themeLockPath(config));
    QVERIFY(lock.tryLock(1000));
    saveTheme(config, manual);
    QVERIFY(writeText(noctaliaFile, noctaliaPalette("#abcdef")));
    watcher.reload();
    QCOMPARE(watcher.theme(), previousTheme);
    lock.unlock();

    QTRY_COMPARE_WITH_TIMEOUT(watcher.theme(), manual, 2000);
    QTRY_COMPARE_WITH_TIMEOUT(loadTheme(config), manual, 2000);
}

void ThemeTests::rejectsSelfFollowCycles() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QString config = temporary.filePath("theme/theme.json");

    auto fileCycle = preset("slate");
    fileCycle.sourceKind = "file";
    fileCycle.sourcePath = QFileInfo(config).absoluteFilePath();
    saveTheme(config, fileCycle);
    ThemeWatcher fileWatcher(config, loadTheme(config));
    QSignalSpy fileRejections(&fileWatcher, &ThemeWatcher::rejected);
    QTRY_VERIFY_WITH_TIMEOUT(!fileRejections.isEmpty(), 2000);
    QCOMPARE(fileWatcher.theme(), fileCycle);

    const QString directoryConfig = temporary.filePath("directory/theme.json");
    auto directoryCycle = preset("slate");
    directoryCycle.sourceKind = "static";
    directoryCycle.sourcePath = QFileInfo(directoryConfig).absolutePath();
    saveTheme(directoryConfig, directoryCycle);
    ThemeWatcher directoryWatcher(directoryConfig, loadTheme(directoryConfig));
    QSignalSpy directoryRejections(&directoryWatcher, &ThemeWatcher::rejected);
    QTRY_VERIFY_WITH_TIMEOUT(!directoryRejections.isEmpty(), 2000);
    QCOMPARE(directoryWatcher.theme(), directoryCycle);
    QCOMPARE(loadTheme(directoryConfig), directoryCycle);
}

void ThemeTests::idleWatcherDoesNotChurnConfigDirectory() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QString configDirectory = temporary.filePath("app");
    const QString config = QDir(configDirectory).filePath("theme.json");
    QVERIFY(QDir().mkpath(QDir(configDirectory).filePath(".locks")));
    QCOMPARE(themeLockPath(config), QDir(configDirectory).filePath(".locks/theme.lock"));
    saveTheme(config, preset("slate"));

    QFileSystemWatcher directoryWatcher;
    QVERIFY(directoryWatcher.addPath(configDirectory));
    QSignalSpy directoryChanges(&directoryWatcher, &QFileSystemWatcher::directoryChanged);
    ThemeWatcher watcher(config, loadTheme(config));
    QSignalSpy rejections(&watcher, &ThemeWatcher::rejected);
    QTest::qWait(250);

    QCOMPARE(rejections.count(), 0);
    QCOMPARE(directoryChanges.count(), 0);
}

void ThemeTests::watcherRetriesWhenThemeLockIsBusy() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QString source = temporary.filePath("source/Colors.qml");
    QVERIFY(writeText(source, qmlPalette("#112233")));

    const QString config = temporary.filePath("app/theme.json");
    const auto initial = bindSource(preset("slate"), "static", source);
    saveTheme(config, initial);
    ThemeWatcher watcher(config, loadTheme(config));
    QTRY_COMPARE_WITH_TIMEOUT(watcher.theme().color("accent"), QColor("#112233"), 1500);

    const auto lockPath = themeLockPath(config);
    QVERIFY(QDir().mkpath(QFileInfo(lockPath).absolutePath()));
    QLockFile lock(lockPath);
    QVERIFY(lock.tryLock(1000));
    QVERIFY(writeText(source, qmlPalette("#445566")));
    watcher.reload();
    QCOMPARE(watcher.theme().color("accent"), QColor("#112233"));
    lock.unlock();

    QTRY_COMPARE_WITH_TIMEOUT(watcher.theme().color("accent"), QColor("#445566"), 2000);
    verifyPersistedTheme(config, watcher.theme());
}

void ThemeTests::importsNoctaliaJson() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QString directory = temporary.filePath("noctalia");
    QDir().mkpath(directory);
    const QJsonObject dark{
        {"mPrimary", "#ff2222"}, {"mOnPrimary", "#0d1215"}, {"mSurface", "#181a1f"},
        {"mOnSurface", "#eceff2"}, {"mSurfaceVariant", "#20242e"}, {"mOnSurfaceVariant", "#8f949c"},
        {"mOutline", "#2b2f37"}, {"mError", "#b31414"}, {"blue", "#8b1b1b"},
    };
    const QJsonObject data{{"mode", "dark"}, {"dark", dark}, {"light", dark}};
    QFile file(QDir(directory).filePath("noctalia.json"));
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write(QJsonDocument(data).toJson()), qint64(QJsonDocument(data).toJson().size()));
    file.close();

    const auto theme = bindSource(preset("slate"), "noctalia", directory);
    QCOMPARE(theme.color("accent"), QColor("#ff2222"));
    QCOMPARE(theme.color("background"), QColor("#181a1f"));
    QCOMPARE(theme.color("text"), QColor("#eceff2"));
    QCOMPARE(theme.sourceKind, QString("noctalia"));
    QCOMPARE(theme.sourcePath, QFileInfo(directory).absoluteFilePath());
}

QTEST_GUILESS_MAIN(ThemeTests)
#include "theme_tests.moc"
