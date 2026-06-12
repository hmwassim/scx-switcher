#pragma once

#include <QMainWindow>
#include <QScrollBar>
#include <QLabel>
#include <QPushButton>
#include <QTextEdit>
#include <QTabWidget>
#include <QTimer>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QCloseEvent>

class ControlTab;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

    static const QString APP_NAME;
    static const QString APP_VERSION;

private slots:
    void refreshStatus();
    void onStopClicked();
    void updateMarquee();

private:
    void buildUi();
    void buildSetupMode();
    void buildNormalMode();
    void buildReferenceTab();
    QWidget *buildUnsupportedPage();
    void checkKernelAndBuildUi();
    void log(const QString &msg);
    void toggleTrayIcon(bool running, const QString &name = {});

    void closeEvent(QCloseEvent *event) override;

    QLabel *m_statusDot = nullptr;
    QLabel *m_statusLabel = nullptr;
    QPushButton *m_stopBtn = nullptr;

    QTabWidget *m_tabs = nullptr;
    ControlTab *m_controlTab = nullptr;

    QTextEdit *m_logView = nullptr;

    QSystemTrayIcon *m_tray = nullptr;

    QTimer *m_statusTimer = nullptr;

    QTimer *m_marqueeTimer = nullptr;
    int m_marqueeOffset = 0;
    QString m_marqueeText;

    bool m_kernelChecked = false;
    bool m_refreshing = false;
};
