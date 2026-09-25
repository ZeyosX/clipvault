#pragma once
#include <QObject>
#include <QDBusObjectPath>
namespace cv {
    class PortalPaster : public QObject {
        Q_OBJECT
    public:
        explicit PortalPaster(QObject *parent = nullptr);
        [[nodiscard]] static bool isLikelyAvailable();
        void pasteCtrlV(); 
        signals:
        void status(const QString &msg);
    private
        slots:
        void onCreateSessionResponse(uint response, const QVariantMap &results);
        void onSelectDevicesResponse(uint response, const QVariantMap &results);
        void onStartResponse(uint response, const QVariantMap &results);
    private:
        void ensureSession();
        void selectDevices();
        void startSession();
        void sendCtrlVNow();
        QDBusObjectPath _sessionHandle;
        bool _creating = false;
        bool _ready = false;
        bool _pendingPaste = false;
    };
} 
