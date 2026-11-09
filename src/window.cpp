#include "window.h"

#include "activity.h"
#include "monitor.h"
#include "spinner.h"
#include "style.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QFontDatabase>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QScrollBar>
#include <QStackedWidget>
#include <QStandardItemModel>
#include <QStyle>
#include <QSysInfo>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

using style::Tone;

/* The window opens at its smallest size and can't get narrower */
static constexpr int MIN_WIDTH_PX = 528;
static constexpr int LOG_PX = 110;
static constexpr int DOT_PX = 18;
static constexpr int BUTTON_MIN_PX = 84;
static const QString OPTIONS_HINT = "Options can only use letters, digits and - _ = . , : / +";

static QLabel *mutedLabel(const QString &text = {}) {
   auto *label = new QLabel(text);
   label->setForegroundRole(QPalette::PlaceholderText);
   label->setWordWrap(true);
   return label;
}

static QLabel *caption(const QString &text) {
   auto *label = mutedLabel(text.toUpper());
   QFont font = label->font();
   font.setPointSizeF(font.pointSizeF() * 0.8);
   font.setBold(true);
   label->setFont(font);
   return label;
}

/* Breeze draws no plain panel frame, so a thin border mixed from the theme's own colors */
static void setBorder(QFrame *frame) {
   const QPalette palette = QApplication::palette();
   const QColor window = palette.color(QPalette::Window);
   const QColor text = palette.color(QPalette::WindowText);
   const QColor border = QColor::fromRgbF(window.redF() * 0.78 + text.redF() * 0.22,
                                          window.greenF() * 0.78 + text.greenF() * 0.22,
                                          window.blueF() * 0.78 + text.blueF() * 0.22);
   frame->setStyleSheet(QString("#%1 { border: 1px solid %2; border-radius: 6px; }")
                           .arg(frame->objectName(), border.name()));
}

static QFrame *panel(const QString &name) {
   auto *frame = new QFrame;
   frame->setObjectName(name);
   setBorder(frame);
   return frame;
}

/* A button and its spinner share one slot, so nothing moves while busy */
static QStackedWidget *withSpinner(QPushButton *button) {
   auto *slot = new QStackedWidget;
   slot->addWidget(button);
   slot->addWidget(new Spinner);
   slot->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
   return slot;
}

Window::Window(Monitor &monitor, Activity &activity, QWidget *parent)
   : QWidget(parent), m_monitor(monitor), m_activity(activity) {
   setWindowTitle("SCX Switcher");

   setMinimumWidth(MIN_WIDTH_PX);
   /* No maximize button where the window system allows it (X11; Wayland has no way to ask) */
   setWindowFlag(Qt::WindowMaximizeButtonHint, false);

   /* Content anchored to the top, the log to the bottom, free space in between */
   m_outer = new QVBoxLayout(this);
   m_outer->setContentsMargins(12, 10, 12, 10);
   m_outer->setSpacing(10);
   /* The content is its own widget, so its height for a given width is always current */
   auto *content = new QWidget;
   m_root = new QVBoxLayout(content);
   m_root->setContentsMargins(0, 0, 0, 0);
   m_root->setSpacing(10);
   m_outer->addWidget(content);
   m_outer->addStretch(1);
   m_root->addWidget(buildCard());

   /* Both pages sit in the layout and only one is shown; a hidden page takes no room */
   m_root->addWidget(buildSwitcher());
   m_root->addWidget(m_info);
   m_root->addWidget(buildProblem());
   m_problem->hide();
   buildLog(m_outer);

   m_schedulers = catalog::installed();
   fillSchedulers();

   connect(&m_monitor, &Monitor::changed, this, [this] {
      syncFromSystem();
      updateStatus();
   });
   connect(&m_activity, &Activity::added, this, [this](const QString &line) {
      m_log->appendPlainText(line);
      m_log->verticalScrollBar()->setValue(m_log->verticalScrollBar()->maximum());
   });

   syncFromSystem();
   updateStatus();
   /* Opening at exactly the content's height for the starting width */
   m_outer->activate();
   const int height = m_outer->hasHeightForWidth() ? m_outer->totalHeightForWidth(MIN_WIDTH_PX)
                                                   : m_outer->totalSizeHint().height();
   resize(MIN_WIDTH_PX, height);
}

