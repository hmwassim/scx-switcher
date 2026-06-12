#include "priv_ops.h"

#include <QProcess>
#include <QTemporaryFile>
#include <QFile>
#include <QCoreApplication>
#include <QStandardPaths>
#include <QDir>

namespace priv_ops {

static const QString PKEXEC   = "pkexec";
static const QString SCXCTL   = "scxctl";
static const QString SYSTEMCTL = "systemctl";
static const QString SERVICE  = "scx_loader.service";

static bool runPrivileged(const QStringList &args, QString *output = nullptr) {
    QProcess proc;
    proc.start(PKEXEC, args);
    if (!proc.waitForFinished(60000))
        return false;
    int code = proc.exitCode();
    if (output)
        *output = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
    return code == 0;
}

bool isPolkitAgentRunning() {
    return true;
}

bool startScheduler(const QString &sched, const QString &mode) {
    return runPrivileged({SCXCTL, "start", "--sched", sched, "--mode", mode});
}

bool stopScheduler() {
    return runPrivileged({SCXCTL, "stop"});
}

bool switchScheduler(const QString &sched, const QString &mode) {
    return runPrivileged({SCXCTL, "switch", "--sched", sched, "--mode", mode});
}

bool enableService() {
    return runPrivileged({SYSTEMCTL, "enable", "--now", SERVICE});
}

bool disableService() {
    return runPrivileged({SYSTEMCTL, "disable", "--now", SERVICE});
}

void writeConfigToml(const QString &sched, const QString &mode) {
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
    if (!tmp.open())
        return;
    tmp.write(content.toUtf8());
    QString tmpName = tmp.fileName();
    tmp.close();

    runPrivileged({"cp", tmpName, "/etc/scx_loader/config.toml"});
    QFile::remove(tmpName);
}

} // namespace priv_ops
