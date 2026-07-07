#pragma once

#include <QObject>
#include <QPair>
#include <QQueue>
#include <QString>
#include <QStringList>
#include <QTimer>

class QProcess;

struct SchedStatus {
    bool active = false;
    QString name;
    QString mode;
};

class ScxUtils : public QObject {
    Q_OBJECT
    public:
    static ScxUtils *get();

    static bool scxctlPresent();

    void checkKernel();
    void queryStatus();
    void listSchedulers();
    void checkServiceEnabled();

    static void saveState(const QString &sched, const QString &mode);
    static QPair<QString, QString> loadState();

    signals:
    void kernelChecked(bool supported, const QString &detail);
    void statusReady(const SchedStatus &status);
    void schedulersListed(const QStringList &bareNames);
    void serviceEnabled(bool enabled);

    private:
    explicit ScxUtils(QObject *parent = nullptr);

    enum class Op { None, Kernel, Status, List, Service };

    struct Job {
        Op op;
        QString program;
        QStringList args;
    };

    void enqueue(Op op, const QString &prog, const QStringList &args);
    void runNext();

    void onKernel(const QString &out);
    void onStatus(int exit, const QString &out);
    void onList(int exit, const QString &out);
    void onService(int exit, const QString &out);

    void dispatchError(Op op, const QString &msg);

    Op m_current = Op::None;
    QQueue<Job> m_queue;
    QProcess *m_proc = nullptr;
    QTimer *m_timeout = nullptr;
    QString m_pendingError; // set by timeout handler, consumed by finished
};
