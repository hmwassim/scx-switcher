#pragma once

#include <QObject>
#include <QString>

class SchedulerController : public QObject {
    Q_OBJECT
    public:
    explicit SchedulerController(QObject *parent = nullptr);

    void start(const QString &sched, const QString &mode);
    void stop();

    signals:
    void log(const QString &msg);
    void operationInProgress(bool inFlight);
    void statusChanged();

    private:
    void setEnabled(bool enabled);
    void autoEnable();
};
