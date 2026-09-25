#pragma once
#include <QWidget>
#include <QPoint>
class QClipboard;
class QLineEdit;
class QListView;
class QSortFilterProxyModel;
class QLabel;
class QTextEdit;
class QStackedWidget;
class QPushButton;
namespace cv {
    class HistoryDb;
    class Settings;
    class HistoryModel;
    class PortalPaster;
    class X11Paster;
    class HistoryPopup : public QWidget {
        Q_OBJECT
    public:
        HistoryPopup(QClipboard *clipboard,
                     HistoryDb *db,
                     Settings *settings,
                     PortalPaster *portalPaster,
                     X11Paster *x11Paster,
                     QWidget *parent = nullptr);
        void openPopup();
        void refresh();
        bool eventFilter(QObject *obj, QEvent *e) override;
        signals:
        void requestedSettings();
    protected:
        bool event(QEvent *e) override;
    private
        slots:
        void onFilterChanged(const QString &) const;
        void onConfirm();
        void onCopy() const;
        void onTogglePin() const;
        void onDeleteSelected() const;
        void updatePreview() const;
        void showContextMenu(const QPoint &pos);
    private:
        void applyAlwaysOnTop();
        void selectFirstItem() const;
        void selectItem(qint64 id) const;
        void updateViewState() const;
        [[nodiscard]] bool setClipboardFromSelection() const;
        void triggerPaste() const;
        QClipboard *_clipboard = nullptr;
        HistoryDb *_db = nullptr;
        Settings *_settings = nullptr;
        PortalPaster *_portalPaster = nullptr;
        X11Paster *_x11Paster = nullptr;
        HistoryModel *_model = nullptr;
        QSortFilterProxyModel *_proxy = nullptr;
        QLineEdit *_filter = nullptr;
        QListView *_list = nullptr;
        QWidget *_header = nullptr;
        QLabel *_hint = nullptr;
        QLabel *_emptyState = nullptr;
        QLabel *_previewEmpty = nullptr;
        QLabel *_status = nullptr;
        QLabel *_previewImage = nullptr;
        QTextEdit *_previewText = nullptr;
        QLabel *_previewMeta = nullptr;
        QStackedWidget *_listStack = nullptr;
        QStackedWidget *_previewStack = nullptr;
        QPushButton *_primaryButton = nullptr;
        QPushButton *_copyButton = nullptr;
        QPushButton *_pinButton = nullptr;
        QPushButton *_deleteButton = nullptr;
        bool _dragging = false;
        QPoint _dragOffset;
    };
} 
