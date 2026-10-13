#include "mainwindow.h"

#include "catalog.h"
#include "distro.h"
#include "installedview.h"
#include "ui.h"

#include <QApplication>
#include <QButtonGroup>
#include <QCloseEvent>
#include <QFontDatabase>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPushButton>
#include <QScrollBar>
#include <QStackedWidget>
#include <QSysInfo>
#include <QSystemTrayIcon>
#include <QTabWidget>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>

using ui::Tone;

static constexpr int POLL_MS = 2000;
static constexpr int HELPER_TIMEOUT_MS = 120000; // includes typing the password
static const QStringList ALL_MODES = {"auto", "gaming", "lowlatency", "powersave", "server"};

static QString describe(const QString &sched, const QString &mode) {
    return QString("%1 (%2)").arg(catalog::displayName(sched), catalog::modeLabel(mode));
}

static QLabel *styledLabel(qreal scale, bool bold, bool muted = false) {
    auto *label = new QLabel;
    QFont font = label->font();
    font.setPointSizeF(font.pointSizeF() * scale);
    font.setBold(bold);
    label->setFont(font);
    if (muted)
        label->setForegroundRole(QPalette::PlaceholderText);
    return label;
}

static QLabel *caption(const QString &text) {
    auto *label = styledLabel(0.8, true, true);
    label->setText(text.toUpper());
    return label;
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setWindowTitle("SCX Switcher");
    setMinimumSize(780, 620);

    auto *central = new QWidget;
    auto *root = new QVBoxLayout(central);
    root->setContentsMargins(14, 12, 14, 12);
    root->setSpacing(10);
    setCentralWidget(central);

    root->addWidget(buildStatusCard());

    m_tabs = new QTabWidget;
    m_tabs->setDocumentMode(true);
    m_tabs->addTab(buildSchedulerTab(), "Schedulers");
    m_installed = new InstalledView;
    m_tabs->addTab(m_installed, "Installed");

    m_problemPage = buildProblemPage();
    m_pages = new QStackedWidget;
    m_pages->addWidget(m_tabs);
    m_pages->addWidget(m_problemPage);
    root->addWidget(m_pages, 1);

    root->addWidget(caption("Activity"));
    m_log = new QTextEdit;
    m_log->setReadOnly(true);
    m_log->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_log->setFixedHeight(110);
    root->addWidget(m_log);

    m_poll = new QTimer(this);
    m_poll->setInterval(POLL_MS);
    connect(m_poll, &QTimer::timeout, this, &MainWindow::refreshStatus);

    setupTray();
    log(QString("SCX Switcher %1").arg(APP_VERSION), Tone::Grey);
    startup();
}

QWidget *MainWindow::buildStatusCard() {
    m_card = new QFrame;
    m_card->setObjectName("statusCard");
    auto *row = new QHBoxLayout(m_card);
    row->setContentsMargins(16, 12, 16, 12);
    row->setSpacing(14);

    m_dot = new QLabel;
    row->addWidget(m_dot, 0, Qt::AlignVCenter);

    auto *col = new QVBoxLayout;
    col->setSpacing(2);
    col->addWidget(caption("Now running"));
    auto *titleRow = new QHBoxLayout;
    titleRow->setSpacing(8);
    m_statusText = styledLabel(1.45, true);
    m_statusText->setWordWrap(true);
    titleRow->addWidget(m_statusText);
    m_modeBadge = ui::badge({}, Tone::Blue);
    titleRow->addWidget(m_modeBadge, 0, Qt::AlignVCenter);
    titleRow->addStretch();
    col->addLayout(titleRow);
    m_bootText = styledLabel(1.0, false, true);
    col->addWidget(m_bootText);
    row->addLayout(col, 1);

    m_stopBtn = new QPushButton("Stop");
    m_stopBtn->setToolTip("Stop the scheduler and use the kernel default at boot");
    m_stopBtn->setMinimumSize(90, 32);
    m_stopBtn->setVisible(false);
    connect(m_stopBtn, &QPushButton::clicked, this, &MainWindow::stop);
    row->addWidget(m_stopBtn, 0, Qt::AlignVCenter);
    return m_card;
}

