#include "ui/tray.h"
#include <QAction>
#include <QMenu>
#include <QSystemTrayIcon>
#include <QIcon>
namespace cv {
    Tray::Tray(Settings *settings, QObject *parent) : QObject(parent), _settings(settings) {
    }
    bool Tray::init() {
        if (!QSystemTrayIcon::isSystemTrayAvailable()) {
            return false;
        }
        _tray = new QSystemTrayIcon(this);
        _tray->setIcon(QIcon::fromTheme("edit-paste", QIcon(":/assets/clipvault.svg")));
        _tray->setToolTip("ClipVault");
        _menu = new QMenu();
        const auto *actShow = _menu->addAction("Show History (Ctrl+F1)");
        const auto *actSettings = _menu->addAction("Settings");
        const auto *actClear = _menu->addAction("Clear History");
        _menu->addSeparator();
        const auto *actQuit = _menu->addAction("Quit");
        connect(actShow, &QAction::triggered, this, &Tray::showHistoryRequested);
        connect(actSettings, &QAction::triggered, this, &Tray::settingsRequested);
        connect(actClear, &QAction::triggered, this, &Tray::clearHistoryRequested);
        connect(actQuit, &QAction::triggered, this, &Tray::quitRequested);
        _tray->setContextMenu(_menu);
        connect(_tray, &QSystemTrayIcon::activated, this, &Tray::onActivated);
        _tray->show();
        return true;
    }
    void Tray::showMessage(const QString &title, const QString &message) const {
        if (_tray) _tray->showMessage(title, message);
    }
    void Tray::onActivated(const QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger) {
            emit showHistoryRequested();
        }
    }
} 
