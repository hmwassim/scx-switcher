#include "mainwindow.h"
#include "control_tab.h"
#include "scx_utils.h"

#include <QApplication>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QFont>
#include <QGroupBox>
#include <QDateTime>
#include <QIcon>
#include <QPixmap>
#include <QPainter>
#include <QColor>
#include <QCloseEvent>
#include <QScrollArea>
#include <QScrollBar>

const QString MainWindow::APP_NAME = "SCX Switcher";
const QString MainWindow::APP_VERSION = "1.0.0";

static QIcon makeDotIcon(const QColor &color) {
    QPixmap pm(16, 16);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(color);
    p.setPen(Qt::NoPen);
    p.drawEllipse(1, 1, 14, 14);
    return QIcon(pm);
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setWindowTitle(APP_NAME);
    setWindowFlags(windowFlags() & ~Qt::WindowMaximizeButtonHint);

    buildUi();

    m_marqueeTimer = new QTimer(this);
    m_marqueeTimer->setInterval(200);
    connect(m_marqueeTimer, &QTimer::timeout, this, &MainWindow::updateMarquee);

    checkKernelAndBuildUi();

    QTimer::singleShot(0, this, [this]() {
        adjustSize();
        setFixedSize(size());
    });
}

void MainWindow::buildUi() {
    auto *cw = new QWidget;
    setCentralWidget(cw);
    auto *root = new QVBoxLayout(cw);
    root->setContentsMargins(10, 10, 10, 8);
    root->setSpacing(6);

    auto *header = new QFrame;
    auto *hl = new QHBoxLayout(header);
    hl->setContentsMargins(8, 4, 8, 4);

    auto *title = new QLabel(APP_NAME);
    QFont tf = title->font();
    tf.setPointSize(15);
    tf.setBold(true);
    title->setFont(tf);
    hl->addWidget(title);
    hl->addStretch();

    m_statusDot = new QLabel("\u25cf");
    QFont sf = m_statusDot->font();
    sf.setPointSize(15);
    m_statusDot->setFont(sf);
    m_statusDot->setStyleSheet("color: #888;");
    m_statusLabel = new QLabel("Not installed");
    m_statusLabel->setMinimumWidth(200);
    hl->addWidget(m_statusDot);
    hl->addWidget(m_statusLabel);
    hl->addSpacing(4);

    m_stopBtn = new QPushButton("Stop");
    m_stopBtn->setMinimumWidth(80);
    connect(m_stopBtn, &QPushButton::clicked, this, &MainWindow::onStopClicked);
    m_stopBtn->setEnabled(false);
    hl->addWidget(m_stopBtn);

    root->addWidget(header);

    m_tabs = new QTabWidget;
    root->addWidget(m_tabs);

    m_logView = new QTextEdit;
    m_logView->setReadOnly(true);
    m_logView->setMaximumHeight(120);
    QFont mf("Monospace", 9);
    m_logView->setFont(mf);
    auto *logGroup = new QGroupBox("Log");
    auto *logLayout = new QVBoxLayout(logGroup);
    logLayout->setContentsMargins(4, 4, 4, 4);
    logLayout->addWidget(m_logView);
    root->addWidget(logGroup);
}

void MainWindow::buildSetupMode() {
    auto *page = new QWidget;
    auto *l = new QVBoxLayout(page);
    l->setAlignment(Qt::AlignCenter);

    auto *icon = new QLabel("\u26a0\ufe0f");
    QFont if2 = icon->font();
    if2.setPointSize(28);
    icon->setFont(if2);
    icon->setAlignment(Qt::AlignCenter);
    l->addWidget(icon);

    auto *msg = new QLabel("scxctl not found");
    QFont mf2 = msg->font();
    mf2.setPointSize(14);
    mf2.setBold(true);
    msg->setFont(mf2);
    msg->setAlignment(Qt::AlignCenter);
    l->addWidget(msg);

    auto *hint = new QLabel(
        "Run <b>./install.sh</b> in the scx-switcher directory<br>"
        "to install schedulers and scxctl.");
    hint->setAlignment(Qt::AlignCenter);
    hint->setStyleSheet("color: #999;");
    l->addWidget(hint);

    auto *btn = new QPushButton("Exit");
    btn->setMaximumWidth(100);
    connect(btn, &QPushButton::clicked, qApp, &QApplication::quit);
    auto *btnRow = new QHBoxLayout;
    btnRow->setAlignment(Qt::AlignCenter);
    btnRow->addWidget(btn);
    l->addLayout(btnRow);

    m_tabs->addTab(page, "Setup");
    m_tabs->setTabEnabled(0, false);
}

