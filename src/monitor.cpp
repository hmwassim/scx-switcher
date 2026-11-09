#include "monitor.h"

#include "catalog.h"

static constexpr int WINDOW_MS = 2000;
static constexpr int TRAY_MS = 5000;

Monitor::Monitor(QObject *parent) : QObject(parent) {
   connect(&m_timer, &QTimer::timeout, this, &Monitor::poll);
   poll();
}

style::Tone Monitor::tone() const {
   if (m_busy)
      return style::Tone::Blue;
   if (!m_status.crashed.isEmpty())
      return style::Tone::Red;
   return m_status.kernel == kernel::State::Enabled ? style::Tone::Green : style::Tone::Grey;
}

QString Monitor::summary() const {
   if (m_status.kernel == kernel::State::Unsupported)
      return "sched_ext is not available";
   if (!m_status.crashed.isEmpty())
      return catalog::name(m_status.crashed) + " crashed";
   if (m_status.kernel == kernel::State::Enabled && !m_status.running.isEmpty())
      return catalog::name(m_status.running);
   return "Kernel default";
}

void Monitor::setWindowVisible(bool visible) {
   m_windowVisible = visible;
   updateInterval();
   if (visible)
      poll();
}

void Monitor::setTray(bool tray) {
   m_tray = tray;
   updateInterval();
}

/* While the helper works the kernel briefly has no scheduler, which is not a crash */
void Monitor::setBusy(bool busy) {
   m_busy = busy;
   m_serviceRan = false;
   emit changed();
   if (!busy)
      refresh();
}

void Monitor::refresh() {
   m_status.kernel = kernel::state();
   m_status.running = kernel::running();
   m_seq = kernel::enableSeq();
   queryUnits();
}

void Monitor::clearCrash() {
   m_status.crashed.clear();
   emit changed();
}

void Monitor::updateInterval() {
   const int interval = m_windowVisible ? WINDOW_MS : m_tray ? TRAY_MS : 0;
   if (interval == 0)
      m_timer.stop();
   else
      m_timer.start(interval);
}

void Monitor::poll() {
   const kernel::State state = kernel::state();
   const QString running = kernel::running();
   const quint64 seq = kernel::enableSeq();
   const bool same = state == m_status.kernel && running == m_status.running && seq == m_seq;
   m_status.kernel = state;
   m_status.running = running;
   m_seq = seq;
   if (!same || !m_status.ready)
      queryUnits();
}

void Monitor::queryUnits() {
   if (m_querying)
      return;
   m_querying = true;
   sys::units(this, [this](const sys::Units &units) {
      m_querying = false;
      Status &s = m_status;
      s.units = units;
      s.config = sys::config();
      s.ready = true;

      /* A crash leaves scx.service failed or restarting; a clean stop leaves it inactive */
      const bool attached = s.kernel == kernel::State::Enabled;
      const bool broken = units.state == "failed" || units.state == "activating";
      if (!m_busy && m_serviceRan && s.kernel == kernel::State::Disabled && broken)
         s.crashed = s.config.sched;
      if (attached && units.active)
         s.crashed.clear();
      m_serviceRan = attached && units.active;
      emit changed();
   });
}
