#include "core/theme.h"
#include "core/theme_source.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QtTest>

using namespace tetris;

namespace {

bool writeText(const QString &path, const QByteArray &bytes) {
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}

}

class SourceTests : public QObject {
    Q_OBJECT
private slots:
    void terminalColorsAndSemanticColors();
    void preferredAliasWins();
    void completeThemeTakesPriority();
};

void SourceTests::terminalColorsAndSemanticColors() {
    QTemporaryDir temporary;
    const auto path = temporary.filePath("kitty.conf");
    QVERIFY(writeText(path, "background #101112\nforeground #eeeeee\ncursor #123456\ncolor1 #aa1122\ncolor2 #33bb44\ncolor3 #ccdd11\ncolor4 #5566ff\ncolor5 #cc55ee\ncolor6 #33ddff\n"));
    auto theme = bindSource(preset("slate"), "static", temporary.path());
    QCOMPARE(theme.color("background"), QColor("#101112"));
    QCOMPARE(theme.color("accent"), QColor("#123456"));
    QCOMPARE(theme.color("piece_i"), QColor("#33ddff"));
    QCOMPARE(theme.color("piece_t"), QColor("#cc55ee"));
    const auto qml = temporary.filePath("Colors.qml");
    QVERIFY(writeText(qml, "property color background: '#121212'\nproperty color foreground: '#fefefe'\nproperty color primary: '#123abc'\nproperty color secondary: '#aa22ff'\nproperty color success: '#22aa33'\nproperty color warning: '#ffcc22'\n"));
    theme = bindSource(theme, "static", temporary.path());
    QCOMPARE(theme.color("background"), QColor("#121212"));
    QCOMPARE(theme.color("accent"), QColor("#123abc"));
    QCOMPARE(theme.color("piece_i"), QColor("#33ddff"));
    const auto semantic = bindSource(theme, "file", qml);
    QCOMPARE(semantic.color("piece_i"), QColor("#aa22ff"));
    QCOMPARE(semantic.color("piece_s"), QColor("#22aa33"));
    QCOMPARE(semantic.color("piece_o"), QColor("#ffcc22"));
    QCOMPARE(semantic.color("piece_t"), QColor("#123abc"));
}

void SourceTests::preferredAliasWins() {
    QTemporaryDir temporary;
    const auto path = temporary.filePath("palette.json");
    QJsonObject object{{"background", "#112233"}, {"base", "#aabbcc"}, {"surface", "#334455"},
        {"foreground", "#ddeeff"}, {"text", "#ffffff"}, {"primary", "#ffee33"}};
    QVERIFY(writeText(path, QJsonDocument(object).toJson()));
    const auto theme = bindSource(preset("slate"), "file", path);
    QCOMPARE(theme.color("background"), QColor("#112233"));
    QCOMPARE(theme.color("text"), QColor("#ddeeff"));
}

void SourceTests::completeThemeTakesPriority() {
    QTemporaryDir temporary;
    const auto original = preset("forest");
    saveTheme(temporary.filePath("theme.json"), original);
    QVERIFY(writeText(temporary.filePath("Colors.qml"), "not a palette"));
    const auto theme = bindSource(preset("slate"), "static", temporary.path());
    QCOMPARE(theme.colors, original.colors);
}

QTEST_GUILESS_MAIN(SourceTests)
#include "source_tests.moc"
