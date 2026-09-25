#include "integration/portal_request.h"
#include <QDBusConnection>
#include <utility>
namespace cv {
    PortalRequest::PortalRequest(QString requestPath, QObject *parent)
        : QObject(parent), _path(std::move(requestPath)) {
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
