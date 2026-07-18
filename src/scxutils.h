#pragma once

#include <QObject>
#include <QPair>
#include <QQueue>
#include <QString>
#include <QStringList>

class ProcessRunner;

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

    static void saveState(const QString &sched, const QString &mode);
    static QPair<QString, QString> loadState();

    signals:
    void kernelChecked(bool supported, const QString &detail);
    void statusReady(const SchedStatus &status);
    void schedulersListed(const QStringList &bareNames);

    private:
    explicit ScxUtils(QObject *parent = nullptr);

    enum class Op { None, Kernel, Status, List };

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

    void dispatchError(Op op, const QString &msg);

    Op m_current = Op::None;
    QQueue<Job> m_queue;
    ProcessRunner *m_runner = nullptr;
};
