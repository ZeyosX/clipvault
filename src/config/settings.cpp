#include "config/settings.h"
#include "util/paths.h"
#include <QSettings>
namespace cv {
    Settings::Settings(QObject *parent) : QObject(parent) {
    }
    void Settings::load() {
        paths::ensureDirs();
        const QSettings s(paths::settingsPath(), QSettings::IniFormat);
        _d.maxEntries = s.value("history/maxEntries", _d.maxEntries).toInt();
        _d.alwaysOnTop = s.value("ui/alwaysOnTop", _d.alwaysOnTop).toBool();
        _d.startOnLogin = s.value("app/startOnLogin", _d.startOnLogin).toBool();
        _d.autoPaste = s.value("app/autoPaste", _d.autoPaste).toBool();
        _d.hotkey = s.value("input/hotkey", _d.hotkey).toString();
        if (_d.maxEntries < 10) _d.maxEntries = 10;
        if (_d.maxEntries > 50000) _d.maxEntries = 50000;
        emit changed();
    }
    void Settings::save() const {
        paths::ensureDirs();
        QSettings s(paths::settingsPath(), QSettings::IniFormat);
        s.setValue("history/maxEntries", _d.maxEntries);
        s.setValue("ui/alwaysOnTop", _d.alwaysOnTop);
        s.setValue("app/startOnLogin", _d.startOnLogin);
        s.setValue("app/autoPaste", _d.autoPaste);
        s.setValue("input/hotkey", _d.hotkey);
        s.sync();
    }
} 
