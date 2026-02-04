#include "app.h"
#include "util/paths.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QLockFile>
#include <QStandardPaths>
#include <QDir>
#include <QMessageBox>
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
    QLockFile lock(lockPath());
    lock.setStaleLockTime(0);
    if (!lock.tryLock()) {
        return 0;
    }
    QCommandLineParser parser;
    parser.setApplicationDescription("ClipVault - clipboard history (text + images)");
    parser.addHelpOption();
    const QCommandLineOption hiddenOpt("hidden", "Start hidden (tray only)");
    parser.addOption(hiddenOpt);
    parser.process(app);
    cv::App clipvault;
    if (!clipvault.init()) {
        QMessageBox::critical(nullptr, "ClipVault", "Failed to initialize (DB or dependencies).");
        return 1;
    }
    clipvault.runPostInit();
    return QApplication::exec();
}
