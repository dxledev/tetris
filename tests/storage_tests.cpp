#include "core/storage.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>
#include <stdexcept>
#include <vector>

using namespace tetris;

class StorageTests : public QObject {
    Q_OBJECT
private slots:
    void migratesLegacyRecords();
    void retainsTopTenAndTies();
    void keepsModesIndependentAndSkipsPractice();
    void preservesExistingFiveAndGrowsToTen();
    void savesAndLoadsNormalizedVersionTwo();
    void rejectsMalformedScoreArrays();
};

void StorageTests::migratesLegacyRecords() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    writeObject(temporary.filePath("records.json"), {
        {"version", 1},
        {"endless_score", "1200"},
        {"classic_score", "800"},
        {"classic_time_ms", "45678"},
    });

    const auto records = loadRecords(temporary.path());
    QCOMPARE(records.endlessScore, std::uint64_t(1200));
    QCOMPARE(records.classicScore, std::uint64_t(800));
    QCOMPARE(records.classicTimeMs, std::uint64_t(45678));
    QVERIFY((records.endlessScores == std::vector<std::uint64_t>{1200}));
    QVERIFY((records.classicScores == std::vector<std::uint64_t>{800}));
}

void StorageTests::retainsTopTenAndTies() {
    Records records;
    for (const auto score : {40, 10, 50, 30, 20, 60, 70, 80, 90, 100, 110, 120})
        QVERIFY(recordScore(records, "endless", score));
    QVERIFY((records.endlessScores == std::vector<std::uint64_t>{120, 110, 100, 90, 80, 70, 60, 50, 40, 30}));

    QVERIFY(recordScore(records, "endless", 120));
    QVERIFY((records.endlessScores == std::vector<std::uint64_t>{120, 120, 110, 100, 90, 80, 70, 60, 50, 40}));
    QCOMPARE(records.endlessScore, std::uint64_t(120));
    QVERIFY(!recordScore(records, "endless", 30));
    QVERIFY(!recordScore(records, "endless", 0));
}

void StorageTests::keepsModesIndependentAndSkipsPractice() {
    Records records;
    records.classicTimeMs = 9876;
    QVERIFY(recordScore(records, "endless", 300));
    QVERIFY(recordScore(records, "classic", 500));
    QVERIFY(!recordScore(records, "practice", 900));
    QVERIFY(!recordScore(records, "unknown", 1000));
    QVERIFY((records.endlessScores == std::vector<std::uint64_t>{300}));
    QVERIFY((records.classicScores == std::vector<std::uint64_t>{500}));
    QCOMPARE(records.endlessScore, std::uint64_t(300));
    QCOMPARE(records.classicScore, std::uint64_t(500));
    QCOMPARE(records.classicTimeMs, std::uint64_t(9876));
}

void StorageTests::preservesExistingFiveAndGrowsToTen() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    writeObject(temporary.filePath("records.json"), {
        {"version", 2},
        {"endless_scores", QJsonArray{QStringLiteral("50"), QStringLiteral("40"),
            QStringLiteral("30"), QStringLiteral("20"), QStringLiteral("10")}},
        {"classic_scores", QJsonArray{QStringLiteral("500"), QStringLiteral("400"),
            QStringLiteral("300"), QStringLiteral("200"), QStringLiteral("100")}},
    });

    auto records = loadRecords(temporary.path());
    QVERIFY((records.endlessScores == std::vector<std::uint64_t>{50, 40, 30, 20, 10}));
    QVERIFY((records.classicScores == std::vector<std::uint64_t>{500, 400, 300, 200, 100}));
    for (const auto score : {60, 70, 80, 90, 100})
        QVERIFY(recordScore(records, "endless", score));
    for (const auto score : {600, 700, 800, 900, 1000})
        QVERIFY(recordScore(records, "classic", score));

    saveRecords(temporary.path(), records);
    const auto loaded = loadRecords(temporary.path());
    QVERIFY((loaded.endlessScores == std::vector<std::uint64_t>{100, 90, 80, 70, 60, 50, 40, 30, 20, 10}));
    QVERIFY((loaded.classicScores == std::vector<std::uint64_t>{1000, 900, 800, 700, 600, 500, 400, 300, 200, 100}));
}

void StorageTests::savesAndLoadsNormalizedVersionTwo() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    Records records;
    records.endlessScore = 45;
    records.endlessScores = {0, 10, 30, 20, 40, 50, 60, 70, 80, 90, 100, 110};
    records.classicScore = 70;
    records.classicScores = {70, 70, 0};
    records.classicTimeMs = 123456;

    saveRecords(temporary.path(), records);
    const auto object = readObject(temporary.filePath("records.json"));
    QCOMPARE(object.value("version").toInt(), 2);
    QCOMPARE(object.value("endless_scores").toArray(),
        (QJsonArray{QStringLiteral("110"), QStringLiteral("100"), QStringLiteral("90"),
            QStringLiteral("80"), QStringLiteral("70"), QStringLiteral("60"), QStringLiteral("50"),
            QStringLiteral("45"), QStringLiteral("40"), QStringLiteral("30")}));
    QCOMPARE(object.value("classic_scores").toArray(),
        (QJsonArray{QStringLiteral("70"), QStringLiteral("70")}));

    const auto loaded = loadRecords(temporary.path());
    QVERIFY((loaded.endlessScores == std::vector<std::uint64_t>{110, 100, 90, 80, 70, 60, 50, 45, 40, 30}));
    QVERIFY((loaded.classicScores == std::vector<std::uint64_t>{70, 70}));
    QCOMPARE(loaded.endlessScore, std::uint64_t(110));
    QCOMPARE(loaded.classicScore, std::uint64_t(70));
    QCOMPARE(loaded.classicTimeMs, std::uint64_t(123456));
}

void StorageTests::rejectsMalformedScoreArrays() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    writeObject(temporary.filePath("records.json"), {
        {"endless_scores", QJsonArray{QStringLiteral("20"), 10}},
    });
    QVERIFY_EXCEPTION_THROWN(loadRecords(temporary.path()), std::runtime_error);

    writeObject(temporary.filePath("records.json"), {
        {"endless_scores", QStringLiteral("20")},
    });
    QVERIFY_EXCEPTION_THROWN(loadRecords(temporary.path()), std::runtime_error);
}

QTEST_GUILESS_MAIN(StorageTests)

#include "storage_tests.moc"