void MainWindow::buildNormalMode() {
    m_controlTab = new ControlTab;
    connect(m_controlTab, &ControlTab::logMessage, this, &MainWindow::log);
    connect(m_controlTab, &ControlTab::statusRefreshRequested, this, &MainWindow::refreshStatus);

    struct SchedRef {
        QString bare, display, category, desc;
        QStringList modes;
    };

    static const QList<SchedRef> ALL_SCHED_REF = {
        {"bpfland", "BPFland", "Gaming / Interactive",
         "A vruntime-based scheduler prioritising interactive tasks that block frequently. "
         "Cache-topology aware \u2014 keeps tasks near their L2/L3 cache. Recommended default for "
         "desktop and gaming.",
         {"auto", "gaming", "lowlatency", "powersave", "server"}},
        {"lavd", "LAVD", "Gaming / Low Latency",
         "Latency-Aware Virtual Deadline scheduler. Computes a latency-criticality score per "
         "task and assigns virtual deadlines. Core Compaction keeps active cores at high "
         "frequency. Autopilot mode auto-switches between performance/balanced/powersave.",
         {"auto", "gaming", "lowlatency", "powersave", "server"}},
        {"rusty", "Rusty", "Desktop / Server",
         "Partitions CPUs by last-level cache domain to minimise cross-cache migration. "
         "Good scalability on high core-count systems. Hybrid BPF + userspace design.",
         {"auto", "gaming"}},
        {"flash", "Flash", "Desktop / Soft RT",
         "Emphasises fairness and predictability over prioritising interactive tasks. "
         "Good for batch, encoding, and audio workloads.",
         {"auto", "gaming", "powersave"}},
        {"cosmos", "Cosmos", "General Purpose",
         "Lightweight locality-first scheduler. Keeps tasks on the same CPU using local "
         "DSQs when not saturated. Under load switches to deadline-based policy. "
         "10\u00b5s default time slices. Low overhead general-purpose choice.",
         {"auto", "gaming", "lowlatency", "powersave", "server"}},
        {"layered", "Layered", "Power Users",
         "Classifies threads into named layers (like cgroups) with independent scheduling "
         "policies per layer. Highly flexible but requires manual JSON config.",
         {"auto"}},
        {"nest", "Nest", "Lightly-Loaded",
         "Places tasks on already-warm, high-frequency cores to keep turbo boost active. "
         "Effective when CPU utilisation is low to moderate.",
         {"auto", "powersave"}},
        {"p2dq", "P2DQ", "Mixed Desktop/Server",
         "Pick-2 randomised load balancing keeps queues shallow. Simple design means low "
         "scheduler overhead. PELT-based load tracking.",
         {"auto", "gaming", "powersave", "server"}},
        {"tickless", "Tickless", "Cloud / HPC",
         "Routes scheduling through a small pool of primary CPUs, allowing others to run "
         "tickless (no scheduler interrupts). Reduces OS noise for VMs and HPC.",
         {"auto", "powersave", "server"}},
        {"simple", "Simple", "Reference / Testing",
         "Minimal FIFO/least-runtime policy. No topology awareness. Useful as a baseline "
         "for benchmarking and understanding sched_ext.",
         {"auto"}},
        {"beerland", "Beerland", "Gaming / Desktop",
         "A reduced-overhead variant of scx_bpfland by the same author. Strips back "
         "expensive per-task tracking for lower scheduler overhead on busy systems.",
         {"auto", "gaming", "lowlatency", "powersave", "server"}},
        {"rustland", "Rustland", "Userspace / Educational",
         "Predecessor to bpfland with similar logic but running in userspace (Rust). "
         "More readable for learning but adds context-switch overhead.",
         {"auto"}},
    };

    auto *refTab = new QWidget;
    auto *refLayout = new QVBoxLayout(refTab);
    auto *refTitle = new QLabel("Scheduler Reference");
    QFont rf = refTitle->font();
    rf.setPointSize(13);
    rf.setBold(true);
    refTitle->setFont(rf);
    refLayout->addWidget(refTitle);

    QStringList installed = scx_utils::listSchedulers();
    if (installed.isEmpty()) {
        installed = {"bpfland", "lavd", "rusty", "flash", "layered",
                     "cosmos", "nest", "p2dq", "simple", "tickless"};
    }

    for (const auto &r : ALL_SCHED_REF) {
        if (!installed.contains(r.bare))
            continue;

        auto *card = new QGroupBox;
        auto *cl = new QVBoxLayout(card);
        cl->setContentsMargins(10, 6, 10, 6);

        auto *hl = new QHBoxLayout;
        auto *nl = new QLabel(r.display);
        QFont nf = nl->font();
        nf.setPointSize(12);
        nf.setBold(true);
        nl->setFont(nf);
        hl->addWidget(nl);
        hl->addWidget(new QLabel(QString("(scx_%1)").arg(r.bare)));
        hl->addStretch();
        auto *cl2 = new QLabel(r.category);
        cl2->setStyleSheet("color: #88aaff; font-size: 11px; padding: 2px 6px; "
                           "border: 1px solid #6688cc; border-radius: 4px;");
        hl->addWidget(cl2);
        cl->addLayout(hl);

        auto *dl = new QLabel(r.desc);
        dl->setWordWrap(true);
        dl->setStyleSheet("color: #bbb; font-size: 12px;");
        cl->addWidget(dl);

        QStringList modeLabels;
        for (const auto &m : r.modes)
            modeLabels << scx_utils::humanizeMode(m);
        auto *ml = new QLabel(QString("Supported modes: %1").arg(modeLabels.join(", ")));
        ml->setStyleSheet("color: #88dd88; font-size: 11px; margin-top: 4px;");
        cl->addWidget(ml);

        refLayout->addWidget(card);
    }
    refLayout->addStretch();
    auto *scrollArea = new QScrollArea;
    scrollArea->setWidgetResizable(true);
    scrollArea->setWidget(refTab);
    scrollArea->setFrameShape(QFrame::NoFrame);

    m_tabs->addTab(m_controlTab, "Control");
    m_tabs->addTab(scrollArea, "Reference");

    m_tray = new QSystemTrayIcon(makeDotIcon(QColor("#888888")), this);
    auto *menu = new QMenu;
    menu->addAction("Show Window", this, [this]() { show(); raise(); activateWindow(); });
    menu->addSeparator();
    menu->addAction("Quit", qApp, &QApplication::quit);
    m_tray->setContextMenu(menu);
    m_tray->setToolTip(APP_NAME);
    m_tray->show();
    connect(m_tray, &QSystemTrayIcon::activated, this,
        [this](QSystemTrayIcon::ActivationReason r) {
            if (r == QSystemTrayIcon::DoubleClick) {
                show(); raise(); activateWindow();
            }
        });

    log(APP_NAME + " initialized");

    m_statusTimer = new QTimer(this);
    m_statusTimer->setInterval(3000);
    connect(m_statusTimer, &QTimer::timeout, this, &MainWindow::refreshStatus);
    refreshStatus();
    m_statusTimer->start();
}

