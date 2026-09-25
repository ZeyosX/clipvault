#include "app.h"
#include "util/paths.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QLockFile>
#include <QStandardPaths>
#include <QDir>
#include <QMessageBox>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QMetaObject>

namespace {
constexpr auto activationPath = "/org/clipvault/ClipVault";
constexpr auto activationInterface = "org.clipvault.ClipVault";
constexpr auto activationSignal = "OpenHistory";
}

static QString lockPath() {
    auto run = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (run.isEmpty()) run = QDir::tempPath();
    QDir().mkpath(run);
    return run + "/clipvault.lock";
}
int main(int argc, char **argv) {
    const QApplication app(argc, argv);
    QApplication::setApplicationName("ClipVault");
    QApplication::setOrganizationName("ClipVault");
    QCommandLineParser parser;
    parser.setApplicationDescription("ClipVault - clipboard history (text + images)");
    parser.addHelpOption();
    const QCommandLineOption hiddenOpt("hidden", "Start hidden (tray only)");
    parser.addOption(hiddenOpt);
    parser.process(app);
    QLockFile lock(lockPath());
    lock.setStaleLockTime(0);
    if (!lock.tryLock()) {
        if (!parser.isSet(hiddenOpt)) {
            QDBusConnection::sessionBus().send(
                QDBusMessage::createSignal(activationPath, activationInterface, activationSignal));
        }
        return 0;
    }
    cv::App clipvault;
    if (!clipvault.init()) {
        QMessageBox::critical(nullptr, "ClipVault", "Failed to initialize (DB or dependencies).");
        return 1;
    }
    QDBusConnection::sessionBus().connect(QString(), activationPath, activationInterface, activationSignal,
                                          &clipvault, SLOT(showHistory()));
    clipvault.runPostInit();
    if (!parser.isSet(hiddenOpt)) {
        QMetaObject::invokeMethod(&clipvault, "showHistory", Qt::QueuedConnection);
    }
    return QApplication::exec();
}
