#include "theme.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QSaveFile>
#include <algorithm>
#include <stdexcept>

namespace tetris {
namespace {

const QMap<QString, QStringList> palettes{
    {"forest", {"#101819", "#182324", "#1e2d2e", "#152522", "#edf3ed", "#91a6a2", "#b6e3c6", "#304243", "#536967", "#355c4b", "#203230", "#e07a70", "#72d6d2", "#f4d35e", "#c69bee", "#8cd790", "#ef7777", "#7fa9ef", "#f3a65a"}},
    {"paper", {"#f4f1e9", "#fffcf5", "#ebe7dd", "#f3f0e7", "#292f2c", "#6f7970", "#315c48", "#ddd8cb", "#9aab9b", "#ccdfcc", "#eeeee1", "#b9423b", "#238b9b", "#c28b23", "#8a57a6", "#52803a", "#c44748", "#4b67b2", "#c56b2b"}},
    {"slate", {"#131720", "#1c2230", "#252e40", "#202838", "#ebeffa", "#97a3be", "#b2c7ff", "#34405a", "#576986", "#3b4e78", "#252e44", "#ee7788", "#65c9d8", "#e8c56a", "#c09af4", "#82c98f", "#e77882", "#7298ee", "#eea161"}},
    {"rose", {"#211a22", "#2c232e", "#382d3b", "#332833", "#f7eaf1", "#b8a0b2", "#efb7ce", "#4e3a4c", "#85687f", "#654258", "#3b2c3b", "#ed8796", "#91d7e3", "#eed49f", "#c6a0f6", "#a6da95", "#ee99a0", "#8aadf4", "#f5a97f"}},
};

[[noreturn]] void fail(const QString &message) {
    throw std::invalid_argument(message.toStdString());
}

QJsonObject readJson(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        fail(path + ": " + file.errorString());
    if (file.size() > 1024 * 1024)
        fail(path + ": JSON file exceeds 1 MiB");
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
        fail(path + ": expected a valid JSON object (" + error.errorString() + ")");
    return document.object();
}

}

QColor Theme::color(const QString &key) const {
    return colors.value(key);
}

QString Theme::hex(const QString &key) const {
    return color(key).name(QColor::HexRgb);
}

QJsonObject Theme::toJson() const {
    QJsonObject palette;
    for (const auto &key : colorKeys()) {
        const auto value = color(key);
        if (!value.isValid())
            fail("Missing or invalid theme color: " + key);
        palette.insert(key, value.name(QColor::HexRgb));
    }
    QJsonObject result{{"version", 1}, {"name", name}, {"colors", palette}};
    if (!sourceKind.isEmpty())
        result.insert("source", QJsonObject{{"kind", sourceKind}, {"path", sourcePath}});
    return result;
}

bool Theme::operator==(const Theme &other) const {
    return name == other.name && colors == other.colors
        && sourceKind == other.sourceKind && sourcePath == other.sourcePath;
}

QStringList colorKeys() {
    return {"background", "surface", "surface_alt", "board", "text", "muted", "accent",
            "border", "grid", "ghost", "selection", "danger", "piece_i", "piece_o",
            "piece_t", "piece_s", "piece_z", "piece_j", "piece_l"};
}

QStringList presets() {
    return palettes.keys();
}

Theme preset(const QString &name) {
    if (!palettes.contains(name))
        fail("Unknown preset: " + name + ". Available: " + presets().join(", "));
    Theme theme;
    theme.name = name;
    const auto keys = colorKeys();
    const auto values = palettes.value(name);
    for (int index = 0; index < keys.size(); ++index)
        theme.colors.insert(keys[index], QColor(values[index]));
    return theme;
}

Theme parseTheme(const QJsonObject &data) {
    if (data.value("version").toInt(-1) != 1)
        fail("Theme version must be 1");
    const QString name = data.value("name").toString().trimmed();
    if (name.isEmpty() || name.size() > 60)
        fail("Theme name must contain 1–60 characters");
    if (!data.value("colors").isObject())
        fail("Theme must contain a colors object");

    const auto palette = data.value("colors").toObject();
    const auto keys = colorKeys();
    for (auto entry = palette.constBegin(); entry != palette.constEnd(); ++entry) {
        if (!keys.contains(entry.key()))
            fail("Unknown color: " + entry.key());
    }

    static const QRegularExpression colorPattern("^#[0-9a-fA-F]{6}$");
    Theme theme;
    theme.name = name;
    for (const auto &key : keys) {
        const auto value = palette.value(key).toString();
        if (!colorPattern.match(value).hasMatch())
            fail(key + ": expected a color in #RRGGBB format");
        theme.colors.insert(key, QColor(value));
    }

    if (data.contains("source")) {
        if (!data.value("source").isObject())
            fail("Theme source must be an object");
        const auto source = data.value("source").toObject();
        theme.sourceKind = source.value("kind").toString();
        theme.sourcePath = source.value("path").toString();
        if (!QStringList{"desktop", "static", "file", "caelestia", "noctalia"}.contains(theme.sourceKind)
            || theme.sourcePath.isEmpty() || !QFileInfo(theme.sourcePath).isAbsolute())
            fail("Theme source needs a known kind and an absolute path");
    }
    return theme;
}

Theme loadTheme(const QString &path) {
    return parseTheme(readJson(path));
}

void saveTheme(const QString &path, const Theme &theme) {
    const auto validated = parseTheme(theme.toJson());
    if (!QDir().mkpath(QFileInfo(path).absolutePath()))
        fail("Cannot create " + QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        fail(path + ": " + file.errorString());
    const auto bytes = QJsonDocument(validated.toJson()).toJson(QJsonDocument::Indented);
    if (file.write(bytes) != bytes.size() || !file.commit())
        fail(path + ": " + file.errorString());
}

QString themeLockPath(const QString &outputPath) {
    const auto configDirectory = QFileInfo(outputPath).absolutePath();
    return QDir::cleanPath(QDir(configDirectory).filePath(".locks/theme.lock"));
}

Theme ensureTheme(const QString &path) {
    if (QFileInfo::exists(path))
        return loadTheme(path);
    const auto theme = preset("slate");
    saveTheme(path, theme);
    return theme;
}

Theme setColors(Theme theme, const QStringList &assignments) {
    if (assignments.isEmpty())
        fail("Provide at least one COLOR=#RRGGBB assignment");
    auto data = theme.toJson();
    auto palette = data.value("colors").toObject();
    for (const auto &assignment : assignments) {
        const auto separator = assignment.indexOf('=');
        if (separator < 1)
            fail("Expected COLOR=#RRGGBB. Colors: " + colorKeys().join(", "));
        const auto key = assignment.left(separator);
        if (!colorKeys().contains(key))
            fail("Unknown color: " + key);
        palette.insert(key, assignment.mid(separator + 1));
    }
    data.insert("name", "custom");
    data.remove("source");
    data.insert("colors", palette);
    return parseTheme(data);
}

}
