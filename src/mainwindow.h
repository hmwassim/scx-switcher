#pragma once

#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QTextEdit>
#include <QTabWidget>
#include <QTimer>
#include <QSystemTrayIcon>
#include <QCloseEvent>
#include <QMenu>

#include "config.h"

class ControlTab;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

    static constexpr const char *APP_NAME    = "SCX Switcher";
    static constexpr const char *APP_VERSION = APP_VERSION_STR;

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void refreshStatus();
    void onStopClicked();

private:
    void buildShell();
    void onKernelResult(bool supported, const QString &detail);
    void buildNormalMode();
    void buildUnsupportedPage();
    void buildSetupPage();
    void buildReferenceTab(const QStringList &installed);

    void appendLog(const QString &msg);
    void updateStatusBar(bool active, const QString &name, const QString &mode);
    void setTray(bool active, const QString &schedName = {});

    static QIcon trayIcon(const QColor &color);

    QLabel      *m_dot        = nullptr;
    QLabel      *m_statusText = nullptr;
    QPushButton *m_stopBtn    = nullptr;

    QTabWidget  *m_tabs       = nullptr;
    ControlTab  *m_ctrlTab    = nullptr;
    QTextEdit   *m_log        = nullptr;

    QSystemTrayIcon *m_tray     = nullptr;
    QMenu           *m_trayMenu = nullptr;

    QTimer *m_pollTimer                  = nullptr;
    QMetaObject::Connection m_statusConn;

    bool m_kernelOk                      = false;
    bool m_schedActive                   = false;
    bool m_opInFlight                    = false;
};
