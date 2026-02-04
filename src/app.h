#pragma once
#include <QObject>
namespace cv {
    class Settings;
    class HistoryDb;
    class ClipboardWatcher;
    class HistoryPopup;
    class Tray;
    class HotkeyManager;
    class PortalPaster;
    class X11Paster;
    class App : public QObject {
        Q_OBJECT
    public:
        explicit App(QObject *parent = nullptr);
        ~App();
        bool init();
        void runPostInit() const;
    private
        slots:
        void showHistory() const;
        void showSettings() const;
        void onSettingsChanged();
        void clearHistory() const;
    private:
        void ensureAutostart() const;
    private:
        Settings *_settings = nullptr;
        HistoryDb *_db = nullptr;
        ClipboardWatcher *_watcher = nullptr;
        HistoryPopup *_popup = nullptr;
        Tray *_tray = nullptr;
        HotkeyManager *_hotkey = nullptr;
        PortalPaster *_portalPaster = nullptr;
        X11Paster *_x11Paster = nullptr;
    };
} 
