#include "scxutils.h"

#include <QProcess>
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QSysInfo>
#include <QDebug>

static constexpr int QUERY_TIMEOUT_MS = 15000;  // timeout for info queries

ScxUtils *ScxUtils::get() {
    static ScxUtils *inst = new ScxUtils;
    return inst;
}

ScxUtils::ScxUtils(QObject *parent) : QObject(parent) {
    m_proc = new QProcess(this);
    m_proc->setProcessChannelMode(QProcess::SeparateChannels);

    m_timeout = new QTimer(this);
    m_timeout->setSingleShot(true);

    connect(m_proc, &QProcess::finished, this, [this](int exit) {
        m_timeout->stop();
        const Op op = m_current;
        m_current = Op::None;

        if (op == Op::None) {
            // errorOccurred(FailedToStart) already dispatched and called runNext.
            return;
        }

        if (!m_pendingError.isEmpty()) {
            const QString msg = m_pendingError;
            m_pendingError.clear();
            dispatchError(op, msg);
            if (m_current == Op::None)
                runNext();
            return;
        }

        const QString out = QString::fromUtf8(m_proc->readAllStandardOutput()).trimmed();

        switch (op) {
        case Op::Kernel:  onKernel(out);        break;
        case Op::Status:  onStatus(exit, out);  break;
        case Op::List:    onList(exit, out);     break;
        case Op::Service: onService(exit, out);  break;
        default: break;
        }

        // If a signal handler re-entrantly enqueued + ran the next job,
        // m_current is non-None and we must not call runNext again.
        if (m_current == Op::None)
            runNext();
    });

    connect(m_proc, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        if (m_current == Op::None) return;
        // FailedToStart: finished never fires, so we handle it here.
        // Crashed: fall through — let finished handle it with the real exit status.
        if (m_proc->error() != QProcess::FailedToStart) return;

        m_timeout->stop();
        const Op op = m_current;
        m_current = Op::None;

        dispatchError(op, QString("Cannot determine %1")
                      .arg(op == Op::Kernel ? "kernel version"
                                            : "process status"));
        runNext();
    });

    connect(m_timeout, &QTimer::timeout, this, [this]() {
        if (m_current == Op::None) return;

        // Record the error and kill the process. Do NOT emit or mutate
        // m_current here — finished will fire asynchronously, see the
        // pending error, and dispatch the result. This avoids re-entrancy
        // from synchronous signal delivery (Bug 2).
        m_pendingError = "Operation timed out";
        if (m_proc->state() != QProcess::NotRunning)
            m_proc->kill();
    });
}

bool ScxUtils::scxctlPresent() {
    return !QStandardPaths::findExecutable("scxctl").isEmpty();
}

void ScxUtils::checkKernel()        { enqueue(Op::Kernel,  "uname",    {"-r"}); }
void ScxUtils::queryStatus()        { enqueue(Op::Status,  "scxctl",   {"get"}); }
void ScxUtils::listSchedulers()     { enqueue(Op::List,    "scxctl",   {"list"}); }
void ScxUtils::checkServiceEnabled(){ enqueue(Op::Service, "systemctl",{"is-enabled", "scx_loader.service"}); }

void ScxUtils::enqueue(Op op, const QString &prog, const QStringList &args) {
    m_queue.enqueue({op, prog, args});
    if (m_current == Op::None)
        runNext();
}

void ScxUtils::runNext() {
    if (m_queue.isEmpty()) return;
    const auto job = m_queue.dequeue();
    m_current = job.op;
    m_timeout->start(QUERY_TIMEOUT_MS);
    m_proc->start(job.program, job.args);
}

void ScxUtils::dispatchError(Op op, const QString &msg) {
    switch (op) {
    case Op::Kernel:
        emit kernelChecked(false, msg);
        break;
    case Op::Status: {
        SchedStatus s;
        emit statusReady(s);
        break;
    }
    case Op::List:
        emit schedulersListed({});
        break;
    case Op::Service:
        emit serviceEnabled(false);
        break;
    default: break;
    }
}

