#pragma once

#include <QString>
#include <QStringList>
#include <functional>

class QObject;

namespace scx {

// What scx_loader believes is running. It keeps reporting a scheduler after
// that scheduler has died, so check kernelState() before trusting it.
struct Status {
    bool running = false;
    QString sched; // bare lowercase name, e.g. "bpfland"
    QString mode;  // lowercase scxctl mode, e.g. "lowlatency"
};

// Parses `scxctl get`, e.g. "running Bpfland in Server mode".
Status parseStatus(const QString &output);

// Parses `scxctl list`, e.g. `supported schedulers: ["bpfland", "lavd"]`.
QStringList parseList(const QString &output);

// Parses `scxctl modes`, e.g. "modes configured for Bpfland: [Auto, LowLatency]".
QStringList parseModes(const QString &output);

// Absolute paths of the sched_ext programs on PATH (scx_*, scxctl, scxtop).
QStringList installedPrograms();

enum class Kernel { Unsupported, Disabled, Enabled, Changing };
Kernel kernelState(); // from /sys/kernel/sched_ext/state

// The scheduler scx_loader starts at boot, from its config file.
struct BootDefault {
    bool emptyConfig = false; // scx_loader refuses to start with an empty file
    QString sched;            // empty if none is set
    QString mode;
};
BootDefault bootDefault();

bool scxctlAvailable();
bool pkexecAvailable();

// Runs a program without blocking and calls done(exitCode, output) once.
// exitCode is -1 if the program failed to start, crashed or timed out.
using Done = std::function<void(int exitCode, const QString &output)>;
void run(QObject *context, const QString &program, const QStringList &args, Done done,
         int timeoutMs = 10000);

} // namespace scx
