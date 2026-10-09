#pragma once

#include <QStringList>
#include <cstdint>
#include <optional>

namespace tetris {

struct Options {
    QString configDirectory;
    QStringList commands;
    std::optional<QString> mode;
    std::optional<std::uint32_t> seed;
    bool help = false;
    bool version = false;
    bool dryRun = false;
};

Options parseOptions(const QStringList &arguments);
void printHelp();
int runThemeCommand(const Options &options);

}
