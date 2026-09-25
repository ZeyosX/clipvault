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
#include <memory>

namespace {
constexpr auto activationPath = "/org/clipvault/ClipVault";
constexpr auto activationInterface = "org.clipvault.ClipVault";
constexpr auto activationSignal = "OpenHistory";
}

static QString lockPath() {
    const auto lockDir = QDir::homePath() + "/.cache/clipvault";
    if (!QDir().mkpath(lockDir)) return {};
    return lockDir + "/instance.lock";
}

static void activateRunningInstance() {
    QDBusConnection::sessionBus().send(
        QDBusMessage::createSignal(activationPath, activationInterface, activationSignal));
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
    const auto instanceLockPath = lockPath();
    if (instanceLockPath.isEmpty()) {
        QMessageBox::critical(nullptr, "ClipVault", "Cannot create the single-instance lock directory.");
        return 1;
    }
    QLockFile lock(instanceLockPath);
    lock.setStaleLockTime(0);
    if (!lock.tryLock()) {
        if (lock.error() != QLockFile::LockFailedError) {
            QMessageBox::critical(nullptr, "ClipVault", "Cannot create the single-instance lock file.");
            return 1;
        }
        if (!parser.isSet(hiddenOpt)) activateRunningInstance();
        return 0;
    }
    std::unique_ptr<QLockFile> oldLock;
    const auto runtimeDir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (!runtimeDir.isEmpty()) {
        oldLock = std::make_unique<QLockFile>(runtimeDir + "/clipvault.lock");
        oldLock->setStaleLockTime(0);
        if (!oldLock->tryLock()) {
            if (oldLock->error() != QLockFile::LockFailedError) {
                QMessageBox::critical(nullptr, "ClipVault", "Cannot check the previous instance lock.");
                return 1;
            }
            if (!parser.isSet(hiddenOpt)) activateRunningInstance();
            return 0;
        }
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