void MainWindow::checkKernelAndBuildUi() {
    auto [supported, msg] = scx_utils::checkKernelSupport();
    m_kernelSupported = supported;

    if (!supported) {
        m_statusDot->setStyleSheet("color: #cc0000;");
        m_statusLabel->setText("Kernel unsupported \u2014 " + msg);
        m_stopBtn->setEnabled(false);
        m_tabs->addTab(buildUnsupportedPage(), "Unsupported Kernel");
        return;
    }

    if (scx_utils::isScxctlInstalled()) {
        buildNormalMode();
    } else {
        buildSetupMode();
        m_statusLabel->setText("scxctl not found");
    }
}

QWidget *MainWindow::buildUnsupportedPage() {
    auto *page = new QWidget;
    auto *l = new QVBoxLayout(page);
    l->setAlignment(Qt::AlignCenter);

    auto *icon = new QLabel("\u26a0\ufe0f");
    QFont if2 = icon->font();
    if2.setPointSize(28);
    icon->setFont(if2);
    icon->setAlignment(Qt::AlignCenter);
    l->addWidget(icon);

    auto *title = new QLabel("Kernel Unsupported");
    QFont tf = title->font();
    tf.setPointSize(16);
    tf.setBold(true);
    title->setFont(tf);
    title->setAlignment(Qt::AlignCenter);
    l->addWidget(title);

    auto *sub = new QLabel("Does not support sched_ext");
    sub->setAlignment(Qt::AlignCenter);
    sub->setStyleSheet("color: #999; font-size: 13px;");
    l->addWidget(sub);

    auto *hint = new QLabel("Install a kernel with sched_ext support:");
    hint->setAlignment(Qt::AlignCenter);
    hint->setStyleSheet("color: #ccc; margin-top: 12px;");
    l->addWidget(hint);

    auto *cmd = new QLineEdit(
        "sudo apt install -t trixie-backports linux-image-amd64 linux-headers-amd64");
    cmd->setReadOnly(true);
    cmd->setAlignment(Qt::AlignCenter);
    cmd->setStyleSheet(
        "QLineEdit { background: #2d2d2d; color: #88dd88; border: 1px solid #555; "
        "border-radius: 4px; padding: 6px 10px; font-family: monospace; font-size: 12px; }");
    QFontMetrics fm2(cmd->font());
    cmd->setMinimumWidth(fm2.horizontalAdvance(cmd->text()) + 30);
    l->addWidget(cmd);

    auto *note = new QLabel(
        "Or any Linux 6.12+ kernel with CONFIG_SCHED_CLASS_EXT enabled.\n"
        "Select the command above to copy, then reboot after installing.");
    note->setWordWrap(true);
    note->setAlignment(Qt::AlignCenter);
    note->setStyleSheet("color: #777; font-size: 11px; margin-top: 8px;");
    l->addWidget(note);

    return page;
}

