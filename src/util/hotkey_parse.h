#pragma once
#include <QString>

namespace cv::hotkey {
    struct ParsedHotkey {
        bool ok = false;
        unsigned int mods = 0; // X11 modifier mask (ShiftMask/ControlMask/Mod1Mask/Mod4Mask)
        QString key; // e.g. "F1", "V", "A"
        QString error;
    };

    // Parses strings like: "Ctrl+F1", "Ctrl+Alt+V", "Super+Shift+X"
    ParsedHotkey parseForX11(const QString &hotkey);
}