#pragma once
#include <QByteArray>
#include <QImage>
#include <QPixmap>

namespace cv::qt {
    QByteArray imageToPngBytes(const QImage &img);

    QImage pngBytesToImage(const QByteArray &bytes);

    QPixmap makeThumb(const QImage &img, int maxSize = 64);
}