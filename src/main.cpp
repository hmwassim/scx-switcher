#include "mainwindow.h"
#include <QApplication>
#include <QLockFile>
#include <QLocalServer>
#include <QLocalSocket>
#include <QStandardPaths>
#include <QDir>
#include <QMessageBox>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName(MainWindow::APP_NAME);
    app.setApplicationVersion(MainWindow::APP_VERSION);
    app.setOrganizationName("scx-switcher");
    app.setQuitOnLastWindowClosed(false);

    QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation);
    QDir().mkpath(cacheDir);
    QString lockPath = cacheDir + "/scx-switcher.lock";
    QLockFile lockFile(lockPath);

    if (!lockFile.tryLock(100)) {
        QLocalSocket socket;
        socket.connectToServer("scx-switcher");
        if (socket.waitForConnected(1000)) {
            socket.write("show");
            socket.waitForBytesWritten(500);
        }
        return 0;
    }

    QLocalServer server;
    QLocalServer::removeServer("scx-switcher");
    if (!server.listen("scx-switcher"))
        return 1;

    MainWindow w;
    w.show();

    QObject::connect(&server, &QLocalServer::newConnection, &w, [&]() {
        w.show();
        w.raise();
        w.activateWindow();
        while (server.hasPendingConnections()) {
            if (auto *s = server.nextPendingConnection())
                s->deleteLater();
        }
    });

    return app.exec();
}
