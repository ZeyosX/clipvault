#pragma once
#include <QByteArray>
#include <QString>

namespace cv::crypto {
    QString sha256Hex(const QByteArray &data);
}
