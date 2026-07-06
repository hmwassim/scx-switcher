#include "mainwindow.h"

#include <QApplication>
#include <QLocalServer>
#include <QLocalSocket>

static constexpr int SINGLE_INSTANCE_TIMEOUT_MS = 300;

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName(MainWindow::APP_NAME);
    app.setApplicationVersion(MainWindow::APP_VERSION);
    app.setOrganizationName("scx-switcher");
    app.setQuitOnLastWindowClosed(false);

    // Try to connect to an already-running instance.
    {
        QLocalSocket probe;
        probe.connectToServer("scx-switcher");
        if (probe.waitForConnected(SINGLE_INSTANCE_TIMEOUT_MS)) {
            probe.disconnectFromServer();
            return 0;
        }
    }

    // Become the server.
    QLocalServer::removeServer("scx-switcher");
    QLocalServer server;
    if (!server.listen("scx-switcher"))
        return 1;

    MainWindow w;
    w.show();

    // When a second instance connects, raise our window and clean up the socket.
    QObject::connect(&server, &QLocalServer::newConnection, &w, [&] {
        w.show();
        w.raise();
        w.activateWindow();
        // Must drain pending connections or they accumulate and leak file descriptors.
        while (server.hasPendingConnections()) {
            if (auto *s = server.nextPendingConnection())
                s->deleteLater();
        }
    });

    return app.exec();
}
