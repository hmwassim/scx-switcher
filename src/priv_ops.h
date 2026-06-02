#pragma once

#include <QString>

namespace priv_ops {

bool isPolkitAgentRunning();

bool startScheduler(const QString &sched, const QString &mode);
bool stopScheduler();
bool switchScheduler(const QString &sched, const QString &mode);

bool enableService();
bool disableService();

void writeConfigToml(const QString &sched, const QString &mode);

} // namespace priv_ops
