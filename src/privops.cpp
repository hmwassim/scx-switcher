#include "privops.h"

#include "distro.h"
#include "processrunner.h"

static constexpr const char *CTL = SCX_CTL_PATH;

// ── Singleton ─────────────────────────────────────────────────────────────────

PrivOps *PrivOps::get() {
    static PrivOps *inst = new PrivOps;
    return inst;
}

bool PrivOps::escalationAvailable() {
    return DistroInfo::get()->escalation() != DistroInfo::Escalation::None;
}

QString PrivOps::escalationTool() { return DistroInfo::get()->escalationTool(); }

// ── Construction ──────────────────────────────────────────────────────────────

PrivOps::PrivOps(QObject *parent) : QObject(parent) {
    m_runner = new ProcessRunner(30000, this);
}

// ── Privileged operations ─────────────────────────────────────────────────────

// All scheduler changes funnel through the single scx-switcher-ctl helper so
// PolKit needs only one action (one password prompt per operation). The helper
// is invoked via pkexec when available and falls back to sudo otherwise.
void PrivOps::runCtl(const QStringList &args, Callback cb) {
    const QString tool = escalationTool();
    if (tool.isEmpty()) {
        if (cb)
            cb(false, "No privilege escalation tool (pkexec or sudo) is available.");
        return;
    }
    m_runner->run(tool, QStringList{CTL} + args, [cb = std::move(cb)](int exit, const QString &output) {
        if (cb)
            cb(exit == 0, output);
    });
}
