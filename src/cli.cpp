#include "cli.h"
#include "core/storage.h"
#include "core/theme.h"
#include "core/theme_source.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QLockFile>
#include <QTextStream>
#include <limits>
#include <stdexcept>

namespace tetris {
namespace {

[[noreturn]] void fail(const QString &message) {
    throw std::invalid_argument(message.toStdString());
}

void requireCount(const QStringList &arguments, int count) {
    if (arguments.size() != count)
        fail("Invalid arguments. Run tetris --help for usage.");
}

Theme currentTheme(const QString &path) {
    return QFileInfo::exists(path) ? loadTheme(path) : preset("slate");
}

void printTheme(const Theme &theme) {
    QTextStream(stdout) << QJsonDocument(theme.toJson()).toJson(QJsonDocument::Indented);
}

Theme proposedTheme(const Theme &current, const QString &command, const QStringList &arguments, const QString &outputPath) {
    if (command == "preset") {
        requireCount(arguments, 1);
        return preset(arguments.first());
    }
    if (command == "set")
        return setColors(current, arguments);
    if (command == "import") {
        requireCount(arguments, 1);
        auto imported = importPalette(QDir::cleanPath(QFileInfo(arguments.first()).absoluteFilePath()));
        imported.sourceKind.clear();
        imported.sourcePath.clear();
        return imported;
    }
    if (command == "detach") {
        requireCount(arguments, 0);
        auto detached = current;
        detached.sourceKind.clear();
        detached.sourcePath.clear();
        return detached;
    }
    if (command == "follow") {
        if (arguments.isEmpty() || arguments.size() > 2)
            fail("Expected theme follow SOURCE [PATH]");
        const auto kind = arguments.first() == "auto" ? QString("desktop") : arguments.first();
        auto result = bindSource(current, kind, arguments.value(1));
        if (QFileInfo(result.sourcePath).canonicalFilePath() == QFileInfo(outputPath).canonicalFilePath()
            || QDir::cleanPath(result.sourcePath) == QDir::cleanPath(outputPath))
            fail("A theme cannot follow its own output file");
        return result;
    }
    fail("Unknown theme command: " + command);
}

}

Options parseOptions(const QStringList &arguments) {
    Options result;
    result.configDirectory = defaultConfigDirectory();
    for (int index = 0; index < arguments.size(); ++index) {
        const auto argument = arguments[index];
        if (argument == "--config-dir" || argument == "--mode" || argument == "--seed") {
            if (++index >= arguments.size() || arguments[index].startsWith("--"))
                fail(argument + " requires a value");
            const auto value = arguments[index];
            if (argument == "--config-dir") {
                if (value.trimmed().isEmpty())
                    fail("--config-dir requires a directory");
                result.configDirectory = QDir(value).absolutePath();
            } else if (argument == "--mode") {
                if (!validMode(value))
                    fail("--mode requires endless, classic, or practice");
                result.mode = value;
            } else {
                bool valid = false;
                const auto seed = value.toULongLong(&valid);
                if (!valid || seed > std::numeric_limits<std::uint32_t>::max() || value.startsWith('-'))
                    fail("--seed requires an unsigned 32-bit integer");
                result.seed = static_cast<std::uint32_t>(seed);
            }
        } else if (argument == "--dry-run") {
            result.dryRun = true;
        } else if (argument == "--help" || argument == "-h") {
            result.help = true;
        } else if (argument == "--version") {
            result.version = true;
        } else if (argument.startsWith('-')) {
            fail("Unknown option: " + argument);
        } else {
            result.commands.append(argument);
        }
    }
    return result;
}

void printHelp() {
    QTextStream(stdout) << R"(Tetris — a standalone native falling-block game

Usage:
  tetris [--mode endless|classic|practice] [--seed NUMBER]
         [--config-dir DIRECTORY]
  tetris theme list|keys|path|show
  tetris theme preset forest|paper|rose|slate [--dry-run]
  tetris theme set 'accent=#b2c7ff' 'background=#131720' [--dry-run]
  tetris theme import FILE [--dry-run]
  tetris theme export FILE [--dry-run]
  tetris theme follow auto|static|file [PATH] [--dry-run]
  tetris theme detach [--dry-run]

Colors use #RRGGBB. Theme changes atomically replace theme.json and open windows
reload immediately. Import, preset, set, and detach stop following a source.
Following a file or directory also watches atomic replacements and symlink changes.
--dry-run prints the proposed JSON and leaves files unchanged.
All commands accept --config-dir or the TETRIS_CONFIG_DIR environment variable.

Controls:
  Left / Right     Move               Up / X     Rotate clockwise
  Down             Soft drop          Z          Rotate counterclockwise
  Space            Hard drop          C / Shift  Hold
  P / Escape       Pause              Enter      Start / resume
  Ctrl+N           New game           F1         Controls

Endless increases speed with each level. Classic is a 40-line sprint.
Practice lets you place pieces at your own pace, with automatic falling disabled.
)";
}

int runThemeCommand(const Options &options) {
    if (options.mode || options.seed)
        fail("--mode and --seed apply to the game");
    auto arguments = options.commands;
    if (arguments.isEmpty() || arguments.takeFirst() != "theme" || arguments.isEmpty())
        fail("Expected a theme subcommand. Run tetris --help.");
    const auto command = arguments.takeFirst();
    const auto path = options.configDirectory + "/theme.json";
    if (QStringList{"list", "keys", "path", "show"}.contains(command)) {
        requireCount(arguments, 0);
        if (command == "show")
            printTheme(currentTheme(path));
        else
            QTextStream(stdout) << (command == "list" ? presets().join('\n')
                : command == "keys" ? colorKeys().join('\n') : path) << '\n';
        return 0;
    }
    if (command == "export") {
        requireCount(arguments, 1);
        auto theme = currentTheme(path);
        theme.sourceKind.clear();
        theme.sourcePath.clear();
        if (options.dryRun)
            printTheme(theme);
        else
            saveTheme(QFileInfo(arguments.first()).absoluteFilePath(), theme);
        return 0;
    }
    if (options.dryRun) {
        printTheme(proposedTheme(currentTheme(path), command, arguments, path));
        return 0;
    }
    if (!QDir().mkpath(options.configDirectory))
        fail("Cannot create " + options.configDirectory);
    const auto lockPath = themeLockPath(path);
    if (!QDir().mkpath(QFileInfo(lockPath).absolutePath()))
        fail("Cannot create theme lock directory");
    QLockFile lock(lockPath);
    if (!lock.tryLock(2000))
        fail("Theme is being edited by another process");
    const auto theme = proposedTheme(currentTheme(path), command, arguments, path);
    saveTheme(path, theme);
    QTextStream(stdout) << "Saved " << path << '\n';
    return 0;
}

}
