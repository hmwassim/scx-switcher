#include "app.h"

#include "window.h"

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMenu>
#include <QStandardPaths>
#include <QSystemTrayIcon>
#include <QTimer>

static const QString APP_ID = QStringLiteral("io.github.hmwassim.scx_switcher");
static constexpr int TRAY_WAIT_MS = 2000;
static constexpr int TRAY_TRIES = 15;

static QString autostartPath() {
   return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/autostart/" +
          APP_ID + ".desktop";
}

App::App(bool trayOnly, QObject *parent) : QObject(parent) {
   m_activity.add(QString("SCX Switcher %1").arg(APP_VERSION));
   if (!trayOnly) {
      setupTray();
      Show();
      return;
   }

   if (QSystemTrayIcon::isSystemTrayAvailable()) {
      setupTray();
      return;
   }

   /* Waiting for the panel at login, then falling back to the window */
   auto *timer = new QTimer(this);
   connect(timer, &QTimer::timeout, this, [this, timer] {
      if (!QSystemTrayIcon::isSystemTrayAvailable() && ++m_trayTries < TRAY_TRIES)
         return;
      timer->deleteLater();
      setupTray();
      if (!m_tray)
         Show();
   });
   timer->start(TRAY_WAIT_MS);
}

void App::Show() {
   if (!m_window) {
      m_window = new Window(m_monitor, m_activity);
      m_window->setAttribute(Qt::WA_DeleteOnClose, m_tray != nullptr);
      m_window->setWindowIcon(QIcon::fromTheme("scx-switcher"));
   }
   m_window->showNormal();
   m_window->raise();
   m_window->activateWindow();
}

void App::setupTray() {
   if (m_tray || !QSystemTrayIcon::isSystemTrayAvailable())
      return;
   QApplication::setQuitOnLastWindowClosed(false);

   m_tray = new QSystemTrayIcon(this);
   auto *menu = new QMenu;
   menu->addAction("Show SCX Switcher", this, &App::Show);
   m_loginStart = menu->addAction("Start at login");
   m_loginStart->setCheckable(true);
   m_loginStart->setChecked(QFile::exists(autostartPath()));
   connect(m_loginStart, &QAction::toggled, this, &App::setLoginStart);
   menu->addSeparator();
   menu->addAction("Quit", qApp, &QApplication::quit);
   m_tray->setContextMenu(menu);
   connect(m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason r) {
      if (r == QSystemTrayIcon::Trigger)
         toggleWindow();
   });
   connect(&m_monitor, &Monitor::changed, this, &App::updateTray);
   updateTray();
   m_tray->show();
   m_monitor.setTray(true);
   if (m_window)
      m_window->setAttribute(Qt::WA_DeleteOnClose, true);
}

void App::updateTray() {
   m_tray->setIcon(style::trayIcon(m_monitor.tone()));
   m_tray->setToolTip("SCX Switcher: " + m_monitor.summary());
}

/* Closing to the tray frees the whole window; only the icon stays */
void App::toggleWindow() {
   if (m_window && m_window->isVisible() && !m_window->isMinimized())
      m_window->close();
   else
      Show();
}

void App::setLoginStart(bool on) {
   const QString path = autostartPath();
   if (!on) {
      QFile::remove(path);
      m_activity.add("Login start [off]");
      return;
   }
   QDir().mkpath(QFileInfo(path).path());
   QFile file(path);
   if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
      m_activity.add("Login start [failed] " + file.errorString());
      m_loginStart->setChecked(false);
      return;
   }
   file.write("[Desktop Entry]\n"
              "Type=Application\n"
              "Name=SCX Switcher\n"
              "Comment=Shows the sched_ext scheduler in the tray\n"
              "Exec=scx-switcher --tray\n"
              "Icon=scx-switcher\n"
              "NoDisplay=true\n"
              "X-GNOME-Autostart-enabled=true\n");
   m_activity.add("Login start [on]");
}