QWidget *Window::buildCard() {
   auto *card = panel("card");
   m_cardGrid = new QGridLayout(card);
   m_cardGrid->setContentsMargins(12, 10, 12, 10);
   m_cardGrid->setHorizontalSpacing(12);
   m_cardGrid->setVerticalSpacing(1);

   m_dot = new QLabel;
   m_dot->setFixedSize(DOT_PX, DOT_PX);
   m_cardGrid->addWidget(m_dot, 0, 0, 3, 1, Qt::AlignVCenter);
   m_cardGrid->addWidget(caption("Status"), 0, 1);

   m_status = new QLabel;
   QFont font = m_status->font();
   font.setPointSizeF(font.pointSizeF() * 1.25);
   m_status->setFont(font);
   m_status->setWordWrap(true);
   m_cardGrid->addWidget(m_status, 1, 1);
   m_boot = mutedLabel();
   m_cardGrid->addWidget(m_boot, 2, 1);
   m_cardGrid->setColumnStretch(1, 1);

   auto *buttonBox = new QWidget;
   auto *buttons = new QHBoxLayout(buttonBox);
   buttons->setContentsMargins(0, 0, 0, 0);
   m_stop = new QPushButton("Stop");
   m_stop->setMinimumWidth(BUTTON_MIN_PX);
   m_stop->setToolTip("Stop the scheduler now. Boot start stays as it is.");
   connect(m_stop, &QPushButton::clicked, this, &Window::stop);
   m_stopSlot = withSpinner(m_stop);
   buttons->addWidget(m_stopSlot);
   m_restart = new QPushButton("Restart");
   m_restart->setMinimumWidth(BUTTON_MIN_PX);
   connect(m_restart, &QPushButton::clicked, this, &Window::restart);
   buttons->addWidget(m_restart);
   auto *quit = new QPushButton("Quit");
   quit->setMinimumWidth(BUTTON_MIN_PX);
   connect(quit, &QPushButton::clicked, qApp, &QApplication::quit);
   buttons->addWidget(quit);
   m_cardGrid->addWidget(buttonBox, 0, 2, 3, 1, Qt::AlignVCenter);
   return card;
}

