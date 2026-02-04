#include "portal_request.h"
#include <QDBusConnection>
namespace cv {
    PortalRequest::PortalRequest(const QString &requestPath, QObject *parent)
        : QObject(parent), _path(requestPath) {
        QDBusConnection::sessionBus().connect(
            "org.freedesktop.portal.Desktop",
            _path,
            "org.freedesktop.portal.Request",
            "Response",
            this,
            SLOT(onResponse(uint, QVariantMap))
        );
    }
    void PortalRequest::onResponse(const uint response, const QVariantMap &results) {
        emit responded(response, results);
        deleteLater();
    }
} 
