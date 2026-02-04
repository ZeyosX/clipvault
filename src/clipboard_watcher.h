#pragma once
#include <QObject>
class QClipboard;
namespace cv {
    class HistoryDb;
    class Settings;
    class ClipboardWatcher : public QObject {
        Q_OBJECT
    public:
        ClipboardWatcher(QClipboard *clipboard, HistoryDb *db, Settings *settings, QObject *parent = nullptr);
        signals:
        void entryAdded();
    private
        slots:
        void onClipboardChanged();
    private:
        bool shouldIgnore(const QString &hash) const;
    private:
        QClipboard *_clipboard = nullptr;
        HistoryDb *_db = nullptr;
        Settings *_settings = nullptr;
        QString _lastHash;
        bool _ignoreNext = false;
    };
} 