QWidget *Window::buildSwitcher() {
   auto *box = new QGroupBox("Switch Scheduler");
   auto *col = new QVBoxLayout(box);
   col->setSpacing(10);

   /* One row per element at every width, like a table */
   m_form = new QFormLayout;
   m_form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
   m_sched = new QComboBox;
   m_mode = new QComboBox;
   m_options = new QLineEdit;
   m_options->setPlaceholderText("--slice-us 2000");
   m_options->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
   m_optionsLabel = new QLabel("Options:");
   m_form->addRow("Scheduler:", m_sched);
   m_form->addRow("Mode:", m_mode);
   m_form->addRow(m_optionsLabel, m_options);
   connect(m_sched, &QComboBox::currentIndexChanged, this, [this] {
      fillModes();
      updateInfo();
   });
   connect(m_mode, &QComboBox::currentIndexChanged, this, &Window::updateInfo);
   connect(m_options, &QLineEdit::textChanged, this, &Window::updateInfo);
   col->addLayout(m_form);

   auto *info = panel("info");
   auto *infoCol = new QVBoxLayout(info);
   infoCol->setContentsMargins(10, 8, 10, 8);
   infoCol->setSpacing(5);
   auto *top = new QHBoxLayout;
   top->setSpacing(6);
   m_name = new QLabel;
   QFont bold = m_name->font();
   bold.setPointSizeF(bold.pointSizeF() * 1.15);
   bold.setBold(true);
   m_name->setFont(bold);
   top->addWidget(m_name);
   top->addStretch();
   m_focus = new QLabel;
   m_group = new QLabel;
   top->addWidget(m_focus);
   top->addWidget(m_group);
   infoCol->addLayout(top);
   m_summary = new QLabel;
   m_summary->setWordWrap(true);
   infoCol->addWidget(m_summary);
   m_best = mutedLabel();
   infoCol->addWidget(m_best);
   m_runs = mutedLabel();
   m_runs->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
   m_runs->setTextInteractionFlags(Qt::TextSelectableByMouse);
   infoCol->addWidget(m_runs);
   m_info = info;

   m_bootBox = new QCheckBox("Start at boot");
   m_bootBox->setToolTip("Also start this scheduler every time the computer starts");
   connect(m_bootBox, &QCheckBox::toggled, this, &Window::updateControls);
   col->addWidget(m_bootBox);

   auto *row = new QHBoxLayout;
   row->setSpacing(8);
   m_messageIcon = new QLabel;
   row->addWidget(m_messageIcon, 0, Qt::AlignTop);
   m_message = new QLabel;
   m_message->setWordWrap(true);
   m_message->setTextInteractionFlags(Qt::TextSelectableByMouse);
   row->addWidget(m_message, 1);
   m_messageAction = new QPushButton;
   m_messageAction->hide();
   connect(m_messageAction, &QPushButton::clicked, this, &Window::stopLoader);
   row->addWidget(m_messageAction, 0, Qt::AlignVCenter);
   m_apply = new QPushButton("Apply");
   m_apply->setDefault(true);
   m_apply->setMinimumWidth(BUTTON_MIN_PX);
   connect(m_apply, &QPushButton::clicked, this, &Window::apply);
   m_applySlot = withSpinner(m_apply);
   row->addWidget(m_applySlot, 0, Qt::AlignBottom);
   col->addLayout(row);

   m_switcher = box;
   setMessage(Kind::Info, {});
   return box;
}

QWidget *Window::buildProblem() {
   m_problem = panel("problem");
   auto *col = new QVBoxLayout(m_problem);
   col->setContentsMargins(18, 24, 18, 24);
   col->setSpacing(10);
   col->addStretch();

   auto *icon = new QLabel;
   icon->setPixmap(style()->standardIcon(QStyle::SP_MessageBoxWarning).pixmap(40, 40));
   icon->setAlignment(Qt::AlignCenter);
   col->addWidget(icon);
   m_problemTitle = new QLabel;
   QFont bold = m_problemTitle->font();
   bold.setPointSizeF(bold.pointSizeF() * 1.2);
   bold.setBold(true);
   m_problemTitle->setFont(bold);
   m_problemTitle->setAlignment(Qt::AlignCenter);
   m_problemTitle->setWordWrap(true);
   col->addWidget(m_problemTitle);
   m_problemText = new QLabel;
   m_problemText->setAlignment(Qt::AlignCenter);
   m_problemText->setWordWrap(true);
   col->addWidget(m_problemText);

   auto *copyRow = new QHBoxLayout;
   m_problemCommand = new QLineEdit;
   m_problemCommand->setReadOnly(true);
   m_problemCommand->setAlignment(Qt::AlignCenter);
   copyRow->addWidget(m_problemCommand, 1);
   auto *copy = new QPushButton("Copy");
   connect(copy, &QPushButton::clicked, this,
           [this] { QGuiApplication::clipboard()->setText(m_problemCommand->text()); });
   copyRow->addWidget(copy);
   col->addLayout(copyRow);
   col->addStretch();
   return m_problem;
}

