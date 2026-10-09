#include "theme_watcher.h"

#include "core/theme_source.h"

#include <QDir>
#include <QFileInfo>
#include <QLockFile>
#include <QTimer>
#include <exception>
#include <stdexcept>
#include <utility>

namespace tetris {
namespace {

bool isSameFile(const QString &left, const QString &right) {
    const auto absoluteLeft = QDir::cleanPath(QFileInfo(left).absoluteFilePath());
    const auto absoluteRight = QDir::cleanPath(QFileInfo(right).absoluteFilePath());
    if (absoluteLeft == absoluteRight)
        return true;
    const auto canonicalLeft = QFileInfo(left).canonicalFilePath();
    const auto canonicalRight = QFileInfo(right).canonicalFilePath();
    return !canonicalLeft.isEmpty() && canonicalLeft == canonicalRight;
}

void rejectSelfFollow(const Theme &theme, const QString &outputPath) {
    if (theme.sourceKind.isEmpty())
        return;
    for (const auto &sourcePath : sourceWatchPaths(theme)) {
        if (isSameFile(sourcePath, outputPath))
            throw std::invalid_argument("A theme cannot follow its own output file");
    }
}

}

ThemeWatcher::ThemeWatcher(QString path, Theme initial, QObject *parent)
    : QObject(parent), path_(QDir::cleanPath(QFileInfo(path).absoluteFilePath())),
      theme_(std::move(initial)), configuredTheme_(theme_) {
    debounce_.setSingleShot(true);
    debounce_.setInterval(45);
    connect(&watcher_, &QFileSystemWatcher::fileChanged, this, [this] { debounce_.start(); });
    connect(&watcher_, &QFileSystemWatcher::directoryChanged, this, [this] { debounce_.start(); });
    connect(&debounce_, &QTimer::timeout, this, &ThemeWatcher::reload);
    updateWatches();
    QTimer::singleShot(0, this, &ThemeWatcher::reload);
}

const Theme &ThemeWatcher::theme() const {
    return theme_;
}

const QString &ThemeWatcher::path() const {
    return path_;
}

void ThemeWatcher::updateWatches() {
    QStringList wanted;
    const auto configDirectory = QFileInfo(path_).absolutePath();
    if (QFileInfo(configDirectory).isDir())
        wanted.append(configDirectory);
    if (QFileInfo::exists(path_) || QFileInfo(path_).isSymLink())
        wanted.append(path_);
    wanted.append(sourceWatchPaths(configuredTheme_));
    wanted.removeDuplicates();

    const auto current = watcher_.files() + watcher_.directories();
    for (const auto &path : current) {
        if (!wanted.contains(path) || QFileInfo(path).isSymLink())
            watcher_.removePath(path);
    }
    const auto refreshed = watcher_.files() + watcher_.directories();
    for (const auto &path : wanted) {
        if (!refreshed.contains(path) && (QFileInfo::exists(path) || QFileInfo(path).isSymLink()))
            watcher_.addPath(path);
    }
}

void ThemeWatcher::reload() {
    Theme resolved;
    bool changed = false;
    QString errorMessage;
    const auto lockPath = themeLockPath(path_);
    if (!QDir().mkpath(QFileInfo(lockPath).absolutePath())) {
        emit rejected("Cannot create theme lock directory");
        return;
    }
    QLockFile lock(lockPath);
    if (!lock.tryLock(0)) {
        debounce_.start();
        return;
    }

    try {
        const auto configured = loadTheme(path_);
        configuredTheme_ = configured;
        rejectSelfFollow(configured, path_);
        resolved = configured.sourceKind.isEmpty() ? configured : refreshSource(configured);
        if (resolved != configured)
            saveTheme(path_, resolved);
        configuredTheme_ = resolved;
        if (resolved != theme_) {
            theme_ = resolved;
            changed = true;
        }
    } catch (const std::exception &error) {
        errorMessage = QString::fromUtf8(error.what());
    }
    lock.unlock();

    if (!errorMessage.isEmpty())
        emit rejected(errorMessage);
    if (changed)
        emit themeChanged(theme_);
    updateWatches();
}

}
