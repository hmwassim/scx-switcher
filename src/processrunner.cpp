#include "processrunner.h"

#include <QProcess>

ProcessRunner::ProcessRunner(int timeoutMs, QObject *parent) : QObject(parent), m_timeoutMs(timeoutMs) {
    m_timeout = new QTimer(this);
    m_timeout->setSingleShot(true);

    m_proc = new QProcess(this);
    m_proc->setProcessChannelMode(QProcess::SeparateChannels);

    connect(m_proc, &QProcess::started, this, [this]() {
        if (!m_configContent.isEmpty()) {
            m_proc->write(m_configContent.toUtf8());
            m_proc->closeWriteChannel();
            m_configContent.clear();
        }
    });

    connect(m_proc, &QProcess::finished, this, [this](int exit, QProcess::ExitStatus) {
        m_timeout->stop();
        m_configContent.clear();

        const QString out = QString::fromUtf8(m_proc->readAllStandardOutput()).trimmed();

        if (m_callback) {
            auto cb = std::move(m_callback);
            m_callback = nullptr;
            cb(exit, out);
        }

        drainPending();
    });

    connect(m_proc, &QProcess::errorOccurred, this, [this](QProcess::ProcessError err) {
        if (err != QProcess::FailedToStart)
            return;

        m_timeout->stop();
        m_configContent.clear();

        const QString msg =
            QString("Failed to start process: %1").arg(m_proc->errorString());

        if (m_callback) {
            auto cb = std::move(m_callback);
            m_callback = nullptr;
            cb(-1, msg);
        }

        if (m_pendingOp) {
            auto op = std::move(*m_pendingOp);
            m_pendingOp.reset();
            if (op.callback)
                op.callback(-1, msg);
        }
    });

    connect(m_timeout, &QTimer::timeout, this, [this]() {
        if (m_proc->state() == QProcess::NotRunning)
            return;
        m_configContent.clear();
        if (m_callback) {
            auto cb = std::move(m_callback);
            m_callback = nullptr;
            cb(-1, "Operation timed out");
        }
        m_proc->kill();
    });
}

ProcessRunner::~ProcessRunner() {
    if (m_proc->state() != QProcess::NotRunning) {
        m_proc->kill();
        m_proc->waitForFinished(5000);
    }
}

void ProcessRunner::startPending() {
    if (!m_pendingOp || m_proc->state() != QProcess::NotRunning)
        return;

    auto op = std::move(*m_pendingOp);
    m_pendingOp.reset();
    m_callback = std::move(op.callback);

    if (!op.configContent.isEmpty())
        m_configContent = op.configContent;
    m_timeout->start(m_timeoutMs);
    m_proc->start(op.program, op.args);
}

void ProcessRunner::drainPending() { startPending(); }

bool ProcessRunner::isRunning() const { return m_proc->state() != QProcess::NotRunning; }

void ProcessRunner::run(const QString &program, const QStringList &args, Callback cb) {
    if (m_proc->state() != QProcess::NotRunning) {
        cancel();
        m_pendingOp = PendingOp{program, args, {}, std::move(cb)};
        return;
    }
    m_callback = std::move(cb);
    m_timeout->start(m_timeoutMs);
    m_proc->start(program, args);
}

void ProcessRunner::runWithContent(const QString &program, const QStringList &args,
                                   const QString &content, Callback cb) {
    if (m_proc->state() != QProcess::NotRunning) {
        cancel();
        m_pendingOp = PendingOp{program, args, content, std::move(cb)};
        return;
    }
    m_configContent = content;
    m_callback = std::move(cb);
    m_timeout->start(m_timeoutMs);
    m_proc->start(program, args);
}

void ProcessRunner::cancel() {
    if (m_proc->state() == QProcess::NotRunning)
        return;
    m_timeout->stop();
    m_configContent.clear();
    if (m_callback) {
        auto cb = std::move(m_callback);
        m_callback = nullptr;
        cb(-1, "Superseded by new operation");
    }
    m_proc->kill();
}