QWidget *MainWindow::buildSchedulerTab() {
    auto *page = new QWidget;
    auto *row = new QHBoxLayout(page);
    row->setContentsMargins(0, 8, 0, 0);
    row->setSpacing(12);

    m_schedList = new QListWidget;
    m_schedList->setFixedWidth(190);
    m_schedList->setIconSize(QSize(12, 12));
    m_schedList->setSpacing(2);
    connect(m_schedList, &QListWidget::currentRowChanged, this, &MainWindow::onSchedulerChanged);
    row->addWidget(m_schedList);

    auto *detail = new QFrame;
    detail->setObjectName("detail");
    detail->setStyleSheet("#detail { border: 1px solid palette(mid); border-radius: 10px; }");
    auto *col = new QVBoxLayout(detail);
    col->setContentsMargins(18, 14, 18, 14);
    col->setSpacing(10);

    auto *titleRow = new QHBoxLayout;
    titleRow->setSpacing(8);
    m_title = styledLabel(1.6, true);
    titleRow->addWidget(m_title);
    titleRow->addStretch();
    m_tagBadge = ui::badge({}, Tone::Purple);
    m_maturityBadge = ui::badge({}, Tone::Grey);
    titleRow->addWidget(m_tagBadge);
    titleRow->addWidget(m_maturityBadge);
    col->addLayout(titleRow);

    m_summary = new QLabel;
    m_summary->setWordWrap(true);
    col->addWidget(m_summary);
    m_bestFor = new QLabel;
    m_bestFor->setWordWrap(true);
    m_bestFor->setForegroundRole(QPalette::PlaceholderText);
    col->addWidget(m_bestFor);

    col->addSpacing(4);
    col->addWidget(caption("Mode"));
    m_modeRow = new QHBoxLayout;
    m_modeRow->setSpacing(6);
    col->addLayout(m_modeRow);
    m_modeGroup = new QButtonGroup(this);
    m_modeGroup->setExclusive(true);
    connect(m_modeGroup, &QButtonGroup::buttonClicked, this, &MainWindow::updateControls);
    col->addStretch();

    auto *bottom = new QHBoxLayout;
    m_message = new QLabel;
    m_message->setWordWrap(true);
    m_message->setTextInteractionFlags(Qt::TextSelectableByMouse);
    bottom->addWidget(m_message, 1);
    m_applyBtn = new QPushButton("Apply");
    m_applyBtn->setObjectName("apply");
    m_applyBtn->setDefault(true);
    m_applyBtn->setMinimumSize(110, 34);
    m_applyBtn->setToolTip("Run this scheduler now and at every boot");
    const QColor accent = ui::color(Tone::Blue);
    m_applyBtn->setStyleSheet(
        QString("#apply { background: %1; color: white; border: none; border-radius: 6px;"
                " padding: 6px 18px; font-weight: 600; }"
                "#apply:hover { background: %2; }"
                "#apply:disabled { background: palette(mid); color: palette(window); }")
            .arg(accent.name(), accent.lighter(115).name()));
    connect(m_applyBtn, &QPushButton::clicked, this, &MainWindow::apply);
    bottom->addWidget(m_applyBtn, 0, Qt::AlignBottom);
    col->addLayout(bottom);

    row->addWidget(detail, 1);
    return page;
}

