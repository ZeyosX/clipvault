#include "ui/history_popup.h"
#include "history/history_db.h"
#include "ui/history_model.h"
#include "config/settings.h"
#include "integration/portal_paster.h"
#include "integration/x11_paster.h"
#include "util/qt_helpers.h"
#include <QClipboard>
#include <QCursor>
#include <QEvent>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QDateTime>
#include <QAction>
#include <QMenu>
#include <QTextEdit>
#include <QSplitter>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QPixmap>
#include <QScreen>
#include <QRegularExpression>
#include <QSortFilterProxyModel>
#include <algorithm>
#include <QVBoxLayout>

namespace cv {
    namespace {
    class HistoryFilterProxy final : public QSortFilterProxyModel {
    public:
        using QSortFilterProxyModel::QSortFilterProxyModel;

    protected:
        [[nodiscard]] bool filterAcceptsRow(const int sourceRow, const QModelIndex &sourceParent) const override {
            if (filterRegularExpression().pattern().isEmpty()) return true;
            const auto idx = sourceModel()->index(sourceRow, 0, sourceParent);
            const auto t = sourceModel()->data(idx, HistoryModel::TextRole).toString();
            return t.contains(filterRegularExpression());
        }
    };
    } // namespace

    HistoryPopup::HistoryPopup(QClipboard *clipboard, HistoryDb *db, Settings *settings, PortalPaster *portalPaster,
                               X11Paster *x11Paster, QWidget *parent)
        : QWidget(parent), _clipboard(clipboard), _db(db), _settings(settings), _portalPaster(portalPaster),
          _x11Paster(x11Paster) {
        setWindowFlags(Qt::FramelessWindowHint | Qt::Tool);
        setAttribute(Qt::WA_ShowWithoutActivating, false);
        auto *root = new QVBoxLayout(this);
        root->setContentsMargins(10, 10, 10, 10);
        root->setSpacing(8);
        _header = new QWidget(this);
        _header->setObjectName("cv_header");
        _header->setCursor(Qt::SizeAllCursor);
        _header->installEventFilter(this);
        auto *top = new QHBoxLayout(_header);
        top->setContentsMargins(0, 0, 0, 0);
        auto *title = new QLabel("ClipVault", _header);
        title->setStyleSheet("font-weight:600; font-size:14px;");
        _hint = new QLabel("Enter: paste | Esc: close", this);
        _hint->setStyleSheet("color: rgba(255,255,255,0.65); font-size:12px;");
        top->addWidget(title);
        top->addStretch(1);
        top->addWidget(_hint);
        root->addWidget(_header);
        _filter = new QLineEdit(this);
        _filter->setPlaceholderText("Filter…");
        root->addWidget(_filter);
        _model = new HistoryModel(this);
        _proxy = new HistoryFilterProxy(this);
        _proxy->setSourceModel(_model);
        _proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
        _filter->setFocusPolicy(Qt::ClickFocus);
        _list = new QListView(this);
        _list->setModel(_proxy);
        _list->setSelectionMode(QAbstractItemView::ExtendedSelection);
        _list->setEditTriggers(QAbstractItemView::NoEditTriggers);
        _list->setUniformItemSizes(true);
        _list->setIconSize(QSize(48, 48));
        _list->setContextMenuPolicy(Qt::CustomContextMenu);
        _list->setStyleSheet(R"(
        QListView {
            background: rgba(20,20,20,0.92);
            border-radius: 10px;
            padding: 6px;
            color: white;
        }
        QListView::item {
            padding: 8px;
            border-radius: 8px;
        }
        QListView::item:selected {
            background: rgba(90,140,255,0.35);
        }
    )");
        _previewMeta = new QLabel(this);
        _previewMeta->setStyleSheet("color: rgba(255,255,255,0.65); font-size:12px;");
        _previewImage = new QLabel(this);
        _previewImage->setMinimumHeight(140);
        _previewImage->setAlignment(Qt::AlignCenter);
        _previewImage->setStyleSheet("background: rgba(255,255,255,0.04); border-radius: 10px;");
        _previewImage->setScaledContents(false);
        _previewText = new QTextEdit(this);
        _previewText->setReadOnly(true);
        _previewText->setStyleSheet(R"(
        QTextEdit {
            background: rgba(255,255,255,0.04);
            border: none;
            border-radius: 10px;
            padding: 10px;
            color: white;
        }
    )");
        auto *previewCol = new QVBoxLayout();
        previewCol->setSpacing(8);
        previewCol->addWidget(_previewMeta);
        previewCol->addWidget(_previewImage);
        previewCol->addWidget(_previewText, 1);
        auto *previewWrap = new QWidget(this);
        previewWrap->setLayout(previewCol);
        auto *splitter = new QSplitter(Qt::Horizontal, this);
        splitter->setChildrenCollapsible(false);
        auto *leftWrap = new QWidget(this);
        auto *leftCol = new QVBoxLayout(leftWrap);
        leftCol->setContentsMargins(0, 0, 0, 0);
        leftCol->setSpacing(8);
        leftCol->addWidget(_list, 1);
        splitter->addWidget(leftWrap);
        splitter->addWidget(previewWrap);
        splitter->setStretchFactor(0, 2);
        splitter->setStretchFactor(1, 3);
        root->addWidget(splitter, 1);
        connect(_list->selectionModel(), &QItemSelectionModel::selectionChanged, this, &HistoryPopup::updatePreview);
        connect(_list->selectionModel(), &QItemSelectionModel::currentChanged, this, &HistoryPopup::updatePreview);
        connect(_list, &QListView::customContextMenuRequested, this, &HistoryPopup::showContextMenu);
        setStyleSheet("background: rgba(12,12,12,0.92); border-radius: 12px; color: white;");
        connect(_filter, &QLineEdit::textChanged, this, &HistoryPopup::onFilterChanged);
        connect(_filter, &QLineEdit::returnPressed, this, &HistoryPopup::onConfirm);
        installEventFilter(this);
        applyAlwaysOnTop();
    }

