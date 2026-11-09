#pragma once

#include "catalog.h"

#include <QWidget>

class Activity;
class Monitor;
class QCheckBox;
class QComboBox;
class QFormLayout;
class QFrame;
class QGridLayout;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QStackedWidget;
class QToolButton;
class QVBoxLayout;

class Window : public QWidget {
   Q_OBJECT
public:
   Window(Monitor &monitor, Activity &activity, QWidget *parent = nullptr);

protected:
   bool event(QEvent *event) override;
   void resizeEvent(QResizeEvent *event) override;
   void showEvent(QShowEvent *event) override;
   void hideEvent(QHideEvent *event) override;
   void changeEvent(QEvent *event) override;

private:
   enum class Busy { None, Starting, Stopping, Other };
   enum class Kind { Info, Warning, Problem };

   QWidget *buildCard();
   QWidget *buildSwitcher();
   QWidget *buildProblem();
   void buildLog(QVBoxLayout *root);

   void fillSchedulers();
   void fillModes();
   void syncFromSystem();
   void updateStatus();
   void updateInfo();
   void updateControls();
   void fitHeight();
   void showPage(QWidget *page);
   void showProblem(const QString &title, const QString &text, const QString &command);
   void setMessage(Kind kind, const QString &text, const QString &action = {});
   void openLog(bool open);

   QString selected() const;
   QString selectedMode() const;
   QStringList selectedOptions() const;
   bool selectionRuns() const;

   void apply();
   void stop();
   void restart();
   void stopLoader();
   void startScheduler(const QString &sched, const QStringList &options, bool boot);
   void runHelper(const QStringList &args, Busy busy, const QString &target);

   Monitor &m_monitor;
   Activity &m_activity;
   QList<catalog::Scheduler> m_schedulers;
   Busy m_busy = Busy::None;
   QString m_target;   // scheduler being started or stopped
   QString m_previous; // scheduler that ran before Apply
   bool m_synced = false;
   bool m_actionMessage = false;
   int m_heightBeforeLog = 0;

   QVBoxLayout *m_outer = nullptr;
   QVBoxLayout *m_root = nullptr;
   QGridLayout *m_cardGrid = nullptr;
   QLabel *m_dot = nullptr;
   QLabel *m_status = nullptr;
   QLabel *m_boot = nullptr;
   QStackedWidget *m_stopSlot = nullptr;
   QPushButton *m_stop = nullptr;
   QPushButton *m_restart = nullptr;

   QWidget *m_switcher = nullptr;
   QFormLayout *m_form = nullptr;
   QComboBox *m_sched = nullptr;
   QComboBox *m_mode = nullptr;
   QLabel *m_optionsLabel = nullptr;
   QLineEdit *m_options = nullptr;
   QFrame *m_info = nullptr;
   QLabel *m_name = nullptr;
   QLabel *m_focus = nullptr;
   QLabel *m_group = nullptr;
   QLabel *m_summary = nullptr;
   QLabel *m_best = nullptr;
   QLabel *m_runs = nullptr;
   QCheckBox *m_bootBox = nullptr;
   QLabel *m_messageIcon = nullptr;
   QLabel *m_message = nullptr;
   QPushButton *m_messageAction = nullptr;
   QStackedWidget *m_applySlot = nullptr;
   QPushButton *m_apply = nullptr;

   QWidget *m_problem = nullptr;
   QLabel *m_problemTitle = nullptr;
   QLabel *m_problemText = nullptr;
   QLineEdit *m_problemCommand = nullptr;

   QToolButton *m_logToggle = nullptr;
   QPlainTextEdit *m_log = nullptr;
};
