#pragma once
#include <QString>

namespace cv::paths {
    QString configDir();

    QString dataDir();

    QString dbPath();

    QString settingsPath();

    QString autostartDesktopPath();

    void ensureDirs();
}