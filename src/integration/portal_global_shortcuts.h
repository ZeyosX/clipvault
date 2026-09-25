#pragma once
#include <QObject>
#include <QString>
#include <QDBusObjectPath>
#include <QVariantMap>
namespace cv {
    class PortalGlobalShortcuts : public QObject {
        Q_OBJECT
    public:
        explicit PortalGlobalShortcuts(QObject *parent = nullptr);
        [[nodiscard]] static bool isLikelyAvailable();
        void start(const QString &trigger); 
        void stop();
        [[nodiscard]] QDBusObjectPath sessionHandle() const { return _sessionHandle; }
        signals:
        void activated(); 
        void ready(bool ok, const QString &message);
    private
        slots:
        void onPortalActivated(const QDBusObjectPath &session, const QString &shortcutId, qulonglong timestamp,
                               const QVariantMap &options);
        void onCreateSessionResponse(uint response, const QVariantMap &results);
        void onBindResponse(uint response, const QVariantMap &results);
    private:
        void bindShortcut();
        QDBusObjectPath _sessionHandle;
        QString _trigger = "Ctrl+F1";
        bool _started = false;
    };
} 
