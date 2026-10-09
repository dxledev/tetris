#pragma once

#include <QJsonObject>
#include <QString>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace tetris {

inline constexpr std::size_t LeaderboardSize = 10;

struct Preferences {
    QString mode = "endless";
    int width = 900;
    int height = 860;
    int moveDelayMs = 150;
    int moveRepeatMs = 40;
    int softDropRepeatMs = 35;
    int lockDelayMs = 500;
    int maxLockResets = 15;
    int gravityInitialMs = 1000;
    int gravityStepMs = 75;
    int minimumGravityMs = 100;
    int linesPerLevel = 10;
    bool showGhost = true;
    bool showGrid = true;
    bool autoPause = true;
};

struct Records {
    std::uint64_t endlessScore = 0;
    std::uint64_t classicScore = 0;
    std::uint64_t classicTimeMs = 0;
    std::vector<std::uint64_t> endlessScores;
    std::vector<std::uint64_t> classicScores;
};

QString defaultConfigDirectory();
QJsonObject readObject(const QString &path);
void writeObject(const QString &path, const QJsonObject &object);
Preferences loadPreferences(const QString &directory);
void savePreferences(const QString &directory, const Preferences &preferences);
Records loadRecords(const QString &directory);
void saveRecords(const QString &directory, const Records &records);
bool recordScore(Records &records, const QString &mode, std::uint64_t score);
bool validMode(const QString &mode);

}
