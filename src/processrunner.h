#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <functional>
#include <optional>

class QProcess;

class ProcessRunner : public QObject {
    Q_OBJECT
    public:
    using Callback = std::function<void(int exitCode, const QString &output)>;

    explicit ProcessRunner(int timeoutMs = 30000, QObject *parent = nullptr);
    ~ProcessRunner();

    void run(const QString &program, const QStringList &args, Callback cb = {});

    private:
    struct PendingOp {
        QString program;
        QStringList args;
        Callback callback;
    };

    void startPending();
    void drainPending();
    void cancel();

    int m_timeoutMs;
    QProcess *m_proc = nullptr;
    QTimer *m_timeout = nullptr;
    Callback m_callback;
    std::optional<PendingOp> m_pendingOp;
};
