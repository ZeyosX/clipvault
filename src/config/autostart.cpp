#include "config/autostart.h"
#include "util/paths.h"
#include <QFileInfo>
#include <QTextStream>
namespace cv::autostart {
    static QString desktopFileContents(const QString &execPath, const QString &iconPath) {
        const auto exec = QString("\"%1\" --hidden").arg(execPath);
        return QString(
            "[Desktop Entry]\n"
            "Type=Application\n"
            "Version=1.0\n"
            "Name=ClipVault\n"
            "Comment=Clipboard history (text + images)\n"
            "Exec=%1\n"
            "Icon=%2\n"
            "Terminal=false\n"
            "X-GNOME-Autostart-enabled=true\n"
            "Categories=Utility;\n"
        ).arg(exec, iconPath);
    }
    bool isEnabled() {
        const auto p = paths::autostartDesktopPath();
        return QFileInfo::exists(p);
    }
    bool setEnabled(const bool enabled, const QString &execPath, const QString &iconPath) {
        paths::ensureDirs();
        const auto p = paths::autostartDesktopPath();
        if (!enabled) {
            if (QFileInfo::exists(p)) return QFile::remove(p);
            return true;
        }
        QFile f(p);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) return false;
        QTextStream out(&f);
        out << desktopFileContents(execPath, iconPath);
        f.close();
        return true;
    }
} 