QWidget *MainWindow::buildProblemPage() {
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->addStretch();

    auto *icon = new QLabel;
    icon->setPixmap(ui::chip(Tone::Red).pixmap(48, 48));
    icon->setAlignment(Qt::AlignCenter);
    layout->addWidget(icon);

    m_problemTitle = styledLabel(1.4, true);
    m_problemTitle->setAlignment(Qt::AlignCenter);
    m_problemTitle->setWordWrap(true);
    layout->addWidget(m_problemTitle);

    m_problemText = new QLabel;
    m_problemText->setWordWrap(true);
    m_problemText->setAlignment(Qt::AlignCenter);
    m_problemText->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(m_problemText);

    m_problemCmd = new QLineEdit;
    m_problemCmd->setReadOnly(true);
    m_problemCmd->setAlignment(Qt::AlignCenter);
    m_problemCmd->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_problemCmd->setStyleSheet(QString("QLineEdit { color: %1; padding: 6px; }")
                                    .arg(ui::color(Tone::Green).name()));
    layout->addWidget(m_problemCmd);

    auto *buttons = new QHBoxLayout;
    buttons->addStretch();
    m_repairBtn = new QPushButton("Repair");
    m_repairBtn->setToolTip("Clear the boot default and restart scx_loader");
    connect(m_repairBtn, &QPushButton::clicked, this, &MainWindow::repair);
    buttons->addWidget(m_repairBtn);
    auto *retry = new QPushButton("Retry");
    connect(retry, &QPushButton::clicked, this, &MainWindow::startup);
    buttons->addWidget(retry);
    buttons->addStretch();
    layout->addLayout(buttons);
    layout->addStretch();
    return page;
}

void MainWindow::setupTray() {
    if (!QSystemTrayIcon::isSystemTrayAvailable())
        return;

    m_tray = new QSystemTrayIcon(ui::chip(Tone::Grey), this);
    auto *menu = new QMenu(this);
    menu->addAction("Show", this, [this] {
        show();
        raise();
        activateWindow();
    });
    menu->addAction("Quit", qApp, &QApplication::quit);
    m_tray->setContextMenu(menu);
    connect(m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason r) {
        if (r == QSystemTrayIcon::Trigger)
            setVisible(!isVisible());
    });
    m_tray->show();
}

void MainWindow::closeEvent(QCloseEvent *event) {
    if (m_tray) {
        hide();
        event->ignore();
    } else {
        event->accept();
    }
}

// ── Startup checks ─────────────────────────────────────────────────────────

static void paintCard(QFrame *card, QLabel *dot, Tone tone) {
    const QColor c = ui::color(tone);
    card->setStyleSheet(QString("#statusCard { border: 1px solid palette(mid);"
                                " border-left: 5px solid %1; border-radius: 10px;"
                                " background: rgba(%2, %3, %4, 0.06); }")
                            .arg(c.name())
                            .arg(c.red())
                            .arg(c.green())
                            .arg(c.blue()));
    dot->setPixmap(ui::dot(tone, 26).pixmap(26, 26));
}

void MainWindow::startup() {
    m_poll->stop();
    paintCard(m_card, m_dot, Tone::Grey);
    m_bootText->clear();
    m_modeBadge->hide();
    m_stopBtn->setVisible(false);

    if (scx::kernelState() == scx::Kernel::Unsupported) {
        showProblem("This kernel does not support sched_ext",
                    QString("Linux %1 has no /sys/kernel/sched_ext. Boot a 6.12 or newer "
                            "kernel built with CONFIG_SCHED_CLASS_EXT.")
                        .arg(QSysInfo::kernelVersion()),
                    distro::kernelInstallCommand());
        return;
    }
    if (!scx::scxctlAvailable()) {
        const QString cmd = distro::scxInstallCommand();
        showProblem("scx-tools is not installed",
                    cmd.isEmpty() ? "Install scx-scheds and scx-tools, which provide scxctl and "
                                    "scx_loader. On Debian, build them with scx-bundler."
                                  : QString("Install scx-scheds and scx-tools for %1:")
                                        .arg(distro::prettyName()),
                    cmd);
        return;
    }

    m_statusText->setText("Checking…");
    m_pages->setCurrentWidget(m_tabs);
    loadSchedulers();
}

void MainWindow::showProblem(const QString &title, const QString &text, const QString &command,
                             bool offerRepair) {
    m_poll->stop();
    paintCard(m_card, m_dot, Tone::Red);
    m_statusText->setText(title);
    m_modeBadge->hide();
    m_problemTitle->setText(title);
    m_problemText->setText(text);
    m_problemCmd->setText(command);
    m_problemCmd->setCursorPosition(0);
    m_problemCmd->setVisible(!command.isEmpty());
    m_repairBtn->setVisible(offerRepair);
    m_repairBtn->setEnabled(!m_busy);
    m_pages->setCurrentWidget(m_problemPage);
    if (m_tray) {
        m_tray->setIcon(ui::chip(Tone::Red));
        m_tray->setToolTip("SCX Switcher: " + title);
    }
}

