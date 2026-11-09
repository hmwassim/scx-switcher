#include "app.h"

#include <QApplication>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QLoggingCategory>

static const QString APP_ID = QStringLiteral("io.github.hmwassim.scx_switcher");

int main(int argc, char **argv) {
   /* Skipping Qt's GPU path: plain widgets draw fine without it, and it costs about 20 MB */
   if (qEnvironmentVariableIsEmpty("QT_WAYLAND_CLIENT_BUFFER_INTEGRATION")) {
      qputenv("QT_WAYLAND_CLIENT_BUFFER_INTEGRATION", "none");
      QLoggingCategory::setFilterRules("qt.qpa.wayland.warning=false");
   }

   QApplication app(argc, argv);
   QApplication::setApplicationName("SCX Switcher");
   QApplication::setApplicationVersion(APP_VERSION);
   QApplication::setDesktopFileName(APP_ID);
   QApplication::setWindowIcon(QIcon::fromTheme("scx-switcher"));

   /* Showing the copy that already runs instead of starting a second one */
   QDBusConnection bus = QDBusConnection::sessionBus();
   if (bus.isConnected() && !bus.registerService(APP_ID)) {
      QDBusInterface(APP_ID, "/", APP_ID, bus).call("Show");
      return 0;
   }

   App main(app.arguments().contains("--tray"));
   bus.registerObject("/", &main, QDBusConnection::ExportScriptableSlots);
   return app.exec();
}
