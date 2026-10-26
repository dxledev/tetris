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
    void qmlBackgroundFormats_data();
    void qmlBackgroundFormats();
    void jsonRgbaColorsKeepRgbOrder();
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

void SourceTests::qmlBackgroundFormats_data() {
    QTest::addColumn<QString>("expression");
    QTest::addColumn<QString>("expected");

    QTest::newRow("rgb") << "'#0e1e36'" << "#0e1e36";
    QTest::newRow("akane-argb") << "\"#e00e1e36\"" << "#0e1e36";
    QTest::newRow("transparent-argb") << "'#000e1e36'" << "#0e1e36";
    QTest::newRow("opaque-argb") << "'#ff0e1e36'" << "#0e1e36";
    QTest::newRow("uppercase-argb") << "'#E00E1E36'" << "#0e1e36";
    QTest::newRow("fractional-rgba") << "Qt.rgba(14 / 255, 30 / 255, 54 / 255, 0.88)" << "#0e1e36";
    QTest::newRow("decimal-rgba") << "Qt.rgba(0.2, 0.4, 0.6, 0.5)" << "#336699";
}

void SourceTests::qmlBackgroundFormats() {
    QFETCH(QString, expression);
    QFETCH(QString, expected);
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const auto path = temporary.filePath("Colors.qml");
    const auto qml = QString(
        "readonly property color background: %1\n"
        "readonly property color backgroundAlt: '#e01a2a46'\n"
        "readonly property color foreground: '#f4b999'\n"
        "readonly property color primary: '#9279aa'\n").arg(expression);
    QVERIFY(writeText(path, qml.toUtf8()));

    const auto followed = bindSource(preset("slate"), "static", temporary.path());
    for (const auto &theme : {importPalette(path), followed, parseTheme(followed.toJson())}) {
        QCOMPARE(theme.color("background"), QColor(expected));
        QCOMPARE(theme.color("surface"), QColor("#1a2a46"));
        QCOMPARE(theme.color("accent"), QColor("#9279aa"));
        QCOMPARE(theme.color("text"), QColor("#f4b999"));
        QCOMPARE(theme.color("background").alpha(), 255);
    }
}

void SourceTests::jsonRgbaColorsKeepRgbOrder() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const auto path = temporary.filePath("palette.json");
    const QJsonObject object{{"background", "#0e1e36e0"}, {"foreground", "#f4b999"}, {"primary", "#9279aa"}};
    QVERIFY(writeText(path, QJsonDocument(object).toJson()));
    QCOMPARE(importPalette(path).color("background"), QColor("#0e1e36"));
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