void Window::buildLog(QVBoxLayout *root) {
   m_logToggle = new QToolButton;
   m_logToggle->setText("Activity");
   m_logToggle->setCheckable(true);
   m_logToggle->setAutoRaise(true);
   m_logToggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
   m_logToggle->setArrowType(Qt::RightArrow);
   connect(m_logToggle, &QToolButton::toggled, this, &Window::openLog);
   root->addWidget(m_logToggle, 0, Qt::AlignLeft);

   m_log = new QPlainTextEdit;
   m_log->setReadOnly(true);
   m_log->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
   m_log->setFixedHeight(LOG_PX);
   m_log->setMaximumBlockCount(200);
   for (const QString &line : m_activity.lines())
      m_log->appendPlainText(line);
   m_log->hide();
   root->addWidget(m_log);
}

void Window::openLog(bool open) {
   const bool wasOpen = m_log->isVisible();
   const QSignalBlocker block(m_logToggle);
   m_logToggle->setChecked(open);
   m_logToggle->setArrowType(open ? Qt::DownArrow : Qt::RightArrow);
   if (open == wasOpen)
      return;

   /* The log uses free space first; the window grows only when there is none, then shrinks back */
   setUpdatesEnabled(false);
   if (open)
      m_heightBeforeLog = height();
   m_log->setVisible(open);
   m_outer->activate();
   fitHeight();
   resize(width(), qMax(minimumHeight(), open ? height() : m_heightBeforeLog));
   setUpdatesEnabled(true);
}

// ── Schedulers and modes ───────────────────────────────────────────────

void Window::fillSchedulers() {
   const QSignalBlocker block(m_sched);
   m_sched->clear();
   auto *model = qobject_cast<QStandardItemModel *>(m_sched->model());
   for (const auto group : {catalog::Group::Featured, catalog::Group::Stable,
                            catalog::Group::Experimental, catalog::Group::Other}) {
      bool first = true;
      for (const catalog::Scheduler &s : m_schedulers) {
         if (s.group != group)
            continue;
         if (first) {
            if (m_sched->count() > 0)
               m_sched->insertSeparator(m_sched->count());
            m_sched->addItem(catalog::groupLabel(group));
            QStandardItem *header = model->item(m_sched->count() - 1);
            header->setFlags(Qt::NoItemFlags);
            QFont font = header->font();
            font.setBold(true);
            header->setFont(font);
            first = false;
         }
         m_sched->addItem(s.name, s.binary);
      }
   }
   for (int i = 0; i < m_sched->count(); ++i) {
      if (!m_sched->itemData(i).toString().isEmpty()) {
         m_sched->setCurrentIndex(i);
         break;
      }
   }
   fillModes();
   updateInfo();
}

void Window::fillModes() {
   const QString keep = selectedMode();
   const QSignalBlocker block(m_mode);
   m_mode->clear();
   for (const QString &mode : catalog::modes(selected()))
      m_mode->addItem(mode == "custom" ? "Custom…" : catalog::modeLabel(mode), mode);
   const int index = m_mode->findData(keep);
   m_mode->setCurrentIndex(index >= 0 ? index : 0);
}

QString Window::selected() const {
   return m_sched->currentData().toString();
}

QString Window::selectedMode() const {
   return m_mode->currentData().toString();
}

QStringList Window::selectedOptions() const {
   if (selectedMode() == "custom")
      return m_options->text().split(' ', Qt::SkipEmptyParts);
   return catalog::options(selected(), selectedMode());
}

/* Starting from what the system already has, once */
void Window::syncFromSystem() {
   const Status &s = m_monitor.status();
   if (m_synced || !s.ready)
      return;
   m_synced = true;

   const QString wanted = s.config.sched.isEmpty() ? s.running : s.config.sched;
   const int index = m_sched->findData(wanted);
   if (index >= 0) {
      m_sched->setCurrentIndex(index);
      const QString mode = catalog::modeFor(wanted, s.config.options);
      m_mode->setCurrentIndex(qMax(0, m_mode->findData(mode)));
      if (mode == "custom")
         m_options->setText(s.config.options.join(' '));
   }
   m_bootBox->setChecked(s.units.enabled);
}

