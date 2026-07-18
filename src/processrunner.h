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
    void runWithContent(const QString &program, const QStringList &args, const QString &content,
                        Callback cb = {});
    void cancel();

    bool isRunning() const;

    private:
    struct PendingOp {
        QString program;
        QStringList args;
        QString configContent;
        Callback callback;
    };

    void startPending();
    void drainPending();

    int m_timeoutMs;
    QProcess *m_proc = nullptr;
    QTimer *m_timeout = nullptr;
    QString m_configContent;
    Callback m_callback;
    std::optional<PendingOp> m_pendingOp;
};
