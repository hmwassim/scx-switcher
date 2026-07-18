#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <functional>

class ProcessRunner;

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

    void run(const QStringList &args, Callback cb);

    ProcessRunner *m_runner = nullptr;
};