void Window::updateInfo() {
   const QString binary = selected();
   const catalog::Scheduler *s = catalog::find(binary);
   const bool custom = selectedMode() == "custom";
   m_optionsLabel->setVisible(custom);
   m_options->setVisible(custom);

   m_name->setText(catalog::name(binary));
   style::setTag(m_focus, s ? s->focus : QString(), Tone::Blue);
   const catalog::Group group = s ? s->group : catalog::Group::Other;
   style::setTag(m_group, catalog::groupLabel(group),
                 group == catalog::Group::Experimental || group == catalog::Group::Other
                    ? Tone::Amber
                    : Tone::Green);
   m_summary->setText(s ? s->summary : "No description available.");
   m_summary->setVisible(true);
   m_best->setText(s ? "Best for: " + s->bestFor : QString());
   m_best->setVisible(s != nullptr);
   const QStringList options = selectedOptions();
   m_runs->setText("Runs: " + (QStringList{binary} + options).join(' '));

   if (m_actionMessage) {
      m_actionMessage = false;
      setMessage(Kind::Info, {});
   }
   updateControls();
}

// ── Status ─────────────────────────────────────────────────────────────

static QString describe(const QString &binary, const QStringList &options) {
   return QString("%1 (%2)").arg(catalog::name(binary),
                                 catalog::modeLabel(catalog::modeFor(binary, options)));
}

void Window::updateStatus() {
   const Status &s = m_monitor.status();
   if (s.kernel == kernel::State::Unsupported) {
      showProblem("This kernel does not support sched_ext",
                  QString("Linux %1 has no /sys/kernel/sched_ext. Start a kernel built with "
                          "CONFIG_SCHED_CLASS_EXT, Linux 6.12 or newer. Debian 13's own kernel "
                          "has it:")
                     .arg(QSysInfo::kernelVersion()),
                  "sudo apt install linux-image-amd64");
      return;
   }
   if (m_schedulers.isEmpty()) {
      showProblem("No sched_ext schedulers are installed",
                  "Install the scx-scheds package from scx-bundler, then open SCX Switcher "
                  "again:",
                  "github.com/hmwassim/scx-bundler/releases");
      return;
   }
   showPage(m_switcher);

   const bool attached = s.kernel == kernel::State::Enabled && !s.running.isEmpty();
   const bool ours = attached && s.units.active && s.config.sched == s.running;
   Tone tone = Tone::Grey;
   QString text = "Kernel default";
   if (m_busy == Busy::Starting) {
      tone = Tone::Blue;
      text = QString("Starting %1…").arg(catalog::name(m_target));
   } else if (m_busy == Busy::Stopping) {
      tone = Tone::Blue;
      text = QString("Stopping %1…").arg(catalog::name(m_target));
   } else if (!s.crashed.isEmpty()) {
      tone = Tone::Red;
      text = QString("%1 crashed").arg(catalog::name(s.crashed));
   } else if (attached) {
      tone = Tone::Green;
      text = ours ? describe(s.running, s.config.options) : catalog::name(s.running);
   }
   m_dot->setPixmap(style::dot(tone, DOT_PX, devicePixelRatioF()));
   m_status->setText(text);
   m_boot->setText(s.units.enabled && !s.config.sched.isEmpty()
                      ? "At boot: " + describe(s.config.sched, s.config.options)
                      : "At boot: kernel default");

   m_stopSlot->setVisible(m_busy == Busy::Stopping || (ours && m_busy == Busy::None));
   m_restart->setVisible(!s.crashed.isEmpty() && m_busy == Busy::None);

   /* Explaining a scheduler this app did not start, unless an action just reported */
   if (!m_actionMessage && m_busy == Busy::None) {
      if (attached && !ours && s.units.loaderActive)
         setMessage(Kind::Warning,
                    QString("scx_loader is running %1. Stop it to switch here.")
                       .arg(catalog::name(s.running)),
                    "Stop scx_loader");
      else if (attached && !ours)
         setMessage(Kind::Warning, QString("%1 was started outside this app. Apply replaces it.")
                                      .arg(catalog::name(s.running)));
      else
         setMessage(Kind::Info, {});
   }
   if (!s.crashed.isEmpty())
      openLog(true);
   updateControls();
}

