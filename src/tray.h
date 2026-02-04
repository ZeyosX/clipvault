#pragma once
#include <QObject>
#include <QSystemTrayIcon>
class QMenu;
namespace cv {
    class Settings;
    class Tray : public QObject {
        Q_OBJECT
    public:
        explicit Tray(Settings *settings, QObject *parent = nullptr);
        bool init();
        void showMessage(const QString &title, const QString &message) const;
        signals:
        void showHistoryRequested();
        void settingsRequested();
        void quitRequested();
        void clearHistoryRequested();
    private
        slots:
        void onActivated(QSystemTrayIcon::ActivationReason reason);
    private:
        Settings *_settings = nullptr;
        QSystemTrayIcon *_tray = nullptr;
        QMenu *_menu = nullptr;
    };
} 
