#include "app.h"
#include "config/autostart.h"
#include "history/clipboard_watcher.h"
#include "history/history_db.h"
#include "ui/history_popup.h"
#include "integration/hotkey_manager.h"
#include "integration/portal_paster.h"
#include "config/settings.h"
#include "ui/tray.h"
#include "util/paths.h"
#include "integration/x11_paster.h"
#include <QApplication>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QFile>
#include <QFileInfo>
namespace cv {
    namespace {
    QString iconPathForAutostart() {
        const auto p = paths::dataDir() + "/clipvault.svg";
        if (!QFileInfo::exists(p)) {
            QFile::copy(":/assets/clipvault.svg", p);
        }
        return p;
    }
    class SettingsDialog final : public QDialog {
    public:
        explicit SettingsDialog(Settings *settings, QWidget *parent = nullptr)
            : QDialog(parent), _settings(settings) {
            setWindowTitle("ClipVault Settings");
            setModal(true);
            setMinimumWidth(430);
            auto *root = new QVBoxLayout(this);
            root->setContentsMargins(20, 20, 20, 18);
            root->setSpacing(14);
            auto *heading = new QLabel("Settings", this);
            heading->setStyleSheet("font-size: 18px; font-weight: 600;");
            root->addWidget(heading);
            auto *form = new QFormLayout();
            form->setSpacing(12);
            _maxEntries = new QSpinBox(this);
            _maxEntries->setRange(10, 50000);
            _maxEntries->setValue(_settings->data().maxEntries);
            _maxEntries->setToolTip("Old unpinned clips are removed when this limit is reached.");
            _hotkeyEdit = new QLineEdit(this);
            _hotkeyEdit->setText(_settings->data().hotkey);
            _hotkeyEdit->setPlaceholderText("Ctrl+F1");
            _hotkeyEdit->setToolTip("Shortcut that opens clipboard history, for example Ctrl+F1.");
            _alwaysOnTop = new QCheckBox("Popup always on top", this);
            _alwaysOnTop->setChecked(_settings->data().alwaysOnTop);
            _startOnLogin = new QCheckBox("Start on login", this);
            _startOnLogin->setChecked(_settings->data().startOnLogin);
            _autoPaste = new QCheckBox("Paste into the active app when choosing a clip", this);
            _autoPaste->setChecked(_settings->data().autoPaste);
            form->addRow("History limit", _maxEntries);
            form->addRow("Open history shortcut", _hotkeyEdit);
            form->addRow("", _alwaysOnTop);
            form->addRow("", _startOnLogin);
            form->addRow("", _autoPaste);
            root->addLayout(form);
            auto *note = new QLabel(
                "With paste off, choosing a clip copies it and closes the popup. "
                "On Wayland, automatic paste may ask for remote control permission.", this);
            note->setWordWrap(true);
            note->setStyleSheet("color: palette(mid);");
            root->addWidget(note);
            auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
            connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
            connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
            root->addWidget(buttons);
        }
        void apply() const {
            auto d = _settings->data();
            d.maxEntries = _maxEntries->value();
            d.alwaysOnTop = _alwaysOnTop->isChecked();
            d.startOnLogin = _startOnLogin->isChecked();
            d.autoPaste = _autoPaste->isChecked();
            d.hotkey = _hotkeyEdit->text().trimmed().isEmpty() ? QString("Ctrl+F1") : _hotkeyEdit->text().trimmed();
            _settings->data() = d;
            _settings->save();
            emit
            _settings->changed();
        }
    private:
        Settings *_settings = nullptr;
        QSpinBox *_maxEntries = nullptr;
        QCheckBox *_alwaysOnTop = nullptr;
        QCheckBox *_startOnLogin = nullptr;
        QCheckBox *_autoPaste = nullptr;
        QLineEdit *_hotkeyEdit = nullptr;
    };
    } // namespace
    App::App(QObject *parent) : QObject(parent) {
    }
    App::~App() {
        delete _db;
        _db = nullptr;
    }
    bool App::init() {
        paths::ensureDirs();
        _settings = new Settings(this);
        _settings->load();
        connect(_settings, &Settings::changed, this, &App::onSettingsChanged);
        _db = new HistoryDb();
        if (!_db->open()) return false;
        _portalPaster = new PortalPaster(this);
        _x11Paster = new X11Paster(this);
        _popup = new HistoryPopup(QApplication::clipboard(), _db, _settings, _portalPaster, _x11Paster);
        connect(_popup, &HistoryPopup::requestedSettings, this, &App::showSettings);
        _tray = new Tray(_settings, this);
        _tray->init();
        _hotkey = new HotkeyManager(this);
        connect(_hotkey, &HotkeyManager::activated, this, &App::showHistory);
        connect(_hotkey, &HotkeyManager::status, this, [this](const QString &msg) {
            if (_tray) _tray->showMessage("ClipVault", msg);
        });
        if (_tray) {
            connect(_tray, &Tray::showHistoryRequested, this, &App::showHistory);
            connect(_tray, &Tray::settingsRequested, this, &App::showSettings);
            connect(_tray, &Tray::quitRequested, qApp, &QCoreApplication::quit);
            connect(_tray, &Tray::clearHistoryRequested, this, &App::clearHistory);
        }
        _watcher = new ClipboardWatcher(QApplication::clipboard(), _db, _settings, this);
        connect(_watcher, &ClipboardWatcher::entryAdded, _popup, &HistoryPopup::refresh);
        ensureAutostart();
        return true;
    }
    void App::runPostInit() const {
        _hotkey->start(_settings->data().hotkey);
    }
    void App::showHistory() const {
        _popup->openPopup();
    }
    void App::showSettings() const {
        if (SettingsDialog dlg(_settings); dlg.exec() == QDialog::Accepted) {
            dlg.apply();
            _db->pruneToMax(_settings->data().maxEntries);
            _popup->refresh();
        }
    }
    void App::onSettingsChanged() const {
        ensureAutostart();
        if (_hotkey) _hotkey->rebind(_settings->data().hotkey);
    }
    void App::ensureAutostart() const {
        const auto execPath = QCoreApplication::applicationFilePath();
        const auto iconPath = iconPathForAutostart();
        autostart::setEnabled(_settings->data().startOnLogin, execPath, iconPath);
    }
    void App::clearHistory() const {
        QMessageBox confirm(QMessageBox::Warning, "Clear clipboard history",
                            "Delete all saved clips, including pinned items?", QMessageBox::NoButton);
        auto *clearButton = confirm.addButton("Clear history", QMessageBox::DestructiveRole);
        confirm.addButton(QMessageBox::Cancel);
        confirm.exec();
        if (confirm.clickedButton() != clearButton) return;
        if (_db && _db->clearAll()) {
            _popup->refresh();
            if (_tray) _tray->showMessage("ClipVault", "Clipboard history cleared");
        } else {
            QMessageBox::warning(nullptr, "ClipVault", "Could not clear clipboard history.");
        }
    }
} 
