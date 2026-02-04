#include "portal_paster.h"
#include "portal_request.h"
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QUuid>
#include <QTimer>
namespace cv {
    PortalPaster::PortalPaster(QObject *parent) : QObject(parent) {
    }
    bool PortalPaster::isLikelyAvailable() const {
        return QDBusConnection::sessionBus().interface()->isServiceRegistered("org.freedesktop.portal.Desktop");
    }
    void PortalPaster::pasteCtrlV() {
        _pendingPaste = true;
        ensureSession();
        if (_ready) {
            QTimer::singleShot(50, this, &PortalPaster::sendCtrlVNow);
        }
    }
    void PortalPaster::ensureSession() {
        if (_ready || _creating) return;
        _creating = true;
        QDBusInterface iface(
            "org.freedesktop.portal.Desktop",
            "/org/freedesktop/portal/desktop",
            "org.freedesktop.portal.RemoteDesktop",
            QDBusConnection::sessionBus()
        );
        QVariantMap options;
        options["handle_token"] = QString("cv_rd_%1").arg(QUuid::createUuid().toString(QUuid::Id128));
        options["session_handle_token"] = QString("cv_rds_%1").arg(QUuid::createUuid().toString(QUuid::Id128));
        const auto m = iface.call("CreateSession", options);
        if (m.type() == QDBusMessage::ErrorMessage) {
            _creating = false;
            emit status("RemoteDesktop CreateSession failed: " + m.errorMessage());
            return;
        }
        const auto requestPath = m.arguments().value(0).value<QDBusObjectPath>().path();
        const auto *req = new PortalRequest(requestPath, this);
        connect(req, &PortalRequest::responded, this, &PortalPaster::onCreateSessionResponse);
    }
    void PortalPaster::onCreateSessionResponse(const uint response, const QVariantMap &results) {
        if (response != 0) {
            _creating = false;
            emit status("RemoteDesktop denied");
            return;
        }
        const auto sh = results.value("session_handle").toString();
        if (sh.isEmpty()) {
            _creating = false;
            emit status("RemoteDesktop missing session_handle");
            return;
        }
        _sessionHandle = QDBusObjectPath(sh);
        selectDevices();
    }
    void PortalPaster::selectDevices() {
        QDBusInterface iface(
            "org.freedesktop.portal.Desktop",
            "/org/freedesktop/portal/desktop",
            "org.freedesktop.portal.RemoteDesktop",
            QDBusConnection::sessionBus()
        );
        QVariantMap options;
        options["handle_token"] = QString("cv_rdd_%1").arg(QUuid::createUuid().toString(QUuid::Id128));
        options["types"] = static_cast<uint>(1); 
        options["persist_mode"] = static_cast<uint>(1);
        const auto m = iface.call("SelectDevices", QVariant::fromValue(_sessionHandle), options);
        if (m.type() == QDBusMessage::ErrorMessage) {
            _creating = false;
            emit status("RemoteDesktop SelectDevices failed: " + m.errorMessage());
            return;
        }
        const auto requestPath = m.arguments().value(0).value<QDBusObjectPath>().path();
        const auto *req = new PortalRequest(requestPath, this);
        connect(req, &PortalRequest::responded, this, &PortalPaster::onSelectDevicesResponse);
    }
    void PortalPaster::onSelectDevicesResponse(const uint response, const QVariantMap &) {
        if (response != 0) {
            _creating = false;
            emit status("RemoteDesktop SelectDevices denied");
            return;
        }
        startSession();
    }
    void PortalPaster::startSession() {
        QDBusInterface iface(
            "org.freedesktop.portal.Desktop",
            "/org/freedesktop/portal/desktop",
            "org.freedesktop.portal.RemoteDesktop",
            QDBusConnection::sessionBus()
        );
        QVariantMap options;
        options["handle_token"] = QString("cv_rdst_%1").arg(QUuid::createUuid().toString(QUuid::Id128));
        const auto m = iface.call("Start", QVariant::fromValue(_sessionHandle), QString(""), options);
        if (m.type() == QDBusMessage::ErrorMessage) {
            _creating = false;
            emit status("RemoteDesktop Start failed: " + m.errorMessage());
            return;
        }
        const auto requestPath = m.arguments().value(0).value<QDBusObjectPath>().path();
        const auto *req = new PortalRequest(requestPath, this);
        connect(req, &PortalRequest::responded, this, &PortalPaster::onStartResponse);
    }
    void PortalPaster::onStartResponse(const uint response, const QVariantMap &) {
        _creating = false;
        if (response != 0) {
            emit status("RemoteDesktop Start denied");
            return;
        }
        _ready = true;
        emit status("RemoteDesktop ready (keyboard allowed)");
        if (_pendingPaste) {
            QTimer::singleShot(50, this, &PortalPaster::sendCtrlVNow);
        }
    }
    void PortalPaster::sendCtrlVNow() {
        if (!_ready) return;
        _pendingPaste = false;
        constexpr int CTRL_L = 0xffe3;
        constexpr int KEY_V = 0x0076;
        QDBusInterface iface(
            "org.freedesktop.portal.Desktop",
            "/org/freedesktop/portal/desktop",
            "org.freedesktop.portal.RemoteDesktop",
            QDBusConnection::sessionBus()
        );
        QVariantMap options;
        iface.call("NotifyKeyboardKeysym", QVariant::fromValue(_sessionHandle), options, CTRL_L, static_cast<uint>(1));
        iface.call("NotifyKeyboardKeysym", QVariant::fromValue(_sessionHandle), options, KEY_V, static_cast<uint>(1));
        iface.call("NotifyKeyboardKeysym", QVariant::fromValue(_sessionHandle), options, KEY_V, static_cast<uint>(0));
        iface.call("NotifyKeyboardKeysym", QVariant::fromValue(_sessionHandle), options, CTRL_L, static_cast<uint>(0));
    }
} 
