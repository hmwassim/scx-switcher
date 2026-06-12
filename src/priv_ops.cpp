#include "priv_ops.h"

#include <QProcess>
#include <QTemporaryFile>
#include <QFile>
#include <QDir>
#include <QStandardPaths>

static const QString SCX_LOADER_CONFIG  = "/etc/scx_loader/config.toml";

#ifndef SCXCTL_BIN
#define SCXCTL_BIN "scxctl"
#endif
#ifndef SYSTEMCTL_BIN
#define SYSTEMCTL_BIN "systemctl"
#endif
#ifndef PKEXEC_BIN
#define PKEXEC_BIN "pkexec"
#endif

static const QString PKEXEC    = PKEXEC_BIN;
static const QString SCXCTL    = SCXCTL_BIN;
static const QString SYSTEMCTL = SYSTEMCTL_BIN;
static const QString SERVICE   = "scx_loader.service";

PrivOps *PrivOps::instance() {
    static PrivOps *inst = new PrivOps;
    return inst;
}

PrivOps::PrivOps(QObject *parent) : QObject(parent) {
    m_proc = new QProcess(this);
    m_proc->setProcessChannelMode(QProcess::SeparateChannels);

    connect(m_proc, &QProcess::finished, this, [this](int exitCode, QProcess::ExitStatus status) {
        bool success = (status == QProcess::NormalExit && exitCode == 0);
        QString stdOut = QString::fromUtf8(m_proc->readAllStandardOutput()).trimmed();
        QString stdErr = QString::fromUtf8(m_proc->readAllStandardError()).trimmed();

        if (!m_lastTmpFile.isEmpty()) {
            QFile::remove(m_lastTmpFile);
            m_lastTmpFile.clear();
        }

        if (m_callback) {
            QString msg = success ? stdOut : (stdErr.isEmpty() ? stdOut : stdErr);
            auto cb = std::move(m_callback);
            m_callback = nullptr;
            cb(success, msg);
        }
    });
}

PrivOps::~PrivOps() {
    if (!m_lastTmpFile.isEmpty())
        QFile::remove(m_lastTmpFile);
}

bool PrivOps::isPolkitAgentRunning() {
    return !QStandardPaths::findExecutable(PKEXEC).isEmpty();
}

void PrivOps::runPrivileged(const QStringList &args, Callback cb) {
    m_callback = std::move(cb);
    if (!m_lastTmpFile.isEmpty()) {
        QFile::remove(m_lastTmpFile);
        m_lastTmpFile.clear();
    }
    m_proc->start(PKEXEC, args);
}

void PrivOps::startScheduler(const QString &sched, const QString &mode, Callback cb) {
    runPrivileged({SCXCTL, "start", "--sched", sched, "--mode", mode}, std::move(cb));
}

void PrivOps::stopScheduler(Callback cb) {
    runPrivileged({SCXCTL, "stop"}, std::move(cb));
}

void PrivOps::switchScheduler(const QString &sched, const QString &mode, Callback cb) {
    runPrivileged({SCXCTL, "switch", "--sched", sched, "--mode", mode}, std::move(cb));
}

void PrivOps::enableService(Callback cb) {
    runPrivileged({SYSTEMCTL, "enable", "--now", SERVICE}, std::move(cb));
}

void PrivOps::disableService(Callback cb) {
    runPrivileged({SYSTEMCTL, "disable", "--now", SERVICE}, std::move(cb));
}

void PrivOps::writeConfigToml(const QString &sched, const QString &mode, Callback cb) {
    static const QHash<QString, QString> modeMap = {
        {"auto", "Auto"}, {"gaming", "Gaming"},
        {"lowlatency", "LowLatency"}, {"powersave", "PowerSave"},
        {"server", "Server"},
    };
    QString m = modeMap.value(mode.toLower(), mode);
    QString content = QString(
        "default_sched = \"scx_%1\"\n"
        "default_mode = \"%2\"\n"
    ).arg(sched, m);

    QTemporaryFile tmp(QDir::tempPath() + "/scx-switcher-XXXXXX.toml");
    tmp.setAutoRemove(false);
    if (!tmp.open()) {
        if (cb) cb(false, "Failed to create config file");
        return;
    }
    tmp.write(content.toUtf8());
    m_lastTmpFile = tmp.fileName();
    tmp.close();

    runPrivileged({"cp", m_lastTmpFile, SCX_LOADER_CONFIG}, std::move(cb));
}