// ── State ──────────────────────────────────────────────────────────────────

void MainWindow::loadSchedulers() {
    scx::run(this, "scxctl", {"list"}, [this](int code, const QString &out) {
        const QStringList names = code == 0 ? scx::parseList(out) : QStringList{};
        if (names.isEmpty()) {
            log("scxctl list: " + out, Tone::Red);
            QString text = "scxctl could not reach scx_loader.";
            if (scx::bootDefault().emptyConfig)
                text += " /etc/scx_loader/config.toml is empty, which stops scx_loader from "
                        "starting.";
            showProblem("scx_loader is not running",
                        text + " Repair clears the boot default (and resets the config if it "
                               "is invalid), then restarts scx_loader. Details:",
                        "sudo journalctl -b -u scx_loader.service", true);
            return;
        }

        m_synced = false;
        {
            const QSignalBlocker block(m_schedList);
            m_schedList->clear();
            for (const QString &name : names) {
                auto *item = new QListWidgetItem(catalog::displayName(name), m_schedList);
                item->setData(Qt::UserRole, name);
                item->setSizeHint(QSize(0, 30));
            }
        }
        markRunning();
        m_schedList->setCurrentRow(0);
        m_installed->setLoaderSupported(names);
        refreshBoot();
        refreshStatus();
        m_poll->start();
    });
}

void MainWindow::refreshStatus() {
    if (m_querying)
        return;
    m_querying = true;
    scx::run(this, "scxctl", {"get"}, [this](int code, const QString &out) {
        m_querying = false;
        setStatus(code == 0, scx::parseStatus(out), out);
    });
}

void MainWindow::setStatus(bool reachable, const scx::Status &loader, const QString &error) {
    if (!reachable) {
        log("scxctl get: " + error, Tone::Red);
        loadSchedulers(); // shows the problem page with Repair
        return;
    }

    const bool active = scx::kernelState() == scx::Kernel::Enabled;
    const bool changed = loader.running != m_loader.running || loader.sched != m_loader.sched ||
                         loader.mode != m_loader.mode || active != m_active;
    m_loader = loader;
    m_active = active;

    Tone tone = Tone::Grey;
    QString text = "EEVDF";
    QString mode = "Kernel default";
    if (m_active && loader.running) {
        tone = Tone::Green;
        text = catalog::displayName(loader.sched);
        mode = catalog::modeLabel(loader.mode);
    } else if (m_active) {
        tone = Tone::Green;
        text = "sched_ext scheduler";
        mode = "Not started by scx_loader";
    } else if (loader.running) {
        // scx_loader still lists it, but the kernel says nothing is attached
        tone = Tone::Red;
        text = QString("%1 is not running").arg(catalog::displayName(loader.sched));
        mode = "Crashed or failed to load";
    }

    paintCard(m_card, m_dot, tone);
    m_statusText->setText(text);
    ui::setBadge(m_modeBadge, mode, tone == Tone::Red ? Tone::Red : Tone::Blue);
    m_stopBtn->setVisible(loader.running);
    if (m_tray) {
        m_tray->setIcon(ui::chip(tone));
        m_tray->setToolTip(QString("SCX Switcher: %1 (%2)").arg(text, mode));
    }
    if (changed) {
        markRunning();
        log(QString("%1 (%2)").arg(text, mode), tone);
    }

    // Start from whatever is running, then leave the selection to the user.
    if (!m_synced) {
        m_synced = true;
        if (m_active && loader.running) {
            for (int i = 0; i < m_schedList->count(); ++i) {
                if (m_schedList->item(i)->data(Qt::UserRole).toString() == loader.sched) {
                    m_wantedMode = loader.mode;
                    if (m_schedList->currentRow() == i)
                        onSchedulerChanged(); // no currentRowChanged signal in this case
                    else
                        m_schedList->setCurrentRow(i);
                    break;
                }
            }
        }
    }
    updateControls();
}

