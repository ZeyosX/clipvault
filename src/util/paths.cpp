#include "paths.h"

#include <QDir>
#include <QStandardPaths>
#include <QFileInfo>

namespace cv::paths {
    QString configDir() {
        auto dir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
        if (dir.isEmpty()) dir = QDir::homePath() + "/.config";
        return dir + "/clipvault";
    }

    QString dataDir() {
        auto dir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
        if (dir.isEmpty()) dir = QDir::homePath() + "/.local/share/clipvault";
        return dir;
    }

    QString dbPath() {
        return dataDir() + "/history.db";
    }

    QString settingsPath() {
        return configDir() + "/settings.ini";
    }

    QString autostartDesktopPath() {
        auto cfg = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
        if (cfg.isEmpty()) cfg = QDir::homePath() + "/.config";
        return cfg + "/autostart/clipvault.desktop";
    }

    void ensureDirs() {
        QDir().mkpath(configDir());
        QDir().mkpath(dataDir());
        const auto autostartDir = QFileInfo(autostartDesktopPath()).absolutePath();
        QDir().mkpath(autostartDir);
    }
}