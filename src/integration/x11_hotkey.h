#pragma once
#include <QObject>
#include <QString>
class QSocketNotifier;
namespace cv {
    class X11Hotkey : public QObject {
        Q_OBJECT
    public:
        explicit X11Hotkey(QObject *parent = nullptr);
        ~X11Hotkey() override;
        bool start(const QString &hotkey); 
        void stop();
        signals:
        void activated();
        void status(const QString &msg);
    private
        slots:
        void onX11Readable();
    private:
        void cleanup();
        void *_dpy = nullptr;
        unsigned long _root = 0;
        unsigned int _keycode = 0;
        unsigned int _mods = 0;
        QSocketNotifier *_notifier = nullptr;
    };
} 
