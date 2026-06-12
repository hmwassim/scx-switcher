#include "scx_utils.h"

#include <QRegularExpression>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QProcess>
#include <QSysInfo>
#include <QFile>

const QString ScxUtils::SCXCTL   = "scxctl";
const QString ScxUtils::SYSTEMCTL = "systemctl";
const QString ScxUtils::SERVICE  = "scx_loader.service";

ScxUtils::ScxUtils(QObject *parent) : QObject(parent) {
    m_proc = new QProcess(this);
    m_proc->setProcessChannelMode(QProcess::MergedChannels);

    connect(m_proc, &QProcess::finished, this, [this](int exitCode) {
        QString out = QString::fromUtf8(m_proc->readAll()).trimmed();

        switch (m_currentOp) {
        case KernelCheck:
            handleKernelCheck(out);
            break;
        case Status:
            handleStatus(exitCode, out);
            break;
        case List:
            handleList(exitCode, out);
            break;
        case ServiceCheck:
            handleServiceCheck(exitCode, out);
            break;
        default:
            break;
        }

        m_currentOp = None;
        processNext();
    });
}

bool ScxUtils::isToolInstalled(const QString &name) {
    return !QStandardPaths::findExecutable(name).isEmpty();
}

bool ScxUtils::isScxctlInstalled() {
    return isToolInstalled(SCXCTL);
}

void ScxUtils::enqueue(Op op, const QString &program, const QStringList &args) {
    m_queue.enqueue({op, program, args});
    if (m_currentOp == None)
        processNext();
}

void ScxUtils::processNext() {
    if (m_queue.isEmpty()) return;

    auto next = m_queue.dequeue();
    m_currentOp = next.op;
    m_proc->start(next.program, next.args);
}

void ScxUtils::checkKernelSupport() {
    enqueue(KernelCheck, "uname", {"-r"});
}

void ScxUtils::getSchedulerStatus() {
    enqueue(Status, SCXCTL, {"get"});
}

void ScxUtils::listSchedulers() {
    enqueue(List, SCXCTL, {"list"});
}

void ScxUtils::checkServiceEnabled() {
    enqueue(ServiceCheck, SYSTEMCTL, {"is-enabled", SERVICE});
}

void ScxUtils::handleKernelCheck(const QString &out) {
    QString kernel = out;
    if (kernel.isEmpty())
        kernel = QSysInfo::kernelVersion();

    QStringList parts = kernel.split('.');
    if (parts.size() < 2) {
        emit kernelSupportChecked(false, "Unknown");
        return;
    }

    bool ok1 = false, ok2 = false;
    int major = parts[0].toInt(&ok1);
    int minor = parts[1].split('-')[0].toInt(&ok2);

    bool versionOk = ok1 && ok2 && (major > 6 || (major == 6 && minor >= 12));
    bool sysfsOk = QDir("/sys/kernel/sched_ext").exists();

    if (versionOk && sysfsOk)
        emit kernelSupportChecked(true, QString("Supported \u2014 Linux %1").arg(kernel));
    else if (versionOk && !sysfsOk)
        emit kernelSupportChecked(false, QString("Kernel %1 meets 6.12+ but /sys/kernel/sched_ext not found").arg(kernel));
    else
        emit kernelSupportChecked(false, QString("Kernel %1 \u2014 6.12+ required").arg(kernel));
}

void ScxUtils::handleStatus(int exitCode, const QString &out) {
    SchedStatus s;
    s.name = "EEVDF";
    s.mode = "N/A";

    if (exitCode != 0 || out.isEmpty() || out.contains("not running", Qt::CaseInsensitive) ||
        !out.startsWith("running", Qt::CaseInsensitive)) {
        emit schedulerStatusReady(s);
        return;
    }

    QStringList words = out.split(' ', Qt::SkipEmptyParts);
    if (words.size() < 2) {
        emit schedulerStatusReady(s);
        return;
    }

    s.active = true;
    s.name = words[1];

    QRegularExpression re("in (\\S+) mode", QRegularExpression::CaseInsensitiveOption);
    auto m = re.match(out);
    s.mode = m.hasMatch() ? m.captured(1) : "N/A";

    emit schedulerStatusReady(s);
}

void ScxUtils::handleList(int exitCode, const QString &out) {
    if (exitCode != 0) {
        emit schedulersListed({});
        return;
    }

    int idx = out.indexOf("supported schedulers:");
    if (idx == -1) {
        emit schedulersListed({});
        return;
    }

    QString json = out.mid(idx + QString("supported schedulers:").length()).trimmed();
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isArray()) {
        emit schedulersListed({});
        return;
    }

    QStringList list;
    for (const auto &v : doc.array())
        list.append(v.toString());
    emit schedulersListed(list);
}

void ScxUtils::handleServiceCheck(int exitCode, const QString &out) {
    emit serviceEnabledChecked(exitCode == 0 && out.contains("enabled"));
}

QStringList ScxUtils::supportedModes(const QString &schedBareName) {
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

QString ScxUtils::humanizeScheduler(const QString &name) {
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

QString ScxUtils::humanizeMode(const QString &mode) {
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
    return dir + "/scx-switcher/state.json";
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

void ScxUtils::saveState(const QString &sched, const QString &mode) {
    QJsonObject obj = readStateFile();
    obj["scheduler"] = sched;
    obj["mode"] = mode;
    writeStateFile(obj);
}

QPair<QString, QString> ScxUtils::loadState() {
    QJsonObject obj = readStateFile();
    return {obj["scheduler"].toString(), obj["mode"].toString()};
}
