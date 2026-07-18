#pragma once

#include <QObject>
#include <QString>

class SchedulerController : public QObject {
    Q_OBJECT
    public:
    explicit SchedulerController(QObject *parent = nullptr);

    void start(const QString &sched, const QString &mode);
    void stop();
    void setPersist(bool enabled);

    signals:
    void log(const QString &msg);
    void operationInProgress(bool inFlight);
    void statusChanged();
    void persistToggled(bool enabled);

    private:
    void setEnabled(bool enabled);
};
