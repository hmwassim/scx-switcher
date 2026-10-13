#include "scx.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>
#include <QTimer>
#include <memory>
#include <utility>

namespace scx {

Status parseStatus(const QString &output) {
    static const QRegularExpression re(
        R"(^\s*running\s+(?:scx_)?(\w+)(?:\s+in\s+(\w+)\s+mode)?)",
        QRegularExpression::CaseInsensitiveOption);
    const auto m = re.match(output);
    if (!m.hasMatch())
        return {};
    const QString mode = m.captured(2).toLower();
    return {true, m.captured(1).toLower(), mode.isEmpty() ? QStringLiteral("auto") : mode};
}

QStringList parseList(const QString &output) {
    const qsizetype start = output.indexOf('[');
    if (start < 0)
        return {};
    const auto doc = QJsonDocument::fromJson(output.mid(start).toUtf8());
    QStringList names;
    for (const auto &v : doc.array()) {
        QString name = v.toString().toLower();
        if (name.startsWith("scx_"))
            name.remove(0, 4);
        if (!name.isEmpty())
            names << name;
    }
    return names;
}

QStringList parseModes(const QString &output) {
    static const QRegularExpression re(R"(\[([^\]]*)\])");
    QStringList modes;
    for (const QString &m : re.match(output).captured(1).split(',', Qt::SkipEmptyParts))
        modes << m.trimmed().toLower();
    return modes;
}

QStringList installedPrograms() {
    QStringList paths;
    QSet<QString> seen;
    for (const QString &dir : qEnvironmentVariable("PATH").split(':', Qt::SkipEmptyParts)) {
        const auto entries = QDir(dir).entryInfoList({"scx*"}, QDir::Files | QDir::Executable,
                                                     QDir::Name);
        for (const QFileInfo &fi : entries) {
            if (fi.fileName() == "scx-switcher" || seen.contains(fi.fileName()))
                continue;
            seen.insert(fi.fileName());
            paths << fi.canonicalFilePath(); // dpkg knows /usr/bin, not /bin
        }
    }
    return paths;
}

Kernel kernelState() {
    QFile f("/sys/kernel/sched_ext/state");
    if (!f.open(QIODevice::ReadOnly))
        return Kernel::Unsupported;
    const QByteArray state = f.readAll().trimmed();
    if (state == "enabled")
        return Kernel::Enabled;
    if (state == "disabled")
        return Kernel::Disabled;
    return Kernel::Changing;
}

BootDefault bootDefault() {
    BootDefault out;
    QFile f("/etc/scx_loader/config.toml");
    if (!f.open(QIODevice::ReadOnly))
        return out;
    const QString text = QString::fromUtf8(f.readAll());
    out.emptyConfig = text.isEmpty();

    static const QRegularExpression sched(R"(^\s*default_sched\s*=\s*"(?:scx_)?(\w+)\")",
                                          QRegularExpression::MultilineOption);
    static const QRegularExpression mode(R"(^\s*default_mode\s*=\s*"(\w+)\")",
                                         QRegularExpression::MultilineOption);
    out.sched = sched.match(text).captured(1).toLower();
    out.mode = mode.match(text).captured(1).toLower();
    if (out.mode.isEmpty())
        out.mode = "auto";
    return out;
}

bool scxctlAvailable() { return !QStandardPaths::findExecutable("scxctl").isEmpty(); }

bool pkexecAvailable() { return !QStandardPaths::findExecutable("pkexec").isEmpty(); }

void run(QObject *context, const QString &program, const QStringList &args, Done done,
         int timeoutMs) {
    auto *proc = new QProcess(context);
    proc->setProcessChannelMode(QProcess::MergedChannels);

    // finished and errorOccurred can both fire; deliver only the first.
    auto pending = std::make_shared<Done>(std::move(done));
    auto finish = [proc, pending](int code, const QString &output) {
        if (!*pending)
            return;
        const Done cb = std::exchange(*pending, nullptr);
        proc->deleteLater();
        cb(code, output);
    };

    QObject::connect(proc, &QProcess::finished, proc,
                     [proc, finish](int code, QProcess::ExitStatus status) {
                         const QString out = QString::fromUtf8(proc->readAll()).trimmed();
                         if (proc->property("timedOut").toBool())
                             finish(-1, QStringLiteral("timed out"));
                         else
                             finish(status == QProcess::NormalExit ? code : -1, out);
                     });
    QObject::connect(proc, &QProcess::errorOccurred, proc,
                     [proc, finish](QProcess::ProcessError error) {
                         if (error == QProcess::FailedToStart)
                             finish(-1, proc->errorString());
                     });
    QTimer::singleShot(timeoutMs, proc, [proc] {
        proc->setProperty("timedOut", true);
        proc->kill();
    });

    proc->start(program, args);
}

} // namespace scx
