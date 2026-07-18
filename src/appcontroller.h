#pragma once

#include <QObject>
#include <QString>
#include <QTimer>

struct SchedInfo;

class AppController : public QObject {
    Q_OBJECT
    public:
    static AppController *get();

    void startPolling();
    void stopPolling();
    void refreshStatus();

    SchedInfo schedulerInfo(const QString &bare) const;
    QString humanize(const QString &bare) const;
    QString humanizeMode(const QString &mode) const;

    signals:
    void statusChanged(bool active, const QString &name, const QString &mode);
    void logMessage(const QString &msg);

    private:
    explicit AppController(QObject *parent = nullptr);

    QTimer *m_pollTimer = nullptr;
    static constexpr int POLL_INTERVAL_MS = 3000;
};
