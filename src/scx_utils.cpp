#include "scx_utils.h"

#include <QRegularExpression>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QProcess>
#include <QCoreApplication>

namespace scx_utils {

static const QString SCXCTL   = "scxctl";
static const QString SYSTEMCTL = "systemctl";
static const QString SERVICE  = "scx_loader.service";

bool isToolInstalled(const QString &name) {
    return !QStandardPaths::findExecutable(name).isEmpty();
}

bool isScxctlInstalled() {
    return isToolInstalled(SCXCTL);
}

std::pair<bool, QString> checkKernelSupport() {
    QProcess proc;
    proc.start("uname", {"-r"});
    proc.waitForFinished(5000);
    QString kernel = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();

    QStringList parts = kernel.split('.');
    if (parts.size() < 2)
        return {false, "Unknown"};

    bool ok1 = false, ok2 = false;
    int major = parts[0].toInt(&ok1);
    int minor = parts[1].split('-')[0].toInt(&ok2);

    bool versionOk = ok1 && ok2 && (major > 6 || (major == 6 && minor >= 12));
    bool sysfsOk = QDir("/sys/kernel/sched_ext").exists();

    if (versionOk && sysfsOk)
        return {true, QString("Supported \u2014 Linux %1").arg(kernel)};
    if (versionOk && !sysfsOk)
        return {false, QString("Kernel %1 meets 6.12+ but /sys/kernel/sched_ext not found").arg(kernel)};
    return {false, QString("Kernel %1 \u2014 6.12+ required").arg(kernel)};
}

SchedStatus getSchedulerStatus() {
    SchedStatus s;
    s.name = "EEVDF";
    s.mode = "N/A";

    QProcess proc;
    proc.start(SCXCTL, {"get"});
    proc.waitForFinished(5000);

    if (proc.exitCode() != 0)
        return s;

    QString out = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
    if (out.isEmpty() || out.contains("not running", Qt::CaseInsensitive))
        return s;

    if (!out.startsWith("running", Qt::CaseInsensitive))
        return s;

    QStringList words = out.split(' ', Qt::SkipEmptyParts);
    if (words.size() < 2)
        return s;

    s.active = true;
    s.name = words[1];

    QRegularExpression re("in (\\S+) mode", QRegularExpression::CaseInsensitiveOption);
    auto m = re.match(out);
    s.mode = m.hasMatch() ? m.captured(1) : "N/A";

    return s;
}

QStringList listSchedulers() {
    QProcess proc;
    proc.start(SCXCTL, {"list"});
    proc.waitForFinished(10000);

    if (proc.exitCode() != 0)
        return {};

    QString out = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
    int idx = out.indexOf("supported schedulers:");
    if (idx == -1)
        return {};

    QString json = out.mid(idx + QString("supported schedulers:").length()).trimmed();
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isArray())
        return {};

    QStringList list;
    for (const auto &v : doc.array())
        list.append(v.toString());
    return list;
}

bool isServiceEnabled() {
    QProcess proc;
    proc.start(SYSTEMCTL, {"is-enabled", SERVICE});
    proc.waitForFinished(5000);
    return proc.exitCode() == 0 &&
           QString::fromUtf8(proc.readAllStandardOutput()).contains("enabled");
}

QStringList supportedModes(const QString &schedBareName) {
    static const QHash<QString, QStringList> map = {
        {"bpfland",   {"auto", "gaming", "lowlatency", "powersave", "server"}},
        {"lavd",      {"auto", "gaming", "lowlatency", "powersave", "server"}},
        {"rusty",     {"auto", "gaming"}},
        {"flash",     {"auto", "gaming", "powersave"}},
        {"cosmos",    {"auto", "gaming", "lowlatency", "powersave", "server"}},
        {"layered",   {"auto"}},
        {"nest",      {"auto", "powersave"}},
        {"p2dq",      {"auto", "gaming", "powersave", "server"}},
        {"tickless",  {"auto", "powersave", "server"}},
        {"simple",    {"auto"}},
        {"beerland",  {"auto", "gaming", "lowlatency", "powersave", "server"}},
        {"rustland",  {"auto"}},
    };
    return map.value(schedBareName, {"auto"});
}

QString humanizeScheduler(const QString &name) {
    QString n = name;
    if (n.startsWith("scx_"))
        n = n.mid(4);

    static const QHash<QString, QString> overrides = {
        {"bpfland", "BPFland"}, {"beerland", "Beerland"},
        {"rustland", "Rustland"}, {"lavd", "LAVD"}, {"p2dq", "P2DQ"},
    };
    if (overrides.contains(n))
        return overrides[n];

    if (n.isEmpty()) return name;
    n[0] = n[0].toUpper();
    return n;
}

QString humanizeMode(const QString &mode) {
    if (mode.isEmpty() || mode == "N/A") return mode;
    static const QHash<QString, QString> map = {
        {"auto", "Auto"}, {"gaming", "Gaming"},
        {"lowlatency", "Low Latency"}, {"powersave", "Power Save"},
        {"server", "Server"},
    };
    return map.value(mode.toLower(), mode);
}

static QString stateFilePath() {
    QString dir;
    if (qEnvironmentVariableIsEmpty("XDG_STATE_HOME"))
        dir = QDir::homePath() + "/.local/state";
    else
        dir = qgetenv("XDG_STATE_HOME");
    return dir + "/debforge-scx/state.json";
}

static QJsonObject readStateFile() {
    QFile f(stateFilePath());
    if (!f.open(QIODevice::ReadOnly))
        return {};
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    return doc.isObject() ? doc.object() : QJsonObject();
}

static void writeStateFile(const QJsonObject &obj) {
    QDir().mkpath(QFileInfo(stateFilePath()).absolutePath());
    QFile f(stateFilePath());
    if (f.open(QIODevice::WriteOnly))
        f.write(QJsonDocument(obj).toJson());
}

void saveState(const QString &sched, const QString &mode) {
    QJsonObject obj = readStateFile();
    obj["scheduler"] = sched;
    obj["mode"] = mode;
    writeStateFile(obj);
}

QPair<QString, QString> loadState() {
    QJsonObject obj = readStateFile();
    return {obj["scheduler"].toString(), obj["mode"].toString()};
}

} // namespace scx_utils
