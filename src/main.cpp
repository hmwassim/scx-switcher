#include "mainwindow.h"
#include <QApplication>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMessageBox>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName(MainWindow::APP_NAME);
    app.setApplicationVersion(MainWindow::APP_VERSION);
    app.setOrganizationName("scx-switcher");
    app.setQuitOnLastWindowClosed(false);

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