    void HistoryPopup::applyAlwaysOnTop() {
        if (_settings->data().alwaysOnTop) {
            setWindowFlag(Qt::WindowStaysOnTopHint, true);
        } else {
            setWindowFlag(Qt::WindowStaysOnTopHint, false);
        }
        setWindowFlag(Qt::Tool, true);
        setWindowFlag(Qt::FramelessWindowHint, true);
    }

    void HistoryPopup::openPopup() {
        applyAlwaysOnTop();
        _model->reloadFromDb(*_db, _settings->data().maxEntries);
        const QPoint cursor = QCursor::pos();
        constexpr QSize desired(760, 460);
        resize(desired);
        const QScreen *screen = QGuiApplication::screenAt(cursor);
        if (!screen) screen = QGuiApplication::primaryScreen();
        const QRect avail = screen->availableGeometry();
        QPoint pos = cursor + QPoint(14, 14);
        if (pos.x() + width() > avail.right()) pos.setX(avail.right() - width());
        if (pos.y() + height() > avail.bottom()) pos.setY(avail.bottom() - height());
        if (pos.x() < avail.left()) pos.setX(avail.left());
        if (pos.y() < avail.top()) pos.setY(avail.top());
        move(pos);
        show();
        raise();
        activateWindow();
        _list->setFocus();
        _filter->selectAll();
        if (_proxy->rowCount() > 0) {
            const auto idx = _proxy->index(0, 0);
            _list->setCurrentIndex(idx);
        }
        updatePreview();
    }

    bool HistoryPopup::eventFilter(QObject *obj, QEvent *e) {
        if (obj == _header) {
            if (e->type() == QEvent::MouseButtonPress) {
                if (const auto *me = static_cast<QMouseEvent *>(e); me->button() == Qt::LeftButton) {
                    _dragging = true;
                    _dragOffset = me->globalPosition().toPoint() - frameGeometry().topLeft();
                    return true;
                }
            } else if (e->type() == QEvent::MouseMove) {
                if (_dragging) {
                    const auto *me = dynamic_cast<QMouseEvent *>(e);
                    move(me->globalPosition().toPoint() - _dragOffset);
                    return true;
                }
            } else if (e->type() == QEvent::MouseButtonRelease) {
                if (const auto *me = static_cast<QMouseEvent *>(e); me->button() == Qt::LeftButton) {
                    _dragging = false;
                    return true;
                }
            } else if (e->type() == QEvent::WindowDeactivate) {
                _dragging = false;
            }
        }
        return QWidget::eventFilter(obj, e);
    }

