#pragma once

#include "activity.h"
#include "monitor.h"

#include <QObject>
#include <QPointer>

class QAction;
class QSystemTrayIcon;
class Window;

/* Owns everything that outlives the window: the monitor, the log and the tray icon */
class App : public QObject {
   Q_OBJECT
   Q_CLASSINFO("D-Bus Interface", "io.github.hmwassim.scx_switcher")
public:
   explicit App(bool trayOnly, QObject *parent = nullptr);

public slots:
   Q_SCRIPTABLE void Show();

private:
   void setupTray();
   void updateTray();
   void toggleWindow();
   void setLoginStart(bool on);

   Monitor m_monitor;
   Activity m_activity;
   QPointer<Window> m_window;
   QSystemTrayIcon *m_tray = nullptr;
   QAction *m_loginStart = nullptr;
   int m_trayTries = 0;
};
