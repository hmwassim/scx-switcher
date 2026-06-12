#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <functional>

class QProcess;

class PrivOps : public QObject {
    Q_OBJECT
public:
    using Callback = std::function<void(bool success, const QString &message)>;

    static PrivOps *instance();
    static bool isPolkitAgentRunning();

    void startScheduler(const QString &sched, const QString &mode, Callback cb = {});
    void stopScheduler(Callback cb = {});
    void switchScheduler(const QString &sched, const QString &mode, Callback cb = {});
    void enableService(Callback cb = {});
    void disableService(Callback cb = {});
    void writeConfigToml(const QString &sched, const QString &mode, Callback cb = {});

private:
    explicit PrivOps(QObject *parent = nullptr);
    ~PrivOps();
    void runPrivileged(const QStringList &args, Callback cb);

    QProcess *m_proc = nullptr;
    QString m_lastTmpFile;
    Callback m_callback;
};
