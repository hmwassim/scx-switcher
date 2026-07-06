#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <functional>

class QProcess;

class PrivOps : public QObject {
    Q_OBJECT
public:
    using Callback = std::function<void(bool ok, const QString &msg)>;

    static PrivOps *get();
    static bool     pkexecPresent();

    void startScheduler (const QString &sched, const QString &mode, Callback cb = {});
    void switchScheduler(const QString &sched, const QString &mode, Callback cb = {});
    void stopScheduler  (Callback cb = {});
    void enableService  (Callback cb = {});
    void disableService (Callback cb = {});
    void writeConfig    (const QString &sched, const QString &mode, Callback cb = {});

private:
    explicit PrivOps(QObject *parent = nullptr);
    ~PrivOps();

    void run(const QStringList &args, Callback cb);
    void cancelInFlight();

    QProcess *m_proc          = nullptr;
    QTimer   *m_timeout       = nullptr;
    QString   m_configContent;
    Callback  m_callback;
};
