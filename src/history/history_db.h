#pragma once
#include <QMutex>
#include <QString>
#include <vector>
struct sqlite3;
namespace cv {
    enum class EntryType : int {
        Text = 0,
        Image = 1,
    };
    struct EntryRow {
        qint64 id = 0;
        qint64 createdAtMs = 0;
        EntryType type = EntryType::Text;
        QString text;
        QByteArray imagePng; 
        QString hash; 
        bool pinned = false;
    };
    class HistoryDb {
    public:
        HistoryDb();
        ~HistoryDb();
        bool open();
        void close();
        bool addText(const QString &text);
        bool addImagePng(const QByteArray &pngBytes, int width, int height);
        std::vector<EntryRow> loadLatest(int limit) const;
        void pruneToMax(int maxEntries) const;
        bool setPinned(qint64 id, bool pinned) const;
        bool deleteByIds(const std::vector<qint64> &ids) const;
        bool clearAll() const;
        QString lastHash() const;
    private:
        bool exec(const QString &sql) const;
        bool prepareSchema() const;
        mutable QMutex _mx;
        sqlite3 *_db = nullptr;
    };
} 
