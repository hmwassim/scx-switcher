#pragma once

#include "scx.h"
#include "ui.h"

#include <QHash>
#include <QMainWindow>

class InstalledView;
class QButtonGroup;
class QFrame;
class QHBoxLayout;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QStackedWidget;
class QSystemTrayIcon;
class QTabWidget;
class QTextEdit;
class QTimer;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    QWidget *buildStatusCard();
    QWidget *buildSchedulerTab();
    QWidget *buildProblemPage();
    void setupTray();

    void startup();
    void showProblem(const QString &title, const QString &text, const QString &command,
                     bool offerRepair = false);

    void loadSchedulers();
    void refreshStatus();
    void setStatus(bool reachable, const scx::Status &loader, const QString &error);
    void refreshBoot();
    void markRunning();

    QString selectedScheduler() const;
    QString selectedMode() const;
    void onSchedulerChanged();
    void showModes(const QStringList &modes);
    void updateControls();

    void apply();
    void stop();
    void repair();
    void runHelper(const QStringList &args, const QString &progress, const QString &success,
                   const std::function<void(bool ok)> &after = {});
    void setMessage(const QString &text, bool error = false);
    void log(const QString &msg, ui::Tone tone = ui::Tone::Blue);

    QStackedWidget *m_pages = nullptr;
    QTabWidget *m_tabs = nullptr;
    QWidget *m_problemPage = nullptr;

    // Status card
    QFrame *m_card = nullptr;
    QLabel *m_dot = nullptr;
    QLabel *m_statusText = nullptr;
    QLabel *m_modeBadge = nullptr;
    QLabel *m_bootText = nullptr;
    QPushButton *m_stopBtn = nullptr;

    // Schedulers tab
    QListWidget *m_schedList = nullptr;
    QLabel *m_title = nullptr;
    QLabel *m_tagBadge = nullptr;
    QLabel *m_maturityBadge = nullptr;
    QLabel *m_summary = nullptr;
    QLabel *m_bestFor = nullptr;
    QHBoxLayout *m_modeRow = nullptr;
    QButtonGroup *m_modeGroup = nullptr;
    QPushButton *m_applyBtn = nullptr;
    QLabel *m_message = nullptr;

    InstalledView *m_installed = nullptr;
    QTextEdit *m_log = nullptr;

    // Problem page
    QLabel *m_problemTitle = nullptr;
    QLabel *m_problemText = nullptr;
    QLineEdit *m_problemCmd = nullptr;
    QPushButton *m_repairBtn = nullptr;

    QSystemTrayIcon *m_tray = nullptr;
    QTimer *m_poll = nullptr;

    QHash<QString, QStringList> m_modes; // scheduler -> modes from `scxctl modes`
    QString m_wantedMode;                // mode to preselect once modes load
    scx::Status m_loader;
    bool m_active = false;   // the kernel has a sched_ext scheduler attached
    bool m_busy = false;     // a privileged operation is running
    bool m_querying = false; // `scxctl get` is running
    bool m_synced = false;   // selection initialised from the running scheduler
};
