#include "integration/hotkey_manager.h"
#include "integration/portal_global_shortcuts.h"
#include "integration/x11_hotkey.h"
namespace cv {
    HotkeyManager::HotkeyManager(QObject *parent) : QObject(parent) {
        _portal = new PortalGlobalShortcuts(this);
        _x11 = new X11Hotkey(this);
        connect(_portal, &PortalGlobalShortcuts::activated, this, &HotkeyManager::activated);
        connect(_portal, &PortalGlobalShortcuts::ready, this, [this](const bool ok, const QString &msg) {
            emit status(msg);
            if (!ok && !_fallbackStarted) {
                _fallbackStarted = true;
                _x11->start(_hotkey);
            }
        });
        connect(_x11, &X11Hotkey::activated, this, &HotkeyManager::activated);
        connect(_x11, &X11Hotkey::status, this, &HotkeyManager::status);
    }
    void HotkeyManager::start(const QString &hotkey) {
        _hotkey = hotkey.trimmed().isEmpty() ? QString("Ctrl+F1") : hotkey.trimmed();
        if (PortalGlobalShortcuts::isLikelyAvailable()) {
            _portal->start(_hotkey);
            return;
        }
        _fallbackStarted = true;
        _x11->start(_hotkey);
    }
    void HotkeyManager::rebind(const QString &hotkey) {
        const auto hk = hotkey.trimmed().isEmpty() ? QString("Ctrl+F1") : hotkey.trimmed();
        if (hk == _hotkey) return;
        _hotkey = hk;
        if (_portal) _portal->stop();
        if (_x11) _x11->stop();
        _fallbackStarted = false;
        start(_hotkey);
    }
} 
