#pragma once
#include <QObject>
#include <QString>
namespace cv {
    class PortalGlobalShortcuts;
    class X11Hotkey;
    class HotkeyManager : public QObject {
        Q_OBJECT
    public:
        explicit HotkeyManager(QObject *parent = nullptr);
        void start(const QString &hotkey);
        void rebind(const QString &hotkey);
        signals:
        void activated(); 
        void status(const QString &msg);
    private:
        PortalGlobalShortcuts *_portal = nullptr;
        X11Hotkey *_x11 = nullptr;
        bool _fallbackStarted = false;
        QString _hotkey = "Ctrl+F1";
    };
} 