void MainWindow::refreshBoot() {
    const auto boot = scx::bootDefault();
    if (boot.emptyConfig)
        m_bootText->setText("At boot: scx_loader will fail, its config file is empty");
    else if (boot.sched.isEmpty())
        m_bootText->setText("At boot: EEVDF (kernel default)");
    else
        m_bootText->setText("At boot: " + describe(boot.sched, boot.mode));
}

// List icons: green for the running scheduler, otherwise its maturity.
void MainWindow::markRunning() {
    for (int i = 0; i < m_schedList->count(); ++i) {
        auto *item = m_schedList->item(i);
        const QString name = item->data(Qt::UserRole).toString();
        const bool running = m_active && m_loader.running && m_loader.sched == name;
        const auto *info = catalog::scheduler(name);
        // Green is kept for "running"; otherwise flag experimental or unsafe ones.
        Tone tone = info ? ui::maturityTone(info->maturity) : Tone::Grey;
        if (tone == Tone::Green)
            tone = Tone::Grey;
        item->setIcon(ui::dot(running ? Tone::Green : tone));
        QFont font = item->font();
        font.setBold(running);
        item->setFont(font);
        item->setToolTip(running ? "Running" : (info ? catalog::maturityLabel(info->maturity)
                                                     : QString()));
    }
}

QString MainWindow::selectedScheduler() const {
    const auto *item = m_schedList->currentItem();
    return item ? item->data(Qt::UserRole).toString() : QString();
}

QString MainWindow::selectedMode() const {
    const auto *button = m_modeGroup->checkedButton();
    return button ? button->property("mode").toString() : QString();
}

void MainWindow::onSchedulerChanged() {
    const QString sched = selectedScheduler();
    if (sched.isEmpty())
        return;
    if (m_wantedMode.isEmpty())
        m_wantedMode = selectedMode();

    const auto *info = catalog::scheduler(sched);
    m_title->setText(catalog::displayName(sched));
    ui::setBadge(m_tagBadge, info ? info->tag : QString(), Tone::Purple);
    ui::setBadge(m_maturityBadge, info ? catalog::maturityLabel(info->maturity) : QString(),
                 info ? ui::maturityTone(info->maturity) : Tone::Grey);
    m_summary->setText(info ? info->summary : "No description available.");
    m_bestFor->setText(info ? QString("<b>Best for:</b> %1").arg(info->bestFor.toHtmlEscaped())
                            : QString());

    if (m_modes.contains(sched)) {
        showModes(m_modes.value(sched));
        return;
    }
    showModes({});
    scx::run(this, "scxctl", {"modes", "--sched", sched}, [this, sched](int code,
                                                                       const QString &out) {
        QStringList modes = code == 0 ? scx::parseModes(out) : QStringList{};
        if (modes.isEmpty())
            modes = ALL_MODES; // older scxctl without `modes`; the loader accepts them all
        m_modes.insert(sched, modes);
        if (selectedScheduler() == sched)
            showModes(modes);
    });
}

// Mode pills; an empty list means the modes are still loading.
void MainWindow::showModes(const QStringList &modes) {
    for (auto *button : m_modeGroup->buttons()) {
        m_modeGroup->removeButton(button);
        delete button;
    }
    while (QLayoutItem *item = m_modeRow->takeAt(0)) {
        delete item->widget();
        delete item;
    }

    const QColor accent = ui::color(Tone::Blue);
    const QString style =
        QString("QPushButton { border: 1px solid palette(mid); border-radius: 14px;"
                " padding: 5px 14px; }"
                "QPushButton:hover { border-color: %1; }"
                "QPushButton:checked { background: %1; border-color: %1; color: white;"
                " font-weight: 600; }")
            .arg(accent.name());

    const QString want = modes.contains(m_wantedMode) ? m_wantedMode : QString("auto");
    for (const QString &mode : modes) {
        auto *button = new QPushButton(catalog::modeLabel(mode));
        button->setCheckable(true);
        button->setProperty("mode", mode);
        button->setStyleSheet(style);
        button->setChecked(mode == want);
        m_modeGroup->addButton(button);
        m_modeRow->addWidget(button);
    }
    QString note;
    if (modes.isEmpty())
        note = "Loading modes\u2026";
    else if (modes.size() == 1)
        note = "No other modes are tuned for this scheduler.";
    if (!note.isEmpty()) {
        auto *label = styledLabel(1.0, false, true);
        label->setText(note);
        m_modeRow->addWidget(label);
    }
    m_modeRow->addStretch();
    if (!modes.isEmpty())
        m_wantedMode.clear();
    updateControls();
}

