#pragma once

#include <QColor>
#include <QJsonObject>
#include <QMap>
#include <QStringList>

namespace tetris {

struct Theme {
    QString name;
    QMap<QString, QColor> colors;
    QString sourceKind;
    QString sourcePath;

    QColor color(const QString &key) const;
    QString hex(const QString &key) const;
    QJsonObject toJson() const;
    bool operator==(const Theme &other) const;
    bool operator!=(const Theme &other) const { return !(*this == other); }
};

QStringList colorKeys();
QStringList presets();
Theme preset(const QString &name);
Theme parseTheme(const QJsonObject &data);
Theme loadTheme(const QString &path);
void saveTheme(const QString &path, const Theme &theme);
QString themeLockPath(const QString &outputPath);
Theme ensureTheme(const QString &path);
Theme setColors(Theme theme, const QStringList &assignments);

}
