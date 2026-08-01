#include "privops.h"

#include "processrunner.h"

#include <QStandardPaths>

static constexpr const char *PKEXEC = "pkexec";
static constexpr const char *CTL = SCX_CTL_PATH;

// ── Singleton ─────────────────────────────────────────────────────────────────

PrivOps *PrivOps::get() {
    static PrivOps *inst = new PrivOps;
    return inst;
}

bool PrivOps::pkexecPresent() { return !QStandardPaths::findExecutable(PKEXEC).isEmpty(); }

// ── Construction ──────────────────────────────────────────────────────────────

PrivOps::PrivOps(QObject *parent) : QObject(parent) {
    m_runner = new ProcessRunner(30000, this);
}

// ── Privileged operations ─────────────────────────────────────────────────────

// All scheduler changes funnel through the single scx-switcher-ctl helper so
// PolKit needs only one action (one password prompt per operation).
void PrivOps::runCtl(const QStringList &args, Callback cb) {
    m_runner->run(PKEXEC, QStringList{CTL} + args, [cb = std::move(cb)](int exit, const QString &output) {
        if (cb)
            cb(exit == 0, output);
    });
}
