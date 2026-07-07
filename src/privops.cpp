#include "privops.h"

#include <QProcess>
#include <QHash>
#include <QStandardPaths>

static constexpr const char *SCXCTL            = "scxctl";
static constexpr const char *PKEXEC            = "pkexec";
static constexpr const char *TOGGLE_AUTOSTART  = "/usr/libexec/scx-switcher/toggle-autostart";
static constexpr const char *WRITE_CONFIG      = "/usr/libexec/scx-switcher/write-config";

static constexpr int OP_TIMEOUT_MS = 30000;

// ── TOML mode names ───────────────────────────────────────────────────────────
//
// humanizeMode() is for display only ("Low Latency", "Power Save").
// scx_loader config.toml requires the enum variant names ("LowLatency", "PowerSave").
// These must NOT be conflated.

static QString tomlMode(const QString &mode) {
    static const QHash<QString, QString> map = {
        {"auto",       "Auto"},
        {"gaming",     "Gaming"},
        {"lowlatency", "LowLatency"},
        {"powersave",  "PowerSave"},
        {"server",     "Server"},
    };
    return map.value(mode.toLower(), "Auto");
}

// ── Singleton ─────────────────────────────────────────────────────────────────

PrivOps *PrivOps::get() {
    static PrivOps *inst = new PrivOps;
    return inst;
}

bool PrivOps::pkexecPresent() {
    return !QStandardPaths::findExecutable(PKEXEC).isEmpty();
}

// ── Construction / destruction ────────────────────────────────────────────────

PrivOps::PrivOps(QObject *parent) : QObject(parent) {
    m_timeout = new QTimer(this);
    m_timeout->setSingleShot(true);

    m_proc = new QProcess(this);
    m_proc->setProcessChannelMode(QProcess::SeparateChannels);

    // Write pending config content via stdin once the process starts.
    // Used by writeConfig() to pipe content to pkexec tee — avoids
    // the TOCTOU symlink race of the temp-file + cp approach.
    connect(m_proc, &QProcess::started, this, [this]() {
        if (!m_configContent.isEmpty()) {
            m_proc->write(m_configContent.toUtf8());
            m_proc->closeWriteChannel();
            m_configContent.clear();
        }
    });

    connect(m_proc, &QProcess::finished, this,
            [this](int exit, QProcess::ExitStatus status) {
        m_timeout->stop();
        m_configContent.clear();

        const bool ok  = (status == QProcess::NormalExit && exit == 0);
        const QString out = QString::fromUtf8(m_proc->readAllStandardOutput()).trimmed();
        const QString err = QString::fromUtf8(m_proc->readAllStandardError()).trimmed();

        if (m_callback) {
            auto cb = std::move(m_callback);
            m_callback = nullptr;
            cb(ok, ok ? out : (err.isEmpty() ? out : err));
        }

        // Drain any pending op queued while the previous process was in-flight.
        if (m_pendingOp && m_proc->state() == QProcess::NotRunning) {
            auto op = std::move(*m_pendingOp);
            m_pendingOp.reset();
            m_callback = std::move(op.callback);
            if (!op.configContent.isEmpty()) {
                m_configContent = op.configContent;
                m_timeout->start(OP_TIMEOUT_MS);
                m_proc->start(PKEXEC, QStringList{WRITE_CONFIG});
            } else {
                m_timeout->start(OP_TIMEOUT_MS);
                m_proc->start(PKEXEC, op.args);
            }
        }
    });

    connect(m_proc, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        if (m_proc->error() == QProcess::FailedToStart) {
            m_timeout->stop();
            m_configContent.clear();

            const QString msg = QString("Failed to start privileged operation: %1")
                                .arg(m_proc->errorString());

            if (m_callback) {
                auto cb = std::move(m_callback);
                m_callback = nullptr;
                cb(false, msg);
            }

            // A pending op would also fail to start — don't retry it.
            if (m_pendingOp) {
                auto op = std::move(*m_pendingOp);
                m_pendingOp.reset();
                if (op.callback)
                    op.callback(false, msg);
            }
        }
    });

    connect(m_timeout, &QTimer::timeout, this, [this]() {
        if (m_proc->state() == QProcess::NotRunning) return;
        m_configContent.clear();
        if (m_callback) {
            auto cb = std::move(m_callback);
            m_callback = nullptr;
            cb(false, "Operation timed out");
        }
        m_proc->kill();
    });
}

PrivOps::~PrivOps() {
    if (m_proc->state() != QProcess::NotRunning) {
        m_proc->kill();
        m_proc->waitForFinished(5000);
    }
}

// ── Internal ──────────────────────────────────────────────────────────────────

void PrivOps::cancelInFlight() {
    if (m_proc->state() == QProcess::NotRunning) return;
    m_timeout->stop();
    m_configContent.clear();
    // Always fire the old callback so the caller isn't left hanging.
    if (m_callback) {
        auto cb = std::move(m_callback);
        m_callback = nullptr;
        cb(false, "Superseded by new operation");
    }
    m_proc->kill();
}

void PrivOps::run(const QStringList &args, Callback cb) {
    if (m_proc->state() != QProcess::NotRunning) {
        cancelInFlight();
        m_pendingOp = PendingOp{args, {}, std::move(cb)};
        return;
    }
    m_callback = std::move(cb);
    m_timeout->start(OP_TIMEOUT_MS);
    m_proc->start(PKEXEC, args);
}

// ── Public operations ─────────────────────────────────────────────────────────

void PrivOps::startScheduler(const QString &sched, const QString &mode, Callback cb) {
    run({SCXCTL, "start", "--sched", sched, "--mode", mode}, std::move(cb));
}

void PrivOps::switchScheduler(const QString &sched, const QString &mode, Callback cb) {
    run({SCXCTL, "switch", "--sched", sched, "--mode", mode}, std::move(cb));
}

void PrivOps::stopScheduler(Callback cb) {
    run({SCXCTL, "stop"}, std::move(cb));
}

void PrivOps::enableService(Callback cb) {
    run({TOGGLE_AUTOSTART, "enable"}, std::move(cb));
}

void PrivOps::disableService(Callback cb) {
    run({TOGGLE_AUTOSTART, "disable"}, std::move(cb));
}

void PrivOps::writeConfig(const QString &sched, const QString &mode, Callback cb) {
    // Use tomlMode() — NOT humanizeMode() — to get the correct TOML enum variant.
    // humanizeMode("lowlatency") = "Low Latency" (display only)
    // tomlMode("lowlatency")     = "LowLatency"  (what scx_loader config.toml expects)
    const QString content = QString(
        "default_sched = \"scx_%1\"\n"
        "default_mode  = \"%2\"\n"
    ).arg(sched, tomlMode(mode));

    if (m_proc->state() != QProcess::NotRunning) {
        cancelInFlight();
        m_pendingOp = PendingOp{{}, content, std::move(cb)};
        return;
    }
    m_configContent = content;
    m_callback = std::move(cb);
    m_timeout->start(OP_TIMEOUT_MS);
    // Content is written to stdin in the QProcess::started handler.
    m_proc->start(PKEXEC, QStringList{WRITE_CONFIG});
}
