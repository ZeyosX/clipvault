#include "clipboard_watcher.h"
#include "history_db.h"
#include "settings.h"
#include "util/crypto.h"
#include "util/qt_helpers.h"
#include <QClipboard>
#include <QGuiApplication>
#include <QImage>
#include <QMimeData>
namespace cv {
    ClipboardWatcher::ClipboardWatcher(QClipboard *clipboard, HistoryDb *db, Settings *settings, QObject *parent)
        : QObject(parent), _clipboard(clipboard), _db(db), _settings(settings) {
        _lastHash = _db->lastHash();
        connect(_clipboard, &QClipboard::changed, this, &ClipboardWatcher::onClipboardChanged);
    }
    bool ClipboardWatcher::shouldIgnore(const QString &hash) const {
        return hash.isEmpty() || hash == _lastHash;
    }
    void ClipboardWatcher::onClipboardChanged() {
        if (_ignoreNext) {
            _ignoreNext = false;
            return;
        }
        const QMimeData *md = _clipboard->mimeData(QClipboard::Clipboard);
        if (!md) return;
        if (md->hasImage()) {
            const auto img = qvariant_cast<QImage>(md->imageData());
            if (img.isNull()) return;
            const auto png = cv::qt::imageToPngBytes(img);
            const auto hash = cv::crypto::sha256Hex(png);
            if (shouldIgnore(hash)) return;
            if (_db->addImagePng(png, img.width(), img.height())) {
                _db->pruneToMax(_settings->data().maxEntries);
                _lastHash = hash;
                emit entryAdded();
            }
            return;
        }
        if (md->hasText()) {
            auto text = md->text();
            if (text.endsWith('\0')) text.chop(1);
            if (text.trimmed().isEmpty()) return;
            const auto hash = cv::crypto::sha256Hex(text.toUtf8());
            if (shouldIgnore(hash)) return;
            if (_db->addText(text)) {
                _db->pruneToMax(_settings->data().maxEntries);
                _lastHash = hash;
                emit entryAdded();
            }
        }
    }
} 
