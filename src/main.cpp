#include "cli.h"
#include "core/storage.h"
#include "core/theme.h"
#include "ui/window.h"

#include <QApplication>
#include <QCoreApplication>
#include <QIcon>
#include <QTextStream>
#include <stdexcept>

int main(int argc, char **argv) {
    QCoreApplication::setApplicationName("tetris");
    QCoreApplication::setApplicationVersion("1.0.0");
    try {
        QStringList arguments;
        for (int index = 1; index < argc; ++index)
            arguments.append(QString::fromLocal8Bit(argv[index]));
        const auto options = tetris::parseOptions(arguments);
        if (options.help || options.version || !options.commands.isEmpty()) {
            QCoreApplication app(argc, argv);
            if (options.help)
                tetris::printHelp();
            else if (options.version)
                QTextStream(stdout) << "Tetris 1.0.0\n";
            else
                return tetris::runThemeCommand(options);
            return 0;
        }
        if (options.dryRun)
            throw std::invalid_argument("--dry-run requires a theme command");
        if (!qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY") && qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
            qputenv("QT_QPA_PLATFORM", "wayland");
        // Raster painting uses shared memory instead of allocating GPU buffers.
        if (qEnvironmentVariableIsEmpty("QT_WAYLAND_CLIENT_BUFFER_INTEGRATION"))
            qputenv("QT_WAYLAND_CLIENT_BUFFER_INTEGRATION", "shm");
        QApplication app(argc, argv);
        app.setApplicationDisplayName("Tetris");
        app.setDesktopFileName("io.github.tetris");
        app.setStyle("Fusion");
        app.setWindowIcon(QIcon::fromTheme("io.github.tetris"));
        auto preferences = tetris::loadPreferences(options.configDirectory);
        if (options.mode)
            preferences.mode = *options.mode;
        auto theme = tetris::ensureTheme(options.configDirectory + "/theme.json");
        tetris::Window window(options.configDirectory, theme, preferences, options.seed);
        window.show();
        return app.exec();
    } catch (const std::exception &error) {
        QTextStream(stderr) << "tetris: " << error.what() << '\n';
        return 1;
    }
}
