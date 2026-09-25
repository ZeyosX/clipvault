#include "ui/history_model.h"
#include "util/qt_helpers.h"
#include <QDateTime>
#include <QIcon>
namespace cv {
    HistoryModel::HistoryModel(QObject *parent) : QAbstractListModel(parent) {
    }
    int HistoryModel::rowCount(const QModelIndex &parent) const {
        if (parent.isValid()) return 0;
        return static_cast<int>(_items.size());
    }
    QVariant HistoryModel::data(const QModelIndex &index, const int role) const {
        if (!index.isValid()) return {};
        const int r = index.row();
        if (r < 0 || r >= static_cast<int>(_items.size())) return {};
        const auto &[row, thumb] = _items[static_cast<size_t>(r)];
        switch (role) {
            case Qt::DisplayRole: {
                if (row.type == EntryType::Image) {
                    return QString("[Image] %1 bytes").arg(row.imagePng.size());
                }
                auto t = row.text;
                t.replace('\n', ' ');
                if (t.size() > 160) t = t.left(160) + "…";
                return t;
            }
            case Qt::DecorationRole:
                if (row.type == EntryType::Image) return thumb;
                return QIcon::fromTheme("edit-paste");
            case IdRole: return QVariant::fromValue<qint64>(row.id);
            case TypeRole: return static_cast<int>(row.type);
            case TextRole: return row.text;
            case CreatedRole: return QVariant::fromValue<qint64>(row.createdAtMs);
            case PngRole: return row.imagePng;
            case PinnedRole: return row.pinned;
            default:
                return {};
        }
    }
    QHash<int, QByteArray> HistoryModel::roleNames() const {
        auto r = QAbstractListModel::roleNames();
        r[IdRole] = "id";
        r[TypeRole] = "type";
        r[TextRole] = "text";
        r[CreatedRole] = "createdAtMs";
        r[PngRole] = "png";
        r[PinnedRole] = "pinned";
        return r;
    }
    const EntryItem *HistoryModel::itemAt(const int row) const {
        if (row < 0 || row >= static_cast<int>(_items.size())) return nullptr;
        return &_items[static_cast<size_t>(row)];
    }
    void HistoryModel::setItems(std::vector<EntryItem> &&items) {
        beginResetModel();
        _items = std::move(items);
        endResetModel();
    }
    void HistoryModel::reloadFromDb(const HistoryDb &db, const int limit) {
        const auto rows = db.loadLatest(limit);
        std::vector<EntryItem> items;
        items.reserve(rows.size());
        for (const auto &r: rows) {
            EntryItem it;
            it.row = r;
            if (r.type == EntryType::Image && !r.imagePng.isEmpty()) {
                const auto img = qt::pngBytesToImage(r.imagePng);
                it.thumb = qt::makeThumb(img, 48);
            }
            items.push_back(std::move(it));
        }
        setItems(std::move(items));
    }
    void HistoryModel::prependFromDb(const HistoryDb &db, const int limit) {
        reloadFromDb(db, limit);
    }
} 