bool Window::selectionRuns() const {
   const Status &s = m_monitor.status();
   return s.kernel == kernel::State::Enabled && s.units.active && s.running == selected() &&
          s.config.sched == selected() && s.config.options == selectedOptions();
}

void Window::updateControls() {
   const Status &s = m_monitor.status();
   const bool idle = m_busy == Busy::None && s.ready;
   const bool runs = selectionRuns();
   const bool same = runs && m_bootBox->isChecked() == s.units.enabled;

   m_sched->setEnabled(idle);
   m_mode->setEnabled(idle);
   m_options->setEnabled(idle);
   m_bootBox->setEnabled(idle);
   m_stop->setEnabled(idle);
   m_restart->setEnabled(idle);
   m_messageAction->setEnabled(idle);
   m_apply->setEnabled(idle && !same && !selected().isEmpty());
   m_apply->setText(same ? "Running" : "Apply");

   m_applySlot->setCurrentIndex(m_busy == Busy::Starting || m_busy == Busy::Other ? 1 : 0);
   m_stopSlot->setCurrentIndex(m_busy == Busy::Stopping ? 1 : 0);
}

void Window::setMessage(Kind kind, const QString &text, const QString &action) {
   const QStyle::StandardPixmap icon = kind == Kind::Problem   ? QStyle::SP_MessageBoxCritical
                                       : kind == Kind::Warning ? QStyle::SP_MessageBoxWarning
                                                               : QStyle::SP_MessageBoxInformation;
   m_messageIcon->setPixmap(style()->standardIcon(icon).pixmap(16, 16));
   m_messageIcon->setVisible(!text.isEmpty());
   m_message->setText(text);
   m_messageAction->setText(action);
   m_messageAction->setVisible(!action.isEmpty());
}

void Window::showPage(QWidget *page) {
   m_switcher->setVisible(page == m_switcher);
   m_info->setVisible(page == m_switcher);
   m_problem->setVisible(page == m_problem);
}

void Window::showProblem(const QString &title, const QString &text, const QString &command) {
   m_problemTitle->setText(title);
   m_problemText->setText(text);
   m_problemCommand->setText(command);
   showPage(m_problem);
   m_dot->setPixmap(style::dot(Tone::Grey, DOT_PX, devicePixelRatioF()));
   m_status->setText("Kernel default");
   m_boot->setText(m_monitor.status().kernel == kernel::State::Unsupported
                      ? "sched_ext is not available on this kernel"
                      : "No sched_ext schedulers are installed");
   m_stopSlot->hide();
   m_restart->hide();
}

// ── Actions ────────────────────────────────────────────────────────────

void Window::apply() {
   const QStringList options = selectedOptions();
   if (selectedMode() == "custom" && !options.isEmpty() && !catalog::validOptions(options)) {
      m_actionMessage = true;
      setMessage(Kind::Problem, OPTIONS_HINT);
      return;
   }
   const QString boot = m_bootBox->isChecked() ? "on" : "off";
   if (selectionRuns()) {
      runHelper({"boot", boot}, Busy::Other, selected());
      return;
   }
   startScheduler(selected(), options, m_bootBox->isChecked());
}

void Window::startScheduler(const QString &sched, const QStringList &options, bool boot) {
   const Status &s = m_monitor.status();
   const bool running = s.kernel == kernel::State::Enabled && s.units.active;
   m_previous = running ? s.running : QString();
   m_activity.add(QString("Starting [%1]...").arg((QStringList{sched} + options).join(' ')));
   runHelper(QStringList{"start", sched, boot ? "on" : "off"} + options, Busy::Starting, sched);
}

