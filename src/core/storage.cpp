#include "storage.h"

#include <algorithm>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QStandardPaths>
#include <functional>
#include <stdexcept>
#include <utility>

namespace tetris {
namespace {

[[noreturn]] void fail(const QString &message) {
    throw std::runtime_error(message.toStdString());
}

int readNumber(const QJsonObject &object, const QString &key, int fallback, int minimum, int maximum) {
    if (!object.contains(key))
        return fallback;
    const auto value = object.value(key);
    const int number = value.toInt(minimum - 1);
    if (!value.isDouble() || value.toDouble() != number || number < minimum || number > maximum)
        fail(key + ": expected an integer from " + QString::number(minimum) + " to " + QString::number(maximum));
    return number;
}

bool readFlag(const QJsonObject &object, const QString &key, bool fallback) {
    if (!object.contains(key))
        return fallback;
    if (!object.value(key).isBool())
        fail(key + ": expected true or false");
    return object.value(key).toBool();
}

std::uint64_t readRecordValue(const QJsonValue &value, const QString &key) {
    if (!value.isString())
        fail(key + ": expected an unsigned integer string");
    bool valid = false;
    const auto number = value.toString().toULongLong(&valid);
    if (!valid)
        fail(key + ": expected an unsigned integer string");
    return number;
}

std::uint64_t readRecord(const QJsonObject &object, const QString &key) {
    if (!object.contains(key))
        return 0;
    return readRecordValue(object.value(key), key);
}

using ScoreList = std::vector<std::uint64_t>;

ScoreList normalizedScores(ScoreList scores, std::uint64_t legacyScore) {
    if (legacyScore != 0 && std::find(scores.begin(), scores.end(), legacyScore) == scores.end())
        scores.push_back(legacyScore);
    scores.erase(std::remove(scores.begin(), scores.end(), 0), scores.end());
    std::sort(scores.begin(), scores.end(), std::greater<>());
    if (scores.size() > LeaderboardSize)
        scores.resize(LeaderboardSize);
    return scores;
}

ScoreList readScores(const QJsonObject &object, const QString &key, std::uint64_t legacyScore) {
    if (!object.contains(key))
        return legacyScore == 0 ? ScoreList{} : ScoreList{legacyScore};
    const auto value = object.value(key);
    if (!value.isArray())
        fail(key + ": expected an array of unsigned integer strings");
    ScoreList scores;
    for (const auto &entry : value.toArray())
        scores.push_back(readRecordValue(entry, key));
    return normalizedScores(std::move(scores), legacyScore);
}

QJsonArray scoresToJson(const ScoreList &scores) {
    QJsonArray result;
    for (const auto score : scores)
        result.append(QString::number(score));
    return result;
}

}

QString defaultConfigDirectory() {
    const auto directory = qEnvironmentVariable("TETRIS_CONFIG_DIR");
    return directory.isEmpty()
        ? QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + "/tetris"
        : QDir(directory).absolutePath();
}

bool validMode(const QString &mode) {
    return QStringList{"endless", "classic", "practice"}.contains(mode);
}

QJsonObject readObject(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        fail(path + ": " + file.errorString());
    if (file.size() > 1024 * 1024)
        fail(path + ": file exceeds 1 MiB");
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
        fail(path + ": expected a JSON object");
    return document.object();
}

void writeObject(const QString &path, const QJsonObject &object) {
    if (!QDir().mkpath(QFileInfo(path).absolutePath()))
        fail("Cannot create " + QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        fail(path + ": " + file.errorString());
    const auto bytes = QJsonDocument(object).toJson(QJsonDocument::Indented);
    if (file.write(bytes) != bytes.size() || !file.commit())
        fail(path + ": " + file.errorString());
}

Preferences loadPreferences(const QString &directory) {
    Preferences result;
    const auto path = directory + "/settings.json";
    if (!QFileInfo::exists(path))
        return result;
    const auto object = readObject(path);
    result.mode = object.value("mode").toString(result.mode);
    if (!validMode(result.mode))
        fail("mode: expected endless, classic, or practice");
    result.width = readNumber(object, "width", result.width, 640, 8192);
    result.height = readNumber(object, "height", result.height, 600, 8192);
    result.moveDelayMs = readNumber(object, "move_delay_ms", result.moveDelayMs, 50, 1000);
    result.moveRepeatMs = readNumber(object, "move_repeat_ms", result.moveRepeatMs, 10, 250);
    result.softDropRepeatMs = readNumber(object, "soft_drop_repeat_ms", result.softDropRepeatMs, 10, 250);
    result.lockDelayMs = readNumber(object, "lock_delay_ms", result.lockDelayMs, 100, 2000);
    result.maxLockResets = readNumber(object, "max_lock_resets", result.maxLockResets, 0, 100);
    result.gravityInitialMs = readNumber(object, "gravity_initial_ms", result.gravityInitialMs, 1, 60000);
    result.gravityStepMs = readNumber(object, "gravity_step_ms", result.gravityStepMs, 0, 5000);
    result.minimumGravityMs = readNumber(object, "minimum_gravity_ms", result.minimumGravityMs, 1, 60000);
    result.linesPerLevel = readNumber(object, "lines_per_level", result.linesPerLevel, 1, 100);
    result.showGhost = readFlag(object, "show_ghost", result.showGhost);
    result.showGrid = readFlag(object, "show_grid", result.showGrid);
    result.autoPause = readFlag(object, "auto_pause", result.autoPause);
    return result;
}

void savePreferences(const QString &directory, const Preferences &preferences) {
    writeObject(directory + "/settings.json", {
        {"version", 1}, {"mode", preferences.mode}, {"width", preferences.width}, {"height", preferences.height},
        {"move_delay_ms", preferences.moveDelayMs}, {"move_repeat_ms", preferences.moveRepeatMs},
        {"soft_drop_repeat_ms", preferences.softDropRepeatMs}, {"lock_delay_ms", preferences.lockDelayMs},
        {"max_lock_resets", preferences.maxLockResets}, {"gravity_initial_ms", preferences.gravityInitialMs},
        {"gravity_step_ms", preferences.gravityStepMs}, {"minimum_gravity_ms", preferences.minimumGravityMs},
        {"lines_per_level", preferences.linesPerLevel},
        {"show_ghost", preferences.showGhost}, {"show_grid", preferences.showGrid}, {"auto_pause", preferences.autoPause}});
}

Records loadRecords(const QString &directory) {
    const auto path = directory + "/records.json";
    if (!QFileInfo::exists(path))
        return {};
    const auto object = readObject(path);
    Records result;
    result.endlessScore = readRecord(object, "endless_score");
    result.classicScore = readRecord(object, "classic_score");
    result.classicTimeMs = readRecord(object, "classic_time_ms");
    result.endlessScores = readScores(object, "endless_scores", result.endlessScore);
    result.classicScores = readScores(object, "classic_scores", result.classicScore);
    result.endlessScore = result.endlessScores.empty() ? 0 : result.endlessScores.front();
    result.classicScore = result.classicScores.empty() ? 0 : result.classicScores.front();
    return result;
}

void saveRecords(const QString &directory, const Records &records) {
    const auto endlessScores = normalizedScores(records.endlessScores, records.endlessScore);
    const auto classicScores = normalizedScores(records.classicScores, records.classicScore);
    const auto endlessScore = endlessScores.empty() ? 0 : endlessScores.front();
    const auto classicScore = classicScores.empty() ? 0 : classicScores.front();
    writeObject(directory + "/records.json", {{"version", 2},
        {"endless_score", QString::number(endlessScore)},
        {"classic_score", QString::number(classicScore)},
        {"classic_time_ms", QString::number(records.classicTimeMs)},
        {"endless_scores", scoresToJson(endlessScores)},
        {"classic_scores", scoresToJson(classicScores)}});
}

bool recordScore(Records &records, const QString &mode, std::uint64_t score) {
    if (!validMode(mode) || mode == "practice")
        return false;

    auto &scores = mode == "classic" ? records.classicScores : records.endlessScores;
    auto &best = mode == "classic" ? records.classicScore : records.endlessScore;
    const auto originalScores = scores;
    const auto originalBest = best;
    scores = normalizedScores(std::move(scores), best);
    if (score != 0)
        scores.push_back(score);
    scores = normalizedScores(std::move(scores), best);
    best = scores.empty() ? 0 : scores.front();
    return scores != originalScores || best != originalBest;
}

}
