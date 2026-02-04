#pragma once
#include <QString>
namespace cv::autostart {
    bool isEnabled();
    bool setEnabled(bool enabled, const QString &execPath, const QString &iconPath);
} 
