#pragma once

#include "core/theme.h"

#include <QFileSystemWatcher>
#include <QObject>
#include <QTimer>

namespace tetris {

class ThemeWatcher : public QObject {
    Q_OBJECT
public:
    ThemeWatcher(QString path, Theme initial, QObject *parent = nullptr);

    const Theme &theme() const;
    const QString &path() const;
    void reload();

signals:
    void themeChanged(const tetris::Theme &theme);
    void rejected(const QString &message);

private:
    QString path_;
    Theme theme_;
    Theme configuredTheme_;
    QFileSystemWatcher watcher_;
    QTimer debounce_;

    void updateWatches();
};

}

Q_DECLARE_METATYPE(tetris::Theme)
