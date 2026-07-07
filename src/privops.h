#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <functional>
#include <optional>

class QProcess;

class PrivOps : public QObject {
    Q_OBJECT
    public:
    using Callback = std::function<void(bool ok, const QString &msg)>;

    static PrivOps *get();
    static bool pkexecPresent();
    static QString checkPolicyPath();

    void startScheduler(const QString &sched, const QString &mode, Callback cb = {});
    void switchScheduler(const QString &sched, const QString &mode, Callback cb = {});
    void stopScheduler(Callback cb = {});
    void enableService(Callback cb = {});
    void disableService(Callback cb = {});
    void writeConfig(const QString &sched, const QString &mode, Callback cb = {});

    private:
    explicit PrivOps(QObject *parent = nullptr);
    ~PrivOps();

    struct PendingOp {
        QStringList args;
        QString configContent;
        Callback callback;
    };

    void run(const QStringList &args, Callback cb);
    void cancelInFlight();

    QProcess *m_proc = nullptr;
    QTimer *m_timeout = nullptr;
    QString m_configContent;
    Callback m_callback;
    std::optional<PendingOp> m_pendingOp;
};
