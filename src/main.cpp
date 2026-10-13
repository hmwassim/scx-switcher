#include "mainwindow.h"

#include <QApplication>
#include <QLocalServer>
#include <QLocalSocket>
#include <QSystemTrayIcon>
#include <unistd.h>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("scx-switcher");
    app.setApplicationDisplayName("SCX Switcher");
    app.setApplicationVersion(APP_VERSION);
    app.setDesktopFileName("io.github.hmwassim.scx_switcher");
    app.setWindowIcon(QIcon::fromTheme("scx-switcher"));
    app.setQuitOnLastWindowClosed(!QSystemTrayIcon::isSystemTrayAvailable());

    // Single instance: if one is already running, ask it to show itself.
    const QString socket = QString("scx-switcher-%1").arg(getuid());
    QLocalSocket probe;
    probe.connectToServer(socket);
    if (probe.waitForConnected(300))
        return 0;

    QLocalServer::removeServer(socket);
    QLocalServer server;
    server.setSocketOptions(QLocalServer::UserAccessOption);
    server.listen(socket);

    MainWindow window;
    window.show();

    QObject::connect(&server, &QLocalServer::newConnection, &window, [&] {
        while (auto *conn = server.nextPendingConnection())
            conn->deleteLater();
        window.show();
        window.raise();
        window.activateWindow();
    });

    return app.exec();
}
