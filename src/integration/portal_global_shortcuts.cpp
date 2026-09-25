#include "integration/portal_global_shortcuts.h"
#include "integration/portal_request.h"
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusArgument>
#include <QDBusMetaType>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusReply>
#include <QUuid>
struct CvShortcut {
    QString id;
    QVariantMap props;
};
Q_DECLARE_METATYPE(CvShortcut)
typedef QList<CvShortcut> CvShortcutList;
Q_DECLARE_METATYPE(CvShortcutList)
static QDBusArgument &operator<<(QDBusArgument &arg, const CvShortcut &s) {
    arg.beginStructure();
    arg << s.id << s.props;
    arg.endStructure();
    return arg;
}
static const QDBusArgument &operator>>(const QDBusArgument &arg, CvShortcut &s) {
    arg.beginStructure();
    arg >> s.id >> s.props;
    arg.endStructure();
    return arg;
}
static QDBusArgument &operator<<(QDBusArgument &arg, const CvShortcutList &list) {
    arg.beginArray(qMetaTypeId<CvShortcut>());
    for (const auto &s: list) arg << s;
    arg.endArray();
    return arg;
}
static const QDBusArgument &operator>>(const QDBusArgument &arg, CvShortcutList &list) {
    list.clear();
    arg.beginArray();
    while (!arg.atEnd()) {
        CvShortcut s;
        arg >> s;
        list.push_back(s);
    }
    arg.endArray();
    return arg;
}
static void registerTypes() {
    static bool once = false;
    if (once) return;
    once = true;
    qRegisterMetaType<CvShortcut>("CvShortcut");
    qRegisterMetaType<CvShortcutList>("CvShortcutList");
    qDBusRegisterMetaType<CvShortcut>();
    qDBusRegisterMetaType<CvShortcutList>();
}
namespace cv {
    PortalGlobalShortcuts::PortalGlobalShortcuts(QObject *parent) : QObject(parent) {
        registerTypes();
    }
    bool PortalGlobalShortcuts::isLikelyAvailable() const {
        return QDBusConnection::sessionBus().interface()->isServiceRegistered("org.freedesktop.portal.Desktop");
    }
    void PortalGlobalShortcuts::start(const QString &trigger) {
        if (_started) return;
        _trigger = trigger.trimmed().isEmpty() ? QString("Ctrl+F1") : trigger.trimmed();
        _started = true;
        QDBusConnection::sessionBus().connect(
            "org.freedesktop.portal.Desktop",
            "/org/freedesktop/portal/desktop",
            "org.freedesktop.portal.GlobalShortcuts",
            "Activated",
            this,
            SLOT(onPortalActivated(QDBusObjectPath, QString, qulonglong, QVariantMap))
        );
        QDBusInterface iface(
            "org.freedesktop.portal.Desktop",
            "/org/freedesktop/portal/desktop",
            "org.freedesktop.portal.GlobalShortcuts",
            QDBusConnection::sessionBus()
        );
        QVariantMap options;
        options["handle_token"] = QString("cv_%1").arg(QUuid::createUuid().toString(QUuid::Id128));
        options["session_handle_token"] = QString("cv_s_%1").arg(QUuid::createUuid().toString(QUuid::Id128));
        const auto reply = iface.call("CreateSession", options);
        if (reply.type() == QDBusMessage::ErrorMessage) {
            emit ready(false, reply.errorMessage());
            return;
        }
        const auto requestPath = reply.arguments().value(0).value<QDBusObjectPath>().path();
        const auto *req = new PortalRequest(requestPath, this);
        connect(req, &PortalRequest::responded, this, &PortalGlobalShortcuts::onCreateSessionResponse);
    }
    void PortalGlobalShortcuts::onCreateSessionResponse(const uint response, const QVariantMap &results) {
        if (response != 0) {
            emit ready(false, "GlobalShortcuts: user denied or failed");
            return;
        }
        const auto sh = results.value("session_handle").toString();
        if (sh.isEmpty()) {
            emit ready(false, "GlobalShortcuts: missing session_handle");
            return;
        }
        _sessionHandle = QDBusObjectPath(sh);
        bindShortcut();
    }
    void PortalGlobalShortcuts::bindShortcut() {
        if (_sessionHandle.path().isEmpty()) {
            emit ready(false, "GlobalShortcuts: no session handle");
            return;
        }
        QDBusInterface iface(
            "org.freedesktop.portal.Desktop",
            "/org/freedesktop/portal/desktop",
            "org.freedesktop.portal.GlobalShortcuts",
            QDBusConnection::sessionBus()
        );
        CvShortcutList shortcuts;
        CvShortcut s;
        s.id = "toggle_history";
        s.props["description"] = "Open clipboard history";
        s.props["preferred_trigger"] = _trigger;
        shortcuts.push_back(s);
        QVariantMap options;
        options["handle_token"] = QString("cv_bind_%1").arg(QUuid::createUuid().toString(QUuid::Id128));
        const QDBusMessage m = iface.call("BindShortcuts",
                                    QVariant::fromValue(_sessionHandle),
                                    QVariant::fromValue(shortcuts),
                                    QString(""),
                                    options);
        if (m.type() == QDBusMessage::ErrorMessage) {
            emit ready(false, m.errorMessage());
            return;
        }
        const auto requestPath = m.arguments().value(0).value<QDBusObjectPath>().path();
        const auto *req = new PortalRequest(requestPath, this);
        connect(req, &PortalRequest::responded, this, &PortalGlobalShortcuts::onBindResponse);
    }
    void PortalGlobalShortcuts::onBindResponse(const uint response, const QVariantMap &) {
        if (response != 0) {
            emit ready(false, "GlobalShortcuts: bind denied");
            return;
        }
        emit ready(true, QString("GlobalShortcuts ready (%1)").arg(_trigger));
    }
    void PortalGlobalShortcuts::onPortalActivated(const QDBusObjectPath &session, const QString &shortcutId, qulonglong,
                                                  const QVariantMap &) {
        if (session.path() != _sessionHandle.path()) return;
        if (shortcutId == "toggle_history") emit activated();
    }
    void PortalGlobalShortcuts::stop() {
        if (!_started) return;
        _started = false;
        if (!_sessionHandle.path().isEmpty()) {
            QDBusInterface sess(
                "org.freedesktop.portal.Desktop",
                _sessionHandle.path(),
                "org.freedesktop.portal.Session",
                QDBusConnection::sessionBus()
            );
            sess.call("Close");
            _sessionHandle = QDBusObjectPath();
        }
        QDBusConnection::sessionBus().disconnect(
            "org.freedesktop.portal.Desktop",
            "/org/freedesktop/portal/desktop",
            "org.freedesktop.portal.GlobalShortcuts",
            "Activated",
            this,
            SLOT(onPortalActivated(QDBusObjectPath, QString, qulonglong, QVariantMap))
        );
    }
} 
