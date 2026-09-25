#include "ui/history_popup.h"
#include "history/history_db.h"
#include "ui/history_model.h"
#include "config/settings.h"
#include "integration/portal_paster.h"
#include "integration/x11_paster.h"
#include "util/qt_helpers.h"
#include <QClipboard>
#include <QCursor>
#include <QApplication>
#include <QGuiApplication>
#include <QDateTime>
#include <QAction>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QMenu>
#include <QPushButton>
#include <QShortcut>
#include <QStackedWidget>
#include <QTextEdit>
#include <QToolButton>
#include <QSplitter>
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
            const auto type = sourceModel()->data(idx, HistoryModel::TypeRole).toInt();
            const auto pinned = sourceModel()->data(idx, HistoryModel::PinnedRole).toBool();
            return t.contains(filterRegularExpression())
                   || (type == static_cast<int>(EntryType::Image) && QString("image").contains(filterRegularExpression()))
                   || (pinned && QString("pinned").contains(filterRegularExpression()));
        }
    };
    } // namespace

    HistoryPopup::HistoryPopup(QClipboard *clipboard, HistoryDb *db, Settings *settings, PortalPaster *portalPaster,
                               X11Paster *x11Paster, QWidget *parent)
        : QWidget(parent), _clipboard(clipboard), _db(db), _settings(settings), _portalPaster(portalPaster),
          _x11Paster(x11Paster) {
        setWindowFlags(Qt::FramelessWindowHint | Qt::Tool);
        setObjectName("historyPopup");
        setWindowTitle("ClipVault — Clipboard history");
        auto *root = new QVBoxLayout(this);
        root->setContentsMargins(18, 16, 18, 14);
        root->setSpacing(12);

        _header = new QWidget(this);
        _header->setCursor(Qt::SizeAllCursor);
        _header->installEventFilter(this);
        auto *top = new QHBoxLayout(_header);
        top->setContentsMargins(0, 0, 0, 0);
        top->setSpacing(10);
        auto *heading = new QVBoxLayout();
        heading->setSpacing(2);
        auto *title = new QLabel("Clipboard history", _header);
        title->setObjectName("popupTitle");
        _hint = new QLabel("Find, preview, and reuse what you copied", _header);
        _hint->setObjectName("mutedText");
        heading->addWidget(title);
        heading->addWidget(_hint);
        top->addLayout(heading);
        top->addStretch(1);
        auto *settingsButton = new QToolButton(_header);
        settingsButton->setText("Settings");
        settingsButton->setToolTip("Open settings");
        connect(settingsButton, &QToolButton::clicked, this, &HistoryPopup::requestedSettings);
        top->addWidget(settingsButton);
        auto *closeButton = new QToolButton(_header);
        closeButton->setText("×");
        closeButton->setObjectName("closeButton");
        closeButton->setToolTip("Close (Esc)");
        closeButton->setAccessibleName("Close clipboard history");
        connect(closeButton, &QToolButton::clicked, this, &QWidget::close);
        top->addWidget(closeButton);
        root->addWidget(_header);

        _filter = new QLineEdit(this);
        _filter->setObjectName("historySearch");
        _filter->setPlaceholderText("Search clips…");
        _filter->setClearButtonEnabled(true);
        _filter->setToolTip("Search text, images, or pinned items (Ctrl+F)");
        _filter->setAccessibleName("Search clipboard history");
        root->addWidget(_filter);

        _model = new HistoryModel(this);
        _proxy = new HistoryFilterProxy(this);
        _proxy->setSourceModel(_model);
        _proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
        _listStack = new QStackedWidget(this);
        _list = new QListView(_listStack);
        _list->setObjectName("historyList");
        _list->setAccessibleName("Clipboard entries");
        _list->setModel(_proxy);
        _list->setSelectionMode(QAbstractItemView::ExtendedSelection);
        _list->setEditTriggers(QAbstractItemView::NoEditTriggers);
        _list->setUniformItemSizes(true);
        _list->setSpacing(3);
        _list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
        _list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        _list->setTextElideMode(Qt::ElideRight);
        _list->setIconSize(QSize(48, 48));
        _list->setContextMenuPolicy(Qt::CustomContextMenu);
        _list->installEventFilter(this);
        _listStack->addWidget(_list);
        _emptyState = new QLabel(_listStack);
        _emptyState->setObjectName("emptyState");
        _emptyState->setAlignment(Qt::AlignCenter);
        _emptyState->setWordWrap(true);
        _listStack->addWidget(_emptyState);

        auto *previewWrap = new QWidget(this);
        previewWrap->setObjectName("previewPane");
        auto *previewCol = new QVBoxLayout(previewWrap);
        previewCol->setContentsMargins(12, 10, 12, 12);
        previewCol->setSpacing(9);
        auto *previewTitle = new QLabel("Preview", previewWrap);
        previewTitle->setObjectName("sectionTitle");
        previewCol->addWidget(previewTitle);
        _previewMeta = new QLabel(this);
        _previewMeta->setObjectName("mutedText");
        _previewMeta->setWordWrap(true);
        previewCol->addWidget(_previewMeta);
        _previewStack = new QStackedWidget(previewWrap);
        _previewEmpty = new QLabel("Select a clip to see its contents", _previewStack);
        _previewEmpty->setObjectName("previewEmpty");
        _previewEmpty->setAlignment(Qt::AlignCenter);
        _previewEmpty->setWordWrap(true);
        _previewStack->addWidget(_previewEmpty);
        _previewImage = new QLabel(_previewStack);
        _previewImage->setAlignment(Qt::AlignCenter);
        _previewImage->setObjectName("previewContent");
        _previewStack->addWidget(_previewImage);
        _previewText = new QTextEdit(_previewStack);
        _previewText->setObjectName("previewContent");
        _previewText->setReadOnly(true);
        _previewStack->addWidget(_previewText);
        previewCol->addWidget(_previewStack, 1);

        auto *splitter = new QSplitter(Qt::Horizontal, this);
        splitter->setChildrenCollapsible(false);
        splitter->addWidget(_listStack);
        splitter->addWidget(previewWrap);
        splitter->setStretchFactor(0, 2);
        splitter->setStretchFactor(1, 3);
        root->addWidget(splitter, 1);

        auto *footer = new QHBoxLayout();
        footer->setSpacing(8);
        _status = new QLabel(this);
        _status->setObjectName("mutedText");
        footer->addWidget(_status);
        footer->addStretch(1);
        _copyButton = new QPushButton("Copy", this);
        _copyButton->setToolTip("Copy without closing (Ctrl+C)");
        footer->addWidget(_copyButton);
        _pinButton = new QPushButton("Pin", this);
        _pinButton->setToolTip("Pin or unpin the selected clips (P)");
        footer->addWidget(_pinButton);
        _deleteButton = new QPushButton("Delete", this);
        _deleteButton->setObjectName("deleteButton");
        _deleteButton->setToolTip("Delete the selected clips (Del)");
        footer->addWidget(_deleteButton);
        _primaryButton = new QPushButton(this);
        _primaryButton->setObjectName("primaryButton");
        _primaryButton->setDefault(true);
        footer->addWidget(_primaryButton);
        root->addLayout(footer);

        connect(_list->selectionModel(), &QItemSelectionModel::selectionChanged, this, &HistoryPopup::updatePreview);
        connect(_list->selectionModel(), &QItemSelectionModel::currentChanged, this, &HistoryPopup::updatePreview);
        connect(_list, &QListView::doubleClicked, this, &HistoryPopup::onConfirm);
        connect(_list, &QListView::customContextMenuRequested, this, &HistoryPopup::showContextMenu);
        connect(_filter, &QLineEdit::textChanged, this, &HistoryPopup::onFilterChanged);
        connect(_filter, &QLineEdit::returnPressed, this, &HistoryPopup::onConfirm);
        _filter->installEventFilter(this);
        connect(_copyButton, &QPushButton::clicked, this, &HistoryPopup::onCopy);
        connect(_pinButton, &QPushButton::clicked, this, &HistoryPopup::onTogglePin);
        connect(_deleteButton, &QPushButton::clicked, this, &HistoryPopup::onDeleteSelected);
        connect(_primaryButton, &QPushButton::clicked, this, &HistoryPopup::onConfirm);
        auto *findShortcut = new QShortcut(QKeySequence::Find, this);
        findShortcut->setContext(Qt::WidgetWithChildrenShortcut);
        connect(findShortcut, &QShortcut::activated, this, [this] {
            _filter->setFocus();
            _filter->selectAll();
        });
        auto *closeShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this);
        closeShortcut->setContext(Qt::WidgetWithChildrenShortcut);
        connect(closeShortcut, &QShortcut::activated, this, &QWidget::close);
        setStyleSheet(R"(
            QWidget#historyPopup { background: #151c2a; color: #f3f6fb; border: 1px solid #334156; border-radius: 14px; }
            QLabel#popupTitle { color: #f8fafc; font-size: 20px; font-weight: 700; }
            QLabel#sectionTitle { color: #e6edf7; font-size: 14px; font-weight: 600; }
            QLabel#mutedText, QLabel#emptyState, QLabel#previewEmpty { color: #aab8cb; font-size: 12px; }
            QLineEdit#historySearch { background: #202c3e; color: #f8fafc; border: 1px solid #43536b;
                border-radius: 9px; padding: 10px 12px; selection-background-color: #3765a7; }
            QLineEdit#historySearch:focus { border: 1px solid #69a7ff; }
            QLineEdit#historySearch QToolButton { background: transparent; border: 0; padding: 0; }
            QListView#historyList { background: #1b2637; color: #f3f6fb; border: 1px solid #35445b;
                border-radius: 9px; padding: 5px; outline: none; }
            QListView#historyList::item { padding: 8px; border-radius: 6px; }
            QListView#historyList::item:hover { background: #293b55; }
            QListView#historyList::item:selected { background: #315c96; color: white; }
            QWidget#previewPane, QLabel#emptyState, QLabel#previewEmpty { background: #1b2637;
                border: 1px solid #35445b; border-radius: 9px; }
            QLabel#previewContent, QTextEdit#previewContent { background: #202c3e; color: #f3f6fb;
                border: 0; border-radius: 8px; padding: 8px; }
            QPushButton, QToolButton { background: #29374d; color: #eef3fb; border: 1px solid #43536b;
                border-radius: 7px; padding: 7px 12px; }
            QPushButton:hover, QToolButton:hover { background: #354a68; }
            QPushButton:disabled { color: #7f8ca0; background: #202b3b; border-color: #344154; }
            QPushButton#primaryButton { background: #3478d4; color: white; border-color: #3478d4; font-weight: 600; }
            QPushButton#primaryButton:hover { background: #4389e7; }
            QPushButton#primaryButton:disabled { background: #2c4260; color: #91a4bd; border-color: #2c4260; }
            QPushButton#deleteButton { color: #ffc7c7; }
            QPushButton#deleteButton:disabled { color: #7f8ca0; }
            QToolButton#closeButton { font-size: 19px; padding: 3px 9px; }
        )");
        installEventFilter(this);
        applyAlwaysOnTop();
        updateViewState();
    }

    void HistoryPopup::applyAlwaysOnTop() {
        auto flags = Qt::FramelessWindowHint | Qt::Tool;
        if (_settings->data().alwaysOnTop) flags |= Qt::WindowStaysOnTopHint;
        setWindowFlags(flags);
    }

    void HistoryPopup::openPopup() {
        applyAlwaysOnTop();
        _filter->clear();
        _model->reloadFromDb(*_db, _settings->data().maxEntries);
        const QPoint cursor = QCursor::pos();
        constexpr QSize desired(840, 520);
        const QScreen *screen = QGuiApplication::screenAt(cursor);
        if (!screen) screen = QGuiApplication::primaryScreen();
        const QRect avail = screen ? screen->availableGeometry() : QRect(QPoint(0, 0), desired);
        resize(qMin(desired.width(), qMax(1, avail.width() - 24)),
               qMin(desired.height(), qMax(1, avail.height() - 24)));
        QPoint pos = cursor + QPoint(14, 14);
        pos.setX(qBound(avail.left(), pos.x(), avail.right() - width() + 1));
        pos.setY(qBound(avail.top(), pos.y(), avail.bottom() - height() + 1));
        move(pos);
        show();
        raise();
        activateWindow();
        selectFirstItem();
        if (_proxy->rowCount() > 0) _list->setFocus();
        else _filter->setFocus();
        updatePreview();
    }

    void HistoryPopup::refresh() {
        if (!isVisible()) return;
        qint64 selectedId = 0;
        if (const auto source = _proxy->mapToSource(_list->currentIndex()); source.isValid()) {
            if (const auto *item = _model->itemAt(source.row())) selectedId = item->row.id;
        }
        _model->reloadFromDb(*_db, _settings->data().maxEntries);
        if (selectedId) selectItem(selectedId);
        else selectFirstItem();
        updatePreview();
    }

    void HistoryPopup::selectFirstItem() const {
        if (_proxy->rowCount() == 0) {
            _list->selectionModel()->clear();
            return;
        }
        const auto first = _proxy->index(0, 0);
        _list->selectionModel()->setCurrentIndex(first, QItemSelectionModel::ClearAndSelect);
        _list->scrollTo(first);
    }

    void HistoryPopup::selectItem(const qint64 id) const {
        for (int row = 0; row < _model->rowCount(QModelIndex()); ++row) {
            const auto *item = _model->itemAt(row);
            if (!item || item->row.id != id) continue;
            const auto proxyIndex = _proxy->mapFromSource(_model->index(row, 0));
            if (proxyIndex.isValid()) {
                _list->selectionModel()->setCurrentIndex(proxyIndex, QItemSelectionModel::ClearAndSelect);
                _list->scrollTo(proxyIndex);
                return;
            }
        }
        selectFirstItem();
    }

    void HistoryPopup::updateViewState() const {
        const int saved = _model->rowCount(QModelIndex());
        const int shown = _proxy->rowCount();
        const int selected = _list->selectionModel()->selectedIndexes().size();
        _listStack->setCurrentWidget(shown ? static_cast<QWidget *>(_list) : _emptyState);
        _emptyState->setText(saved == 0 ? "Nothing copied yet\nCopy some text or an image to get started"
                                         : "No matching clips\nTry a different search");
        _status->setText(_filter->text().isEmpty()
                             ? QString("%1 saved · %2 selected").arg(saved).arg(selected)
                             : QString("%1 of %2 shown · %3 selected").arg(shown).arg(saved).arg(selected));
        _primaryButton->setText(_settings->data().autoPaste ? "Paste selection" : "Copy selection");
        _primaryButton->setToolTip(_settings->data().autoPaste
                                       ? "Copy and paste the selection (Enter)"
                                       : "Copy the selection (Enter)");
        _primaryButton->setEnabled(selected > 0);
        _copyButton->setVisible(_settings->data().autoPaste);
        _copyButton->setEnabled(selected > 0);
        _pinButton->setEnabled(selected > 0);
        _deleteButton->setEnabled(selected > 0);
        if (selected > 0) {
            const auto source = _proxy->mapToSource(_list->currentIndex());
            const auto *item = source.isValid() ? _model->itemAt(source.row()) : nullptr;
            _pinButton->setText(item && item->row.pinned ? "Unpin" : "Pin");
        } else {
            _pinButton->setText("Pin");
        }
    }

    bool HistoryPopup::eventFilter(QObject *obj, QEvent *e) {
        if ((obj == _list || obj == _filter) && e->type() == QEvent::KeyPress) {
            const auto *key = static_cast<QKeyEvent *>(e);
            if (key->key() == Qt::Key_Escape) {
                close();
                return true;
            }
            if (key->matches(QKeySequence::Find)) {
                _filter->setFocus();
                _filter->selectAll();
                return true;
            }
            if (key->key() == Qt::Key_Backspace && key->modifiers() == Qt::ControlModifier) {
                _filter->clear();
                return true;
            }
            if (obj == _filter && (key->key() == Qt::Key_Down || key->key() == Qt::Key_Up) &&
                _proxy->rowCount() > 0) {
                _list->setFocus();
                return true;
            }
            if (obj == _list) {
                if (key->matches(QKeySequence::Copy)) {
                    onCopy();
                    return true;
                }
                if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
                    onConfirm();
                    return true;
                }
                if (key->key() == Qt::Key_P && key->modifiers() == Qt::NoModifier) {
                    onTogglePin();
                    return true;
                }
                if (key->key() == Qt::Key_Delete) {
                    onDeleteSelected();
                    return true;
                }
                if (key->key() == Qt::Key_Backspace && key->modifiers() == Qt::NoModifier) {
                    _filter->backspace();
                    return true;
                }
                if (key->modifiers() == Qt::NoModifier || key->modifiers() == Qt::ShiftModifier) {
                    if (const auto text = key->text(); !text.isEmpty() && text.at(0).isPrint()) {
                        _filter->insert(text);
                        return true;
                    }
                }
            }
        }
        if (obj == _header) {
            if (e->type() == QEvent::MouseButtonPress) {
                if (const auto *me = static_cast<QMouseEvent *>(e); me->button() == Qt::LeftButton) {
                    _dragging = true;
                    _dragOffset = me->globalPosition().toPoint() - frameGeometry().topLeft();
                    return true;
                }
            } else if (e->type() == QEvent::MouseMove) {
                if (_dragging) {
                    const auto *me = static_cast<QMouseEvent *>(e);
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
        if (e->type() == QEvent::WindowDeactivate && !QApplication::activePopupWidget()) {
            close();
            return true;
        }
        return QWidget::event(e);
    }

    void HistoryPopup::onFilterChanged(const QString &t) const {
        _proxy->setFilterRegularExpression(
            QRegularExpression(QRegularExpression::escape(t), QRegularExpression::CaseInsensitiveOption));
        selectFirstItem();
        if (_proxy->rowCount() == 0) _filter->setFocus();
        updatePreview();
    }

    bool HistoryPopup::setClipboardFromSelection() const {
        auto idxs = _list->selectionModel()->selectedIndexes();
        if (idxs.isEmpty()) return false;
        std::ranges::sort(idxs,
                          [](const QModelIndex &a, const QModelIndex &b) { return a.row() < b.row(); });
        if (idxs.size() == 1) {
            const auto src = _proxy->mapToSource(idxs[0]);
            const auto *it = _model->itemAt(src.row());
            if (!it) return false;
            if (it->row.type == EntryType::Image && !it->row.imagePng.isEmpty()) {
                const QImage img = qt::pngBytesToImage(it->row.imagePng);
                if (img.isNull()) return false;
                _clipboard->setImage(img, QClipboard::Clipboard);
                return true;
            }
            if (it->row.type != EntryType::Text) return false;
            _clipboard->setText(it->row.text, QClipboard::Clipboard);
            return true;
        }
        QStringList parts;
        for (const auto &px: idxs) {
            const auto src = _proxy->mapToSource(px);
            const auto *it = _model->itemAt(src.row());
            if (!it) continue;
            if (it->row.type == EntryType::Text) parts << it->row.text;
        }
        if (parts.isEmpty()) return false;
        _clipboard->setText(parts.join("\n"), QClipboard::Clipboard);
        return true;
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
        if (!setClipboardFromSelection()) {
            _status->setText("Select a text clip or one image first");
            return;
        }
        close();
        triggerPaste();
    }

    void HistoryPopup::onCopy() const {
        if (setClipboardFromSelection()) {
            _status->setText("Copied selection to clipboard");
        } else {
            _status->setText("Select a text clip or one image first");
        }
    }

    void HistoryPopup::onTogglePin() const {
        const auto idx = _list->currentIndex();
        if (!idx.isValid()) return;
        auto selected = _list->selectionModel()->selectedIndexes();
        if (selected.isEmpty()) selected = {idx};
        const auto src0 = _proxy->mapToSource(selected[0]);
        const auto *it0 = _model->itemAt(src0.row());
        if (!it0) return;
        const qint64 selectedId = it0->row.id;
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
        selectItem(selectedId);
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
        if (!_db->deleteByIds(ids)) {
            _status->setText("Could not delete the selected clips");
            return;
        }
        _model->reloadFromDb(*_db, _settings->data().maxEntries);
        selectFirstItem();
        updatePreview();
    }

    void HistoryPopup::updatePreview() const {
        QModelIndex idx = _list->currentIndex();
        if (!idx.isValid()) {
            if (const auto sel = _list->selectionModel()->selectedIndexes(); !sel.isEmpty()) idx = sel[0];
        }
        if (!idx.isValid()) {
            _previewMeta->clear();
            _previewStack->setCurrentWidget(_previewEmpty);
            updateViewState();
            return;
        }
        const auto src = _proxy->mapToSource(idx);
        const auto *it = _model->itemAt(src.row());
        if (!it) {
            _previewMeta->clear();
            _previewStack->setCurrentWidget(_previewEmpty);
            updateViewState();
            return;
        }
        const auto dt = QDateTime::fromMSecsSinceEpoch(it->row.createdAtMs);
        const auto savedAt = dt.toString("yyyy-MM-dd HH:mm");
        const auto pinned = it->row.pinned ? QString(" · Pinned") : QString();
        if (it->row.type == EntryType::Image) {
            const QImage img = qt::pngBytesToImage(it->row.imagePng);
            _previewMeta->setText(QString("Image · %1 × %2 · %3%4")
                                      .arg(img.width()).arg(img.height()).arg(savedAt, pinned));
            _previewStack->setCurrentWidget(_previewImage);
            if (img.isNull()) {
                _previewImage->setText("Image decode failed");
                updateViewState();
                return;
            }
            _previewImage->clear();
            QPixmap px = QPixmap::fromImage(img);
            if (const int maxW = _previewImage->width() - 12, maxH = _previewImage->height() - 12;
                maxW > 0 && maxH > 0) {
                px = px.scaled(maxW, maxH, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            }
            _previewImage->setPixmap(px);
        } else {
            _previewMeta->setText(QString("Text · %1 characters · %2%3")
                                      .arg(it->row.text.size()).arg(savedAt, pinned));
            _previewStack->setCurrentWidget(_previewText);
            _previewText->setPlainText(it->row.text);
        }
        updateViewState();
    }

    void HistoryPopup::showContextMenu(const QPoint &pos) {
        const auto idx = _list->indexAt(pos);
        if (!idx.isValid()) return;
        if (!_list->selectionModel()->isSelected(idx)) {
            _list->selectionModel()->setCurrentIndex(idx, QItemSelectionModel::ClearAndSelect);
        }
        QMenu menu(this);
        const QAction *copy = menu.addAction("Copy (Ctrl+C)");
        const QAction *paste = menu.addAction(_settings->data().autoPaste
                                                  ? "Paste (Enter)" : "Copy and close (Enter)");
        menu.addSeparator();
        const QAction *pin = menu.addAction("Toggle Pin (P)");
        const QAction *del = menu.addAction("Delete (Del)");
        menu.addSeparator();
        const QAction *clearFilter = menu.addAction("Clear Filter (Ctrl+Backspace)");
        const auto *chosen = menu.exec(_list->viewport()->mapToGlobal(pos));
        if (!chosen) return;
        if (chosen == copy) onCopy();
        else if (chosen == paste) onConfirm();
        else if (chosen == pin) onTogglePin();
        else if (chosen == del) onDeleteSelected();
        else if (chosen == clearFilter) {
            _filter->clear();
            onFilterChanged(_filter->text());
            _list->setFocus();
        }
    }
}
