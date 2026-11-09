#pragma once

#include "kernel.h"
#include "style.h"
#include "system.h"

#include <QObject>
#include <QTimer>

/* What is running, read from the kernel; systemd is only asked when that changes */
struct Status {
   bool ready = false; // units known at least once
   kernel::State kernel = kernel::State::Unsupported;
   QString running;    // program name from the kernel
   sys::Units units;
   sys::Config config;
   QString crashed;    // program that stopped without being asked to
};

class Monitor : public QObject {
   Q_OBJECT
public:
   explicit Monitor(QObject *parent = nullptr);

   const Status &status() const { return m_status; }
   bool busy() const { return m_busy; }
   style::Tone tone() const;
   QString summary() const;

   void setWindowVisible(bool visible);
   void setTray(bool tray);
   void setBusy(bool busy);
   void refresh();
   void clearCrash();

signals:
   void changed();

private:
   void poll();
   void queryUnits();
   void updateInterval();

   QTimer m_timer;
   Status m_status;
   quint64 m_seq = 0;
   bool m_busy = false;
   bool m_querying = false;
   bool m_serviceRan = false; // last check: scx.service had a scheduler attached
   bool m_windowVisible = false;
   bool m_tray = false;
};
