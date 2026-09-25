#pragma once
#include <QVariantMap>
namespace cv {
    class PortalRequest : public QObject {
        Q_OBJECT
    public:
        explicit PortalRequest(QString requestPath, QObject *parent = nullptr);
        [[nodiscard]] QString path() const { return _path; }
        signals:
        void responded(uint response, const QVariantMap &results);
    private
        slots:
        void onResponse(uint response, const QVariantMap &results);
    private:
        QString _path;
    };
} 
