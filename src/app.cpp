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
#include <QClipboard>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QLabel>
#include <QSpinBox>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QFile>
#include <QFileInfo>
namespace cv {
    static QString iconPathForAutostart() {
        const auto p = cv::paths::dataDir() + "/clipvault.svg";
        if (!QFileInfo::exists(p)) {
            QFile::copy(":/assets/clipvault.svg", p);
        }
        return p;
    }
    class SettingsDialog final : public QDialog {
    public:
        SettingsDialog(Settings *settings, QWidget *parent = nullptr)
            : QDialog(parent), _settings(settings) {
            setWindowTitle("ClipVault Settings");
            setModal(true);
            auto *root = new QVBoxLayout(this);
            auto *form = new QFormLayout();
            _maxEntries = new QSpinBox(this);
            _maxEntries->setRange(10, 50000);
            _maxEntries->setValue(_settings->data().maxEntries);
            _hotkeyEdit = new QLineEdit(this);
            _hotkeyEdit->setText(_settings->data().hotkey);
            _hotkeyEdit->setPlaceholderText("Ctrl+F1");
            _alwaysOnTop = new QCheckBox("Popup always on top", this);
            _alwaysOnTop->setChecked(_settings->data().alwaysOnTop);
            _startOnLogin = new QCheckBox("Start on login", this);
            _startOnLogin->setChecked(_settings->data().startOnLogin);
            _autoPaste = new QCheckBox("Auto paste on Enter (Wayland asks for Remote Control permission)", this);
            _autoPaste->setChecked(_settings->data().autoPaste);
            form->addRow("Max history entries", _maxEntries);
            form->addRow("Hotkey", _hotkeyEdit);
            form->addRow("", _alwaysOnTop);
            form->addRow("", _startOnLogin);
            form->addRow("", _autoPaste);
            root->addLayout(form);
            auto *note = new QLabel(
                "Tip: On Wayland, global shortcuts/paste use XDG portals.\n"
                "On Xorg/X11, hotkey and paste are implemented via X11."
                , this);
            note->setWordWrap(true);
            note->setStyleSheet("color: rgba(0,0,0,0.65);");
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
    App::App(QObject *parent) : QObject(parent) {
    }
    App::~App() {
        delete _db;
        _db = nullptr;
    }
    bool App::init() {
        cv::paths::ensureDirs();
        _settings = new Settings(this);
        _settings->load();
        connect(_settings, &Settings::changed, this, &App::onSettingsChanged);
        _db = new HistoryDb();
        if (!_db->open()) return false;
        _portalPaster = new PortalPaster(this);
        _x11Paster = new X11Paster(this);
        _popup = new HistoryPopup(QApplication::clipboard(), _db, _settings, _portalPaster, _x11Paster);
        _tray = new Tray(_settings, this);
        if (!_tray->init()) {
        }
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
        SettingsDialog dlg(_settings);
        if (dlg.exec() == QDialog::Accepted) {
            dlg.apply();
            _db->pruneToMax(_settings->data().maxEntries);
            if (_hotkey) _hotkey->rebind(_settings->data().hotkey);
        }
    }
    void App::onSettingsChanged() {
        ensureAutostart();
        if (_hotkey) _hotkey->rebind(_settings->data().hotkey);
    }
    void App::ensureAutostart() const {
        const auto execPath = QCoreApplication::applicationFilePath();
        const auto iconPath = iconPathForAutostart();
        cv::autostart::setEnabled(_settings->data().startOnLogin, execPath, iconPath);
    }
    void App::clearHistory() const {
        if (_db) _db->clearAll();
        if (_tray) _tray->showMessage("ClipVault", "History cleared");
    }
} 
