#include "crypto.h"

#include <QCryptographicHash>

namespace cv::crypto {
    QString sha256Hex(const QByteArray &data) {
        const auto h = QCryptographicHash::hash(data, QCryptographicHash::Sha256);
        return QString::fromLatin1(h.toHex());
    }
}
