#include "history/history_db.h"
#include "util/paths.h"
#include <QDateTime>
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QMutexLocker>
#include <QStandardPaths>
#include <QSet>
#include <sqlite3.h>
namespace cv {
    static QString toUtf8Str(const QString &s) { return s; }
    HistoryDb::HistoryDb() = default;
    HistoryDb::~HistoryDb() { close(); }
    bool HistoryDb::open() {
        QMutexLocker lk(&_mx);
        if (_db) return true;
        cv::paths::ensureDirs();
        const auto path = cv::paths::dbPath();
        if (sqlite3_open(path.toUtf8().constData(), &_db) != SQLITE_OK) {
            _db = nullptr;
            return false;
        }
        sqlite3_exec(_db, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
        sqlite3_exec(_db, "PRAGMA synchronous=NORMAL;", nullptr, nullptr, nullptr);
        sqlite3_exec(_db, "PRAGMA foreign_keys=ON;", nullptr, nullptr, nullptr);
        return prepareSchema();
    }
    void HistoryDb::close() {
        QMutexLocker lk(&_mx);
        if (_db) {
            sqlite3_close(_db);
            _db = nullptr;
        }
    }
    bool HistoryDb::exec(const QString &sql) const {
        char *err = nullptr;
        const auto rc = sqlite3_exec(_db, sql.toUtf8().constData(), nullptr, nullptr, &err);
        if (err) sqlite3_free(err);
        return rc == SQLITE_OK;
    }
    bool HistoryDb::prepareSchema() const {
        const char *schema =
                "CREATE TABLE IF NOT EXISTS entries ("
                "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
                "  created_at_ms INTEGER NOT NULL,"
                "  type INTEGER NOT NULL,"
                "  text TEXT,"
                "  image_png BLOB,"
                "  width INTEGER,"
                "  height INTEGER,"
                "  hash TEXT NOT NULL,"
                "  pinned INTEGER NOT NULL DEFAULT 0"
                ");"
                "CREATE INDEX IF NOT EXISTS idx_entries_created ON entries(created_at_ms DESC);"
                "CREATE INDEX IF NOT EXISTS idx_entries_hash ON entries(hash);"
                "CREATE INDEX IF NOT EXISTS idx_entries_pinned ON entries(pinned DESC, id DESC);";
        if (sqlite3_exec(_db, schema, nullptr, nullptr, nullptr) != SQLITE_OK) return false;
        QSet<QString> cols;
        sqlite3_stmt *st = nullptr;
        if (sqlite3_prepare_v2(_db, "PRAGMA table_info(entries);", -1, &st, nullptr) == SQLITE_OK) {
            while (sqlite3_step(st) == SQLITE_ROW) {
                const auto *name = reinterpret_cast<const char *>(sqlite3_column_text(st, 1));
                if (name) cols.insert(QString::fromUtf8(name));
            }
        }
        if (st) sqlite3_finalize(st);
        if (!cols.contains("pinned")) {
            sqlite3_exec(_db, "ALTER TABLE entries ADD COLUMN pinned INTEGER NOT NULL DEFAULT 0;", nullptr, nullptr,
                         nullptr);
            sqlite3_exec(_db, "CREATE INDEX IF NOT EXISTS idx_entries_pinned ON entries(pinned DESC, id DESC);",
                         nullptr, nullptr, nullptr);
        }
        return true;
    }
    QString HistoryDb::lastHash() const {
        QMutexLocker lk(&_mx);
        if (!_db) return {};
        const char *sql = "SELECT hash FROM entries ORDER BY id DESC LIMIT 1;";
        sqlite3_stmt *st = nullptr;
        QString out;
        if (sqlite3_prepare_v2(_db, sql, -1, &st, nullptr) == SQLITE_OK) {
            if (sqlite3_step(st) == SQLITE_ROW) {
                const auto *t = reinterpret_cast<const char *>(sqlite3_column_text(st, 0));
                if (t) out = QString::fromUtf8(t);
            }
        }
        if (st) sqlite3_finalize(st);
        return out;
    }
    bool HistoryDb::addText(const QString &text) {
        QMutexLocker lk(&_mx);
        if (!_db && !const_cast<HistoryDb *>(this)->open()) return false;
        const auto ts = QDateTime::currentMSecsSinceEpoch();
        const char *sql =
                "INSERT INTO entries(created_at_ms,type,text,image_png,width,height,hash) VALUES(?,?,?,?,?,?,?);";
        sqlite3_stmt *st = nullptr;
        if (sqlite3_prepare_v2(_db, sql, -1, &st, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_int64(st, 1, ts);
        sqlite3_bind_int(st, 2, static_cast<int>(EntryType::Text));
        sqlite3_bind_text(st, 3, text.toUtf8().constData(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_null(st, 4);
        sqlite3_bind_null(st, 5);
        sqlite3_bind_null(st, 6);
        const auto h = QCryptographicHash::hash(text.toUtf8(), QCryptographicHash::Sha256).toHex();
        sqlite3_bind_text(st, 7, reinterpret_cast<const char *>(h.constData()), h.size(), SQLITE_TRANSIENT);
        const auto rc = sqlite3_step(st);
        sqlite3_finalize(st);
        return rc == SQLITE_DONE;
    }
    bool HistoryDb::addImagePng(const QByteArray &pngBytes, const int width, const int height) {
        QMutexLocker lk(&_mx);
        if (!_db && !const_cast<HistoryDb *>(this)->open()) return false;
        const auto ts = QDateTime::currentMSecsSinceEpoch();
        const char *sql =
                "INSERT INTO entries(created_at_ms,type,text,image_png,width,height,hash) VALUES(?,?,?,?,?,?,?);";
        sqlite3_stmt *st = nullptr;
        if (sqlite3_prepare_v2(_db, sql, -1, &st, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_int64(st, 1, ts);
        sqlite3_bind_int(st, 2, static_cast<int>(EntryType::Image));
        sqlite3_bind_null(st, 3);
        sqlite3_bind_blob(st, 4, pngBytes.constData(), pngBytes.size(), SQLITE_TRANSIENT);
        sqlite3_bind_int(st, 5, width);
        sqlite3_bind_int(st, 6, height);
        const auto h = QCryptographicHash::hash(pngBytes, QCryptographicHash::Sha256).toHex();
        sqlite3_bind_text(st, 7, reinterpret_cast<const char *>(h.constData()), h.size(), SQLITE_TRANSIENT);
        const auto rc = sqlite3_step(st);
        sqlite3_finalize(st);
        return rc == SQLITE_DONE;
    }
    std::vector<EntryRow> HistoryDb::loadLatest(const int limit) const {
        QMutexLocker lk(&_mx);
        std::vector<EntryRow> out;
        if (!_db) return out;
        const char *sql =
                "SELECT id, created_at_ms, type, IFNULL(text,''), IFNULL(image_png,x''), IFNULL(hash,''), IFNULL(pinned,0) "
                "FROM entries ORDER BY pinned DESC, id DESC LIMIT ?;";
        sqlite3_stmt *st = nullptr;
        if (sqlite3_prepare_v2(_db, sql, -1, &st, nullptr) != SQLITE_OK) return out;
        sqlite3_bind_int(st, 1, limit);
        while (sqlite3_step(st) == SQLITE_ROW) {
            EntryRow r;
            r.id = sqlite3_column_int64(st, 0);
            r.createdAtMs = sqlite3_column_int64(st, 1);
            r.type = static_cast<EntryType>(sqlite3_column_int(st, 2));
            const auto *t = reinterpret_cast<const char *>(sqlite3_column_text(st, 3));
            if (t) r.text = QString::fromUtf8(t);
            const auto *b = reinterpret_cast<const unsigned char *>(sqlite3_column_blob(st, 4));
            const auto blen = sqlite3_column_bytes(st, 4);
            if (b && blen > 0) r.imagePng = QByteArray(reinterpret_cast<const char *>(b), blen);
            const auto *hh = reinterpret_cast<const char *>(sqlite3_column_text(st, 5));
            if (hh) r.hash = QString::fromUtf8(hh);
            r.pinned = sqlite3_column_int(st, 6) != 0;
            out.push_back(std::move(r));
        }
        sqlite3_finalize(st);
        return out;
    }
    void HistoryDb::pruneToMax(const int maxEntries) const {
        QMutexLocker lk(&_mx);
        if (!_db) return;
        const char *sql =
                "DELETE FROM entries WHERE id NOT IN ("
                "  SELECT id FROM entries ORDER BY id DESC LIMIT ?"
                ");";
        sqlite3_stmt *st = nullptr;
        if (sqlite3_prepare_v2(_db, sql, -1, &st, nullptr) != SQLITE_OK) return;
        sqlite3_bind_int(st, 1, maxEntries);
        sqlite3_step(st);
        sqlite3_finalize(st);
    }
    bool HistoryDb::setPinned(const qint64 id, const bool pinned) const {
        QMutexLocker lk(&_mx);
        if (!_db) return false;
        const char *sql = "UPDATE entries SET pinned=? WHERE id=?;";
        sqlite3_stmt *st = nullptr;
        if (sqlite3_prepare_v2(_db, sql, -1, &st, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_int(st, 1, pinned ? 1 : 0);
        sqlite3_bind_int64(st, 2, id);
        const auto rc = sqlite3_step(st);
        sqlite3_finalize(st);
        return rc == SQLITE_DONE;
    }
    bool HistoryDb::deleteByIds(const std::vector<qint64> &ids) const {
        QMutexLocker lk(&_mx);
        if (!_db) return false;
        if (ids.empty()) return true;
        QString sql = "DELETE FROM entries WHERE id IN (";
        for (size_t i = 0; i < ids.size(); ++i) {
            if (i) sql += ",";
            sql += "?";
        }
        sql += ");";
        sqlite3_stmt *st = nullptr;
        if (sqlite3_prepare_v2(_db, sql.toUtf8().constData(), -1, &st, nullptr) != SQLITE_OK) return false;
        for (size_t i = 0; i < ids.size(); ++i) {
            sqlite3_bind_int64(st, static_cast<int>(i + 1), ids[i]);
        }
        const auto rc = sqlite3_step(st);
        sqlite3_finalize(st);
        return rc == SQLITE_DONE;
    }
    bool HistoryDb::clearAll() const {
        QMutexLocker lk(&_mx);
        if (!_db) return false;
        return sqlite3_exec(_db, "DELETE FROM entries;", nullptr, nullptr, nullptr) == SQLITE_OK;
    }
} 