void MainWindow::updateControls() {
    const QString sched = selectedScheduler();
    const QString mode = selectedMode();
    const bool current =
        m_active && m_loader.running && m_loader.sched == sched && m_loader.mode == mode;

    m_schedList->setEnabled(!m_busy);
    for (auto *button : m_modeGroup->buttons())
        button->setEnabled(!m_busy);
    m_applyBtn->setEnabled(!m_busy && !sched.isEmpty() && !mode.isEmpty() && !current);
    m_applyBtn->setText(m_busy ? "Working…" : current ? "Running" : "Apply");
    m_stopBtn->setEnabled(!m_busy);
    m_repairBtn->setEnabled(!m_busy);
}

// ── Privileged operations ──────────────────────────────────────────────────

void MainWindow::apply() {
    const QString sched = selectedScheduler();
    const QString mode = selectedMode();
    const QString name = describe(sched, mode);
    runHelper({"apply", sched, mode}, QString("Starting %1…").arg(name),
              QString("%1 is running and will start at boot.").arg(name));
}

void MainWindow::stop() {
    runHelper({"stop"}, "Stopping…",
              "Stopped. The kernel default (EEVDF) is used now and at boot.");
}

void MainWindow::repair() {
    runHelper({"repair"}, "Repairing scx_loader…", {}, [this](bool ok) {
        if (ok)
            startup();
    });
}

void MainWindow::runHelper(const QStringList &args, const QString &progress,
                           const QString &success, const std::function<void(bool)> &after) {
    if (!scx::pkexecAvailable()) {
        setMessage("pkexec is missing. Install polkit, then log out and back in.", true);
        return;
    }

    m_busy = true;
    m_poll->stop();
    updateControls();
    setMessage(progress);
    log(progress, Tone::Blue);

    scx::run(
        this, "pkexec", QStringList{SCX_HELPER_PATH} + args,
        [this, success, after](int code, const QString &out) {
            m_busy = false;
            const QString summary = out.section('\n', 0, 0);
            if (code == 0) {
                const QString msg = success.isEmpty() ? summary : success;
                setMessage(msg);
                log(msg, Tone::Green);
            } else if (code == 126) {
                setMessage("Cancelled.");
                log("Cancelled", Tone::Amber);
            } else if (code == 127) {
                setMessage("Not authorized. Is a polkit authentication agent running?", true);
                log("Not authorized", Tone::Red);
            } else {
                setMessage(summary.isEmpty() ? QString("Failed with exit code %1.").arg(code)
                                             : summary,
                           true);
                log(out.isEmpty() ? QString("helper exited with %1").arg(code) : out,
                    Tone::Red);
            }

            refreshBoot();
            updateControls();
            if (after)
                after(code == 0);
            if (m_pages->currentWidget() == m_tabs) {
                refreshStatus();
                m_poll->start();
            }
        },
        HELPER_TIMEOUT_MS);
}

void MainWindow::setMessage(const QString &text, bool error) {
    m_message->setStyleSheet(error ? QString("color: %1;").arg(ui::color(Tone::Red).name())
                                   : QString());
    m_message->setText(text);
    if (m_pages->currentWidget() == m_problemPage && error)
        m_problemText->setText(text);
}

void MainWindow::log(const QString &msg, Tone tone) {
    m_log->append(ui::logLine(msg, tone));
    m_log->verticalScrollBar()->setValue(m_log->verticalScrollBar()->maximum());
}
