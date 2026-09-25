#pragma once
#include "history/history_db.h"
#include <QAbstractListModel>
#include <QPixmap>
#include <vector>
namespace cv {
    struct EntryItem {
        EntryRow row;
        QPixmap thumb; 
    };
    class HistoryModel : public QAbstractListModel {
        Q_OBJECT
    public:
        enum Roles {
            IdRole = Qt::UserRole + 1,
            TypeRole,
            TextRole,
            CreatedRole,
            PngRole,
            PinnedRole,
        };
        explicit HistoryModel(QObject *parent = nullptr);
        [[nodiscard]] int rowCount(const QModelIndex &parent) const override;
        [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
        [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
        [[nodiscard]] const EntryItem *itemAt(int row) const;
        void reloadFromDb(const HistoryDb &db, int limit);
        void prependFromDb(const HistoryDb &db, int limit); 
    private:
        void setItems(std::vector<EntryItem> &&items);
        std::vector<EntryItem> _items;
    };
} 
