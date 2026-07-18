#pragma once

#include <QCloseEvent>
#include <QLabel>
#include <QMainWindow>
#include <QPushButton>
#include <QStackedWidget>
#include <QTextEdit>

class AppController;
class ControlTab;
class QToolButton;

class MainWindow : public QMainWindow {
    Q_OBJECT
    public:
    explicit MainWindow(QWidget *parent = nullptr);

    static constexpr const char *APP_NAME = "SCX Switcher";
    static constexpr const char *APP_VERSION = APP_VERSION_STR;

    protected:
    void closeEvent(QCloseEvent *event) override;

    private slots:
    void onStopClicked();

    private:
    void buildShell();
    void onKernelResult(bool supported, const QString &detail);
    void buildNormalMode();
    void buildUnsupportedPage();
    void buildSetupPage();

    void updateStatusBar(bool active, const QString &name, const QString &mode);
    void updateSchedInfo(const QString &bare);
    void toggleLog();

    AppController *m_app = nullptr;

    // Status card
    QLabel *m_dot = nullptr;
    QLabel *m_statusText = nullptr;
    QPushButton *m_stopBtn = nullptr;

    // Content stack
    QStackedWidget *m_contentStack = nullptr;
    QWidget *m_normalPage = nullptr;
    ControlTab *m_ctrlTab = nullptr;

    // Scheduler info section
    QLabel *m_infoTitle = nullptr;
    QLabel *m_infoCat = nullptr;
    QLabel *m_infoDesc = nullptr;
    QLabel *m_infoModes = nullptr;

    // Collapsible log
    QToolButton *m_logToggle = nullptr;
    QTextEdit *m_log = nullptr;

    bool m_kernelOk = false;
    bool m_schedActive = false;
    bool m_opInFlight = false;
    bool m_logVisible = true;
};