void MainWindow::refreshStatus() {
    auto [kernelOk, kernelMsg] = scx_utils::checkKernelSupport();
    if (!kernelOk) {
        m_statusDot->setStyleSheet("color: #cc0000;");
        m_statusLabel->setText("Kernel unsupported");
        m_statusLabel->setToolTip(kernelMsg);
        m_stopBtn->setEnabled(false);
        toggleTrayIcon(false);
        m_marqueeTimer->stop();
        return;
    }
    auto s = scx_utils::getSchedulerStatus();
    if (s.active) {
        m_statusDot->setStyleSheet("color: #00cc00;");
        QString text = QString("Running: %1 (%2)")
            .arg(scx_utils::humanizeScheduler(s.name),
                 scx_utils::humanizeMode(s.mode));
        m_statusLabel->setText(text);
        m_statusLabel->setToolTip(text);
        m_stopBtn->setEnabled(true);
        toggleTrayIcon(true, s.name);

        QFontMetrics fm(m_statusLabel->font());
        if (fm.horizontalAdvance(text) > m_statusLabel->width()) {
            m_marqueeText = text;
            m_marqueeOffset = 0;
            m_marqueeTimer->start();
        } else {
            m_marqueeTimer->stop();
        }
    } else {
        m_statusDot->setStyleSheet("color: #cc0000;");
        m_statusLabel->setText("EEVDF (default)");
        m_statusLabel->setToolTip({});
        m_stopBtn->setEnabled(false);
        toggleTrayIcon(false);
        m_marqueeTimer->stop();
    }
}

void MainWindow::updateMarquee() {
    m_marqueeOffset++;
    if (m_marqueeOffset > m_marqueeText.length() + 4)
        m_marqueeOffset = 0;
    m_statusLabel->setText(
        m_marqueeText.mid(m_marqueeOffset) + "    " +
        m_marqueeText.left(m_marqueeOffset));
}

void MainWindow::onStopClicked() {
    if (m_controlTab) m_controlTab->stopScheduler();
}

void MainWindow::log(const QString &msg) {
    QString ts = QDateTime::currentDateTime().toString("hh:mm:ss");
    m_logView->append(QString("[%1] %2").arg(ts, msg));
    auto *sb = m_logView->verticalScrollBar();
    sb->setValue(sb->maximum());
}

void MainWindow::toggleTrayIcon(bool running, const QString &name) {
    if (!m_tray) return;
    if (running) {
        m_tray->setIcon(makeDotIcon(QColor("#00cc00")));
        m_tray->setToolTip(QString("SCX: running %1").arg(name));
    } else {
        m_tray->setIcon(makeDotIcon(QColor("#cc0000")));
        m_tray->setToolTip("SCX: no scheduler running");
    }
}

void MainWindow::closeEvent(QCloseEvent *event) {
    if (m_tray && m_tray->isVisible()) {
        hide();
        event->ignore();
    } else {
        if (m_statusTimer) m_statusTimer->stop();
        event->accept();
    }
}