    bool HistoryPopup::event(QEvent *e) {
        if (e->type() == QEvent::WindowDeactivate) {
            close();
            return true;
        }
        if (e->type() == QEvent::KeyPress) {
            const auto *ke = dynamic_cast<QKeyEvent *>(e);
            if (ke->key() == Qt::Key_Escape) {
                close();
                return true;
            }
            if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) {
                onConfirm();
                return true;
            }
            if (ke->key() == Qt::Key_P && ke->modifiers() == Qt::NoModifier) {
                onTogglePin();
                return true;
            }
            if (ke->key() == Qt::Key_Delete) {
                onDeleteSelected();
                return true;
            }
            if (ke->modifiers() == Qt::NoModifier || ke->modifiers() == Qt::ShiftModifier) {
                if (const auto txt = ke->text(); !txt.isEmpty() && txt.at(0).isPrint() && ke->key() != Qt::Key_Space) {
                    _filter->setText(_filter->text() + txt);
                    onFilterChanged(_filter->text());
                    _list->setFocus();
                    return true;
                }
                if (ke->key() == Qt::Key_Space) {
                    _filter->setText(_filter->text() + " ");
                    onFilterChanged(_filter->text());
                    _list->setFocus();
                    return true;
                }
                if (ke->key() == Qt::Key_Backspace) {
                    if (auto t = _filter->text(); !t.isEmpty()) {
                        t.chop(1);
                        _filter->setText(t);
                        onFilterChanged(_filter->text());
                    }
                    _list->setFocus();
                    return true;
                }
            }
            if (ke->key() == Qt::Key_Backspace && (ke->modifiers() & Qt::ControlModifier)) {
                _filter->clear();
                onFilterChanged(_filter->text());
                _list->setFocus();
                return true;
            }
        }
        return QWidget::event(e);
    }

    void HistoryPopup::onFilterChanged(const QString &t) const {
        _proxy->setFilterRegularExpression(
            QRegularExpression(QRegularExpression::escape(t), QRegularExpression::CaseInsensitiveOption));
        if (_proxy->rowCount() > 0) {
            _list->setCurrentIndex(_proxy->index(0, 0));
        }
        updatePreview();
    }

    void HistoryPopup::setClipboardFromSelection() const {
        auto idxs = _list->selectionModel()->selectedIndexes();
        if (idxs.isEmpty()) return;
        std::ranges::sort(idxs,
                          [](const QModelIndex &a, const QModelIndex &b) { return a.row() < b.row(); });
        if (idxs.size() == 1) {
            const auto src = _proxy->mapToSource(idxs[0]);
            const auto *it = _model->itemAt(src.row());
            if (!it) return;
            if (it->row.type == EntryType::Image && !it->row.imagePng.isEmpty()) {
                const QImage img = qt::pngBytesToImage(it->row.imagePng);
                _clipboard->setImage(img, QClipboard::Clipboard);
                return;
            }

            _clipboard->setText(it->row.text, QClipboard::Clipboard);
            return;
        }
        QStringList parts;
        for (const auto &px: idxs) {
            const auto src = _proxy->mapToSource(px);
            const auto *it = _model->itemAt(src.row());
            if (!it) continue;
            if (it->row.type == EntryType::Text) parts << it->row.text;
        }
        _clipboard->setText(parts.join("\n"), QClipboard::Clipboard);
    }

    void HistoryPopup::triggerPaste() const {
        if (!_settings->data().autoPaste) return;
        if (QGuiApplication::platformName() == "wayland") {
            if (_portalPaster && PortalPaster::isLikelyAvailable()) {
                _portalPaster->pasteCtrlV();
            }
            return;
        }
        if (_x11Paster && _x11Paster->isAvailable()) {
            _x11Paster->pasteCtrlV();
        }
    }

    void HistoryPopup::onConfirm() {
        setClipboardFromSelection();
        close();
        triggerPaste();
    }

    void HistoryPopup::onTogglePin() const {
        const auto idx = _list->currentIndex();
        if (!idx.isValid()) return;
        auto selected = _list->selectionModel()->selectedIndexes();
        if (selected.isEmpty()) selected = {idx};
        const auto src0 = _proxy->mapToSource(selected[0]);
        const auto *it0 = _model->itemAt(src0.row());
        if (!it0) return;
        const bool newPinned = !it0->row.pinned;
        std::vector<qint64> ids;
        ids.reserve(selected.size());
        for (const auto &px: selected) {
            const auto src = _proxy->mapToSource(px);
            const auto *it = _model->itemAt(src.row());
            if (!it) continue;
            ids.push_back(it->row.id);
        }
        for (const auto id: ids) _db->setPinned(id, newPinned);
        _model->reloadFromDb(*_db, _settings->data().maxEntries);
        updatePreview();
    }

    void HistoryPopup::onDeleteSelected() const {
        auto selected = _list->selectionModel()->selectedIndexes();
        if (selected.isEmpty()) return;
        std::ranges::sort(selected, [](const QModelIndex &a, const QModelIndex &b) {
            return a.row() < b.row();
        });
        std::vector<qint64> ids;
        ids.reserve(selected.size());
        for (const auto &px: selected) {
            const auto src = _proxy->mapToSource(px);
            const auto *it = _model->itemAt(src.row());
            if (!it) continue;
            ids.push_back(it->row.id);
        }
        _db->deleteByIds(ids);
        _model->reloadFromDb(*_db, _settings->data().maxEntries);
        if (_proxy->rowCount() > 0) {
            _list->setCurrentIndex(_proxy->index(0, 0));
        }
        updatePreview();
    }

    void HistoryPopup::updatePreview() const {
        QModelIndex idx = _list->currentIndex();
        if (!idx.isValid()) {
            if (const auto sel = _list->selectionModel()->selectedIndexes(); !sel.isEmpty()) idx = sel[0];
        }
        if (!idx.isValid()) {
            _previewMeta->setText("");
            _previewImage->clear();
            _previewText->clear();
            return;
        }
        const auto src = _proxy->mapToSource(idx);
        const auto *it = _model->itemAt(src.row());
        if (!it) return;
        const auto dt = QDateTime::fromMSecsSinceEpoch(it->row.createdAtMs);
        _previewMeta->setText(QString("%1 • ID %2%3")
            .arg(dt.toString("yyyy-MM-dd HH:mm:ss"))
            .arg(it->row.id)
            .arg(it->row.pinned ? " • ★ pinned" : ""));
        if (it->row.type == EntryType::Image) {
            _previewText->setVisible(false);
            _previewImage->setVisible(true);
            const QImage img = qt::pngBytesToImage(it->row.imagePng);
            if (img.isNull()) {
                _previewImage->setText("Image decode failed");
                return;
            }
            QPixmap px = QPixmap::fromImage(img);
            if (const int maxW = _previewImage->width() - 12, maxH = _previewImage->height() - 12;
                maxW > 0 && maxH > 0) {
                px = px.scaled(maxW, maxH, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            }
            _previewImage->setPixmap(px);
        } else {
            _previewImage->setVisible(false);
            _previewText->setVisible(true);
            _previewText->setPlainText(it->row.text);
        }
    }

    void HistoryPopup::showContextMenu(const QPoint &pos) {
        if (const auto idx = _list->indexAt(pos); !idx.isValid()) return;
        QMenu menu(this);
        const QAction *pin = menu.addAction("Toggle Pin (P)");
        const QAction *del = menu.addAction("Delete (Del)");
        menu.addSeparator();
        const QAction *clearFilter = menu.addAction("Clear Filter (Ctrl+Backspace)");
        const auto *chosen = menu.exec(_list->viewport()->mapToGlobal(pos));
        if (!chosen) return;
        if (chosen == pin) onTogglePin();
        else if (chosen == del) onDeleteSelected();
        else if (chosen == clearFilter) {
            _filter->clear();
            onFilterChanged(_filter->text());
            _list->setFocus();
        }
    }
}
