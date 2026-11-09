#pragma once

#include <QString>
#include <QStringList>
#include <functional>

class QObject;

namespace sys {

/* Exit code -1 means the program failed to start, crashed or timed out */
using Done = std::function<void(int code, const QString &output)>;

void run(QObject *context, const QString &program, const QStringList &args, Done done,
         int timeoutMs = 10000);

/* Runs the root helper through pkexec, waiting long enough for a password */
void helper(QObject *context, const QStringList &args, Done done);

struct Units {
   QString state;             // scx.service ActiveState: "active", "failed", "activating"...
   bool active = false;       // scx.service
   bool enabled = false;      // scx.service at boot
   bool loaderActive = false; // scx_loader.service
};
void units(QObject *context, std::function<void(const Units &)> done);

/* What /etc/default/scx asks scx.service to run */
struct Config {
   QString sched;
   QStringList options;
};
Config config();

bool pkexecAvailable();

} // namespace sys
