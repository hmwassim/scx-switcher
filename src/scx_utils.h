#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QPair>
#include <QQueue>
#include <QProcess>

struct SchedStatus {
    bool active = false;
    QString name;
    QString mode;
};

class ScxUtils : public QObject {
    Q_OBJECT
public:
    explicit ScxUtils(QObject *parent = nullptr);

    static bool isScxctlInstalled();
    static bool isToolInstalled(const QString &name);
    static QStringList supportedModes(const QString &schedBareName);
    static void saveState(const QString &sched, const QString &mode);
    static QPair<QString, QString> loadState();
    static QString humanizeScheduler(const QString &name);
    static QString humanizeMode(const QString &mode);

    void checkKernelSupport();
    void getSchedulerStatus();
    void listSchedulers();
    void checkServiceEnabled();

signals:
    void kernelSupportChecked(bool supported, const QString &message);
    void schedulerStatusReady(const SchedStatus &status);
    void schedulersListed(const QStringList &schedulers);
    void serviceEnabledChecked(bool enabled);

private:
    enum Op { None, KernelCheck, Status, List, ServiceCheck };

    static const QString SCXCTL;
    static const QString SYSTEMCTL;
    static const QString SERVICE;

    struct PendingOp {
        Op op;
        QString program;
        QStringList args;
    };
    Op m_currentOp = None;
    QQueue<PendingOp> m_queue;
    QProcess *m_proc = nullptr;

    void enqueue(Op op, const QString &program, const QStringList &args);
    void processNext();

    void handleKernelCheck(const QString &out);
    void handleStatus(int exitCode, const QString &out);
    void handleList(int exitCode, const QString &out);
    void handleServiceCheck(int exitCode, const QString &out);
};

namespace scx_utils {
inline ScxUtils *instance() {
    static ScxUtils *inst = new ScxUtils;
    return inst;
}
}
