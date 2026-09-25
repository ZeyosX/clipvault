#pragma once
#include <QString>

namespace cv::crypto {
    QString sha256Hex(const QByteArray &data);
}