void Window::stop() {
   m_activity.add(QString("Stopping [%1]...").arg(m_monitor.status().running));
   runHelper({"stop"}, Busy::Stopping, m_monitor.status().running);
}

void Window::restart() {
   const Status &s = m_monitor.status();
   m_monitor.clearCrash();
   startScheduler(s.config.sched, s.config.options, s.units.enabled);
}

void Window::stopLoader() {
   runHelper({"stop-loader"}, Busy::Other, {});
}

void Window::runHelper(const QStringList &args, Busy busy, const QString &target) {
   if (!sys::pkexecAvailable()) {
      m_actionMessage = true;
      setMessage(Kind::Problem, "pkexec is missing. Install the pkexec package.");
      return;
   }
   m_busy = busy;
   m_target = target;
   m_actionMessage = false;
   setMessage(Kind::Info, {});
   m_monitor.setBusy(true);
   updateStatus();

   /* The helper outlives a closed window, so it reports to the monitor, not to us */
   QPointer<Window> self(this);
   Monitor *monitor = &m_monitor;
   Activity *activity = &m_activity;
   const QString previous = m_previous;
   sys::helper(monitor, args, [=](int code, const QString &output) {
      if (!output.isEmpty())
         activity->add(output);
      monitor->setBusy(false);
      if (!self)
         return;

      self->m_busy = Busy::None;
      QString text;
      Kind kind = Kind::Problem;
      switch (code) {
      case 0: break;
      case 1:
         text = previous.isEmpty() ? QString("%1 failed to start. See the log.")
                                        .arg(catalog::name(target))
                                   : QString("%1 failed to start. %2 is back. See the log.")
                                        .arg(catalog::name(target), catalog::name(previous));
         self->openLog(true);
         break;
      case 2: text = output.section('\n', 0, 0); break;
      case 126:
         kind = Kind::Info;
         text = "Cancelled. Nothing changed.";
         break;
      case 127: text = "No authentication agent is running. Log in to a desktop session."; break;
      case -1:
         text = "The helper did not answer. See the log.";
         self->openLog(true);
         break;
      default:
         text = QString("Something went wrong (exit %1). See the log.").arg(code);
         self->openLog(true);
         break;
      }
      self->m_actionMessage = !text.isEmpty();
      self->setMessage(kind, text);
      self->updateStatus();
   });
}

// ── Window size and visibility ─────────────────────────────────────────

/* Wrapped text needs more height at small widths, which the minimum size alone ignores */
void Window::fitHeight() {
   const int need = m_outer->hasHeightForWidth() ? m_outer->totalHeightForWidth(width())
                                                 : m_outer->totalMinimumSize().height();
   if (need > 0 && need != minimumHeight())
      setMinimumHeight(need);
   /* On Wayland a bigger minimum doesn't grow a shown window, so grow it here */
   if (need > height())
      resize(width(), need);
}

void Window::resizeEvent(QResizeEvent *event) {
   QWidget::resizeEvent(event);
   fitHeight();
}

bool Window::event(QEvent *event) {
   const bool handled = QWidget::event(event);
   if (event->type() == QEvent::LayoutRequest)
      fitHeight();
   return handled;
}

void Window::showEvent(QShowEvent *event) {
   QWidget::showEvent(event);
   m_monitor.setWindowVisible(true);
}

void Window::hideEvent(QHideEvent *event) {
   QWidget::hideEvent(event);
   m_monitor.setWindowVisible(false);
}

void Window::changeEvent(QEvent *event) {
   QWidget::changeEvent(event);
   if (event->type() == QEvent::WindowStateChange) {
      m_monitor.setWindowVisible(!isMinimized() && isVisible());
   } else if (event->type() == QEvent::PaletteChange && m_sched) {
      /* Picking the tone set again, the theme may have gone from light to dark */
      for (QFrame *frame : findChildren<QFrame *>())
         if (!frame->objectName().isEmpty() && !frame->styleSheet().isEmpty())
            setBorder(frame);
      updateInfo();
      updateStatus();
   }
}
