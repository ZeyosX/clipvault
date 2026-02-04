#include "qt_helpers.h"

#include <QBuffer>
#include <QImage>
#include <QPixmap>

namespace cv::qt {
    QByteArray imageToPngBytes(const QImage &img) {
        QByteArray bytes;
        QBuffer buf(&bytes);
        buf.open(QIODevice::WriteOnly);
        img.save(&buf, "PNG");
        return bytes;
    }

    QImage pngBytesToImage(const QByteArray &bytes) {
        QImage img;
        img.loadFromData(bytes, "PNG");
        return img;
    }

    QPixmap makeThumb(const QImage &img, const int maxSize) {
        if (img.isNull()) return {};
        return QPixmap::fromImage(img.scaled(maxSize, maxSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
}