void ScxUtils::onKernel(const QString &out) {
    QString ver = out.isEmpty() ? QSysInfo::kernelVersion() : out;
    const QStringList parts = ver.split('.');
    bool ok1 = false, ok2 = false;
    const int major = parts.value(0).toInt(&ok1);
    const int minor = parts.value(1).split('-').value(0).toInt(&ok2);

    const bool versionOk = ok1 && ok2 && (major > 6 || (major == 6 && minor >= 12));
    const bool sysfsOk   = QDir("/sys/kernel/sched_ext").exists();

    if (versionOk && sysfsOk)
        emit kernelChecked(true,  QString("Linux %1 \xe2\x80\x94 sched_ext active").arg(ver));
    else if (versionOk)
        emit kernelChecked(false, QString("Linux %1 \xe2\x80\x94 /sys/kernel/sched_ext not found").arg(ver));
    else
        emit kernelChecked(false, QString("Linux %1 \xe2\x80\x94 kernel 6.12+ required").arg(ver));
}

void ScxUtils::onStatus(int exit, const QString &out) {
    SchedStatus s;
    if (exit != 0 || out.isEmpty()
            || out.contains("not running", Qt::CaseInsensitive)
            || !out.startsWith("running", Qt::CaseInsensitive)) {
        emit statusReady(s);
        return;
    }

    const QStringList words = out.split(' ', Qt::SkipEmptyParts);
    if (words.size() < 2) { emit statusReady(s); return; }

    s.active = true;
    s.name   = words[1];
    if (s.name.startsWith("scx_"))
        s.name = s.name.mid(4);

    const QRegularExpression re("in (\\S+) mode", QRegularExpression::CaseInsensitiveOption);
    const auto m = re.match(out);
    s.mode = m.hasMatch() ? m.captured(1).toLower() : "auto";

    emit statusReady(s);
}

void ScxUtils::onList(int exit, const QString &out) {
    if (exit != 0) { emit schedulersListed({}); return; }

    const int idx = out.indexOf("supported schedulers:");
    if (idx == -1) { emit schedulersListed({}); return; }

    const QByteArray json = out.mid(idx + static_cast<int>(strlen("supported schedulers:")))
                                .trimmed().toUtf8();
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &err);
    if (err.error != QJsonParseError::NoError || !doc.isArray()) {
        emit schedulersListed({});
        return;
    }

    QStringList list;
    for (const auto &v : doc.array()) {
        QString name = v.toString();
        if (name.startsWith("scx_")) name = name.mid(4);
        list.append(name);
    }
    emit schedulersListed(list);
}

void ScxUtils::onService(int exit, const QString &out) {
    emit serviceEnabled(exit == 0 && out.contains("enabled"));
}

static QString statePath() {
    const QString base = qEnvironmentVariable("XDG_STATE_HOME",
                                              QDir::homePath() + "/.local/state");
    return base + "/scx-switcher/state.json";
}

void ScxUtils::saveState(const QString &sched, const QString &mode) {
    const QString path = statePath();
    QDir().mkpath(QFileInfo(path).absolutePath());

    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        qWarning("saveState: cannot open %s", qPrintable(path));
        return;
    }

    QJsonObject obj;
    obj["scheduler"] = sched;
    obj["mode"]      = mode;

    f.write(QJsonDocument(obj).toJson());

    if (!f.commit()) {
        qWarning("saveState: commit failed for %s", qPrintable(path));
        f.cancelWriting();
    }
}

QPair<QString,QString> ScxUtils::loadState() {
    QFile f(statePath());
    if (!f.open(QIODevice::ReadOnly)) return {};
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject()) return {};
    const QJsonObject obj = doc.object();
    return {obj["scheduler"].toString(), obj["mode"].toString()};
}
