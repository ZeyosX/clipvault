#include "hotkey_parse.h"

#include <QStringList>

#if CLIPVAULT_HAVE_X11
#include <X11/X.h> // ShiftMask/ControlMask/Mod1Mask/Mod4Mask
#endif

namespace cv::hotkey {
    static QString normToken(QString t) {
        t = t.trimmed();
        // allow "Control" -> "Ctrl", "Meta" -> "Super"
        if (t.compare("control", Qt::CaseInsensitive) == 0) return "ctrl";
        if (t.compare("ctrl", Qt::CaseInsensitive) == 0) return "ctrl";
        if (t.compare("alt", Qt::CaseInsensitive) == 0) return "alt";
        if (t.compare("shift", Qt::CaseInsensitive) == 0) return "shift";
        if (t.compare("super", Qt::CaseInsensitive) == 0) return "super";
        if (t.compare("meta", Qt::CaseInsensitive) == 0) return "super";
        if (t.compare("win", Qt::CaseInsensitive) == 0) return "super";
        return t.toLower();
    }

    ParsedHotkey parseForX11(const QString &hotkey) {
        ParsedHotkey out;
        const auto parts = hotkey.split('+', Qt::SkipEmptyParts);
        if (parts.isEmpty()) {
            out.error = "Empty hotkey";
            return out;
        }

        unsigned int mods = 0;
        QString key;

        for (int i = 0; i < parts.size(); ++i) {
            const auto tok = normToken(parts[i]);
            if (const bool last = i == parts.size() - 1; !last) {
#if CLIPVAULT_HAVE_X11
                if (tok == "ctrl") mods |= ControlMask;
                else if (tok == "shift") mods |= ShiftMask;
                else if (tok == "alt") mods |= Mod1Mask;
                else if (tok == "super") mods |= Mod4Mask;
                else {
                    out.error = "Unknown modifier: " + parts[i].trimmed();
                    return out;
                }
#else
                (void) tok;
#endif
            } else {
                key = parts[i].trimmed();
            }
        }

        if (key.isEmpty()) {
            out.error = "Missing key";
            return out;
        }

        out.ok = true;
        out.mods = mods;
        out.key = key;
        return out;
    }
}
