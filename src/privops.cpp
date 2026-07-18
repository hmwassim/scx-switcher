#include "privops.h"

#include "processrunner.h"
#include "schedcatalog.h"

#include <QStandardPaths>

static constexpr const char *SCXCTL = "scxctl";
static constexpr const char *PKEXEC = "pkexec";
static constexpr const char *TOGGLE_AUTOSTART = "/usr/libexec/scx-switcher/toggle-autostart";
static constexpr const char *WRITE_CONFIG = "/usr/libexec/scx-switcher/write-config";

// ── Singleton ─────────────────────────────────────────────────────────────────

PrivOps *PrivOps::get() {
    static PrivOps *inst = new PrivOps;
    return inst;
}

bool PrivOps::pkexecPresent() { return !QStandardPaths::findExecutable(PKEXEC).isEmpty(); }

QString PrivOps::checkPolicyPath() {
    const QString resolved = QStandardPaths::findExecutable("scxctl");
    const QString expected = "/usr/bin/scxctl";
    if (resolved.isEmpty() || resolved == expected)
        return {};
    return QString("scxctl found at %1, but the auth policy expects %2"
                   " \xe2\x80\x94 you may be prompted for a full admin"
                   " password on every scheduler change.")
        .arg(resolved, expected);
}

// ── Construction ──────────────────────────────────────────────────────────────

PrivOps::PrivOps(QObject *parent) : QObject(parent) {
    m_runner = new ProcessRunner(30000, this);
}

// ── Internal ──────────────────────────────────────────────────────────────────

void PrivOps::run(const QStringList &args, Callback cb) {
    m_runner->run(PKEXEC, args, [cb = std::move(cb)](int exit, const QString &output) {
        if (cb)
            cb(exit == 0, output);
    });
}

// ── Public operations ─────────────────────────────────────────────────────────

void PrivOps::startScheduler(const QString &sched, const QString &mode, Callback cb) {
    run({SCXCTL, "start", "--sched", sched, "--mode", mode}, std::move(cb));
}

void PrivOps::switchScheduler(const QString &sched, const QString &mode, Callback cb) {
    run({SCXCTL, "switch", "--sched", sched, "--mode", mode}, std::move(cb));
}

void PrivOps::stopScheduler(Callback cb) { run({SCXCTL, "stop"}, std::move(cb)); }

void PrivOps::enableService(Callback cb) { run({TOGGLE_AUTOSTART, "enable"}, std::move(cb)); }

void PrivOps::disableService(Callback cb) { run({TOGGLE_AUTOSTART, "disable"}, std::move(cb)); }

void PrivOps::writeConfig(const QString &sched, const QString &mode, Callback cb) {
    const QString content = QString("default_sched = \"scx_%1\"\n"
                                    "default_mode  = \"%2\"\n")
                                .arg(sched, SchedCatalog::get()->tomlMode(mode));

    m_runner->runWithContent(PKEXEC, {WRITE_CONFIG}, content,
                             [cb = std::move(cb)](int exit, const QString &output) {
                                 if (cb)
                                     cb(exit == 0, output);
                             });
}
