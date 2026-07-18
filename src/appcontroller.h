#pragma once

#include <QObject>
#include <QString>
#include <QTimer>

class QSystemTrayIcon;
class QMenu;

struct SchedInfo;

class AppController : public QObject {
    Q_OBJECT
    public:
    static AppController *get();

    void start();
    void stop();
    void refreshStatus();

    SchedInfo schedulerInfo(const QString &bare) const;
    QString humanize(const QString &bare) const;
    QString humanizeMode(const QString &mode) const;

    void appendLog(const QString &msg);

    signals:
    void statusChanged(bool active, const QString &name, const QString &mode);
    void logMessage(const QString &msg);
    void showRequested();

    private:
    explicit AppController(QObject *parent = nullptr);

    void initTray();
    void updateTray(bool active, const QString &schedName = {});

    QTimer *m_pollTimer = nullptr;
    QSystemTrayIcon *m_tray = nullptr;
    QMenu *m_trayMenu = nullptr;

    static constexpr int POLL_INTERVAL_MS = 3000;
};
