#pragma once
#include <QObject>
namespace cv {
    class X11Paster : public QObject {
        Q_OBJECT
    public:
        explicit X11Paster(QObject *parent = nullptr);
        ~X11Paster() override;
        [[nodiscard]] bool isAvailable() const;
        void pasteCtrlV() const;
        signals:
        void status(const QString &msg);
    private:
        void *_dpy = nullptr;
    };
} 
