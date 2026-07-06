#include "mainwindow.h"
#include "controltab.h"
#include "scxutils.h"
#include "config.h"

#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QGroupBox>
#include <QScrollArea>
#include <QScrollBar>
#include <QLineEdit>
#include <QFont>
#include <QFontMetrics>
#include <QDateTime>
#include <QIcon>
#include <QPixmap>
#include <QPainter>
#include <QStackedWidget>
#include <QToolButton>
#include <memory>

static constexpr int STATUS_POLL_INTERVAL_MS = 3000;

// ── Helpers ───────────────────────────────────────────────────────────────────

QIcon MainWindow::trayIcon(const QColor &color) {
    QPixmap pm(16, 16);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);

    QPen pen(color, 1.0);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(QRectF(3.5, 3.5, 9, 9), 1.5, 1.5);

    p.setPen(Qt::NoPen);
    p.setBrush(color);

    constexpr qreal pw = 1.5, ph = 1.5;
    // Top
    p.drawRect(QRectF(5.5, 1,   pw, ph));
    p.drawRect(QRectF(9,   1,   pw, ph));
    // Bottom
    p.drawRect(QRectF(5.5, 13.5, pw, ph));
    p.drawRect(QRectF(9,   13.5, pw, ph));
    // Left
    p.drawRect(QRectF(1,   5.5,  ph, pw));
    p.drawRect(QRectF(1,   9,    ph, pw));
    // Right
    p.drawRect(QRectF(13.5, 5.5,  ph, pw));
    p.drawRect(QRectF(13.5, 9,    ph, pw));

    return QIcon(pm);
}

// ── Construction ──────────────────────────────────────────────────────────────

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setWindowTitle(APP_NAME);
    setWindowFlags(windowFlags() & ~Qt::WindowMaximizeButtonHint);

    buildShell();

    auto *utils = ScxUtils::get();
    auto  conn  = std::make_shared<QMetaObject::Connection>();
    *conn = connect(utils, &ScxUtils::kernelChecked, this,
        [this, conn](bool ok, const QString &detail) {
            disconnect(*conn);
            onKernelResult(ok, detail);
        });
    utils->checkKernel();
}

// ── Shell ─────────────────────────────────────────────────────────────────────

void MainWindow::buildShell() {
    auto *cw   = new QWidget;
    auto *root = new QVBoxLayout(cw);
    root->setContentsMargins(12, 10, 12, 10);
    root->setSpacing(0);
    setCentralWidget(cw);

    // Status card
    auto *card = new QFrame;
    card->setObjectName("statusCard");
    card->setStyleSheet(
        "#statusCard { border: 1px solid #333;"
        "  border-radius: 6px; padding: 0px; }");
    auto *cl = new QHBoxLayout(card);
    cl->setContentsMargins(12, 10, 12, 10);
    cl->setSpacing(10);

    m_dot = new QLabel("●");
    QFont df = m_dot->font();
    df.setPointSize(20);
    m_dot->setFont(df);
    m_dot->setStyleSheet("color: #888;");
    cl->addWidget(m_dot);

    auto *statusCol = new QVBoxLayout;
    statusCol->setSpacing(1);
    auto *statusLabel = new QLabel("Status");
    QFont sf = statusLabel->font();
    sf.setPointSize(9);
    sf.setBold(true);
    statusLabel->setFont(sf);
    statusLabel->setStyleSheet("color: #999; text-transform: uppercase;");
    statusCol->addWidget(statusLabel);

    m_statusText = new QLabel("Please wait\u2026");
    QFont msf = m_statusText->font();
    msf.setPointSize(13);
    m_statusText->setFont(msf);
    statusCol->addWidget(m_statusText);
    cl->addLayout(statusCol);
    cl->addStretch();

    m_stopBtn = new QPushButton("Stop");
    m_stopBtn->setMinimumWidth(80);
    m_stopBtn->setMinimumHeight(30);
    m_stopBtn->setEnabled(false);
    m_stopBtn->setVisible(false);
    connect(m_stopBtn, &QPushButton::clicked, this, &MainWindow::onStopClicked);
    cl->addWidget(m_stopBtn);

    auto *quitBtn = new QPushButton("Quit");
    quitBtn->setMinimumWidth(80);
    quitBtn->setMinimumHeight(30);
    connect(quitBtn, &QPushButton::clicked, qApp, &QApplication::quit);
    cl->addWidget(quitBtn);

    root->addWidget(card);
    root->addSpacing(10);

    // Content stack: loading, normal, unsupported, setup
    m_contentStack = new QStackedWidget;
    root->addWidget(m_contentStack, 1);

    adjustSize();
    setMinimumWidth(640);
    setMinimumHeight(560);
}

// ── Post-kernel-check build ───────────────────────────────────────────────────

void MainWindow::onKernelResult(bool supported, const QString &detail) {
    m_kernelOk = supported;
    appendLog(detail);

    if (!supported) {
        updateStatusBar(false, {}, {});
        buildUnsupportedPage();
    } else if (!ScxUtils::scxctlPresent()) {
        buildSetupPage();
    } else {
        buildNormalMode();
    }

    adjustSize();
}

void MainWindow::buildNormalMode() {
    m_normalPage = new QWidget;
    auto *pageLayout = new QVBoxLayout(m_normalPage);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->setSpacing(10);

    // ── Control section ────────────────────────────────────────────────────────
    auto *ctrlBox = new QGroupBox("Switch Scheduler");
    ctrlBox->setMinimumHeight(180);
    auto *ctrlL   = new QVBoxLayout(ctrlBox);
    ctrlL->setContentsMargins(10, 10, 10, 10);

    m_ctrlTab = new ControlTab;
    connect(m_ctrlTab, &ControlTab::log,                this, &MainWindow::appendLog);
    connect(m_ctrlTab, &ControlTab::statusChanged,      this, &MainWindow::refreshStatus);
    connect(m_ctrlTab, &ControlTab::operationInProgress, this,
            [this](bool inFlight) {
                m_opInFlight = inFlight;
                m_stopBtn->setEnabled(m_schedActive && !m_opInFlight);
            });
    connect(m_ctrlTab, &ControlTab::schedulerSelected, this, &MainWindow::updateSchedInfo);
    ctrlL->addWidget(m_ctrlTab);
    pageLayout->addWidget(ctrlBox);

    // ── Scheduler info section ─────────────────────────────────────────────────
    auto *infoFrame = new QFrame;
    infoFrame->setObjectName("schedInfo");
    infoFrame->setStyleSheet(
        "#schedInfo { border: 1px solid #333;"
        "  border-radius: 5px; padding: 0px; }");
    auto *infoL = new QVBoxLayout(infoFrame);
    infoL->setContentsMargins(10, 8, 10, 8);
    infoL->setSpacing(3);

    auto *infoH = new QHBoxLayout;
    m_infoTitle = new QLabel;
    QFont inf = m_infoTitle->font();
    inf.setPointSize(12);
    inf.setBold(true);
    m_infoTitle->setFont(inf);
    infoH->addWidget(m_infoTitle);
    infoH->addStretch();

    m_infoCat = new QLabel;
    m_infoCat->setStyleSheet(
        "color: #88aaff; font-size: 10px; padding: 1px 6px;"
        "border: 1px solid #446; border-radius: 3px;");
    infoH->addWidget(m_infoCat);
    infoL->addLayout(infoH);

    m_infoDesc = new QLabel;
    m_infoDesc->setWordWrap(true);
    m_infoDesc->setStyleSheet("color: #bbb; font-size: 13px;");
    infoL->addWidget(m_infoDesc);

    m_infoModes = new QLabel;
    m_infoModes->setStyleSheet("color: #6a6; font-size: 10px;");
    infoL->addWidget(m_infoModes);

    pageLayout->addWidget(infoFrame);
    pageLayout->addStretch();

    // ── Collapsible log ────────────────────────────────────────────────────────
    auto *logSection = new QWidget;
    logSection->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    auto *logL = new QVBoxLayout(logSection);
    logL->setContentsMargins(0, 0, 0, 0);
    logL->setSpacing(2);

    auto *logHeader = new QHBoxLayout;
    m_logToggle = new QToolButton;
    m_logToggle->setArrowType(Qt::DownArrow);
    m_logToggle->setStyleSheet("QToolButton { border: none; padding: 2px; }");
    m_logToggle->setCheckable(true);
    m_logToggle->setChecked(true);
    connect(m_logToggle, &QToolButton::toggled, this, &MainWindow::toggleLog);
    logHeader->addWidget(m_logToggle);

    auto *logLabel = new QLabel("Logs");
    QFont logf = logLabel->font();
    logf.setPointSize(9);
    logf.setBold(true);
    logLabel->setFont(logf);
    logHeader->addWidget(logLabel);
    logHeader->addStretch();
    logL->addLayout(logHeader);

    m_log = new QTextEdit;
    m_log->setReadOnly(true);
    m_log->setMaximumHeight(110);
    QFont lf2("monospace", 9);
    m_log->setFont(lf2);
    logL->addWidget(m_log);

    pageLayout->addWidget(logSection);

    // Add normal page to stack and show it
    m_contentStack->addWidget(m_normalPage);
    m_contentStack->setCurrentWidget(m_normalPage);

    // ── System tray ────────────────────────────────────────────────────────────
    if (QSystemTrayIcon::isSystemTrayAvailable()) {
        m_tray     = new QSystemTrayIcon(trayIcon(QColor("#888888")), this);
        m_trayMenu = new QMenu;
        m_trayMenu->addAction("Show", this, [this] { show(); raise(); activateWindow(); });
        m_trayMenu->addSeparator();
        m_trayMenu->addAction("Quit", this, [] { QApplication::quit(); });
        m_tray->setContextMenu(m_trayMenu);
        m_tray->setToolTip(APP_NAME);
        m_tray->show();
        connect(m_tray, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason r) {
                if (r == QSystemTrayIcon::DoubleClick)
                    { show(); raise(); activateWindow(); }
            });
    }

    appendLog(QString("%1 v%2 ready").arg(APP_NAME, APP_VERSION));

    // Poll for schedulers list (loads into combo, triggers info update on selection)
    auto *utils = ScxUtils::get();
    auto  conn  = std::make_shared<QMetaObject::Connection>();
    *conn = connect(utils, &ScxUtils::schedulersListed, this,
        [this, conn](const QStringList &) {
            disconnect(*conn);
        });
    utils->listSchedulers();

    // Persistent connection for status updates
    m_statusConn = connect(utils, &ScxUtils::statusReady, this,
        [this](const SchedStatus &s) {
            updateStatusBar(s.active, s.name, s.mode);
        });

    m_pollTimer = new QTimer(this);
    m_pollTimer->setInterval(STATUS_POLL_INTERVAL_MS);
    connect(m_pollTimer, &QTimer::timeout, this, &MainWindow::refreshStatus);
    refreshStatus();
    m_pollTimer->start();
}

// ── Status ────────────────────────────────────────────────────────────────────

void MainWindow::refreshStatus() {
    ScxUtils::get()->queryStatus();
}

void MainWindow::updateStatusBar(bool active, const QString &name, const QString &mode) {
    m_schedActive = active;
    if (active) {
        m_dot->setStyleSheet("color: #00cc00;");
        m_statusText->setText(QString("%1 (%2)")
                              .arg(humanizeSched(name), humanizeMode(mode)));
        m_stopBtn->setEnabled(!m_opInFlight);
        m_stopBtn->setVisible(true);
        setTray(true, name);
    } else {
        m_dot->setStyleSheet("color: #cc0000;");
        m_statusText->setText("EEVDF (default)");
        m_stopBtn->setEnabled(false);
        m_stopBtn->setVisible(false);
        setTray(false);
    }
}

void MainWindow::setTray(bool active, const QString &schedName) {
    if (!m_tray) return;
    if (active) {
        m_tray->setIcon(trayIcon(QColor("#00cc00")));
        m_tray->setToolTip(QString("SCX: %1").arg(humanizeSched(schedName)));
    } else {
        m_tray->setIcon(trayIcon(QColor("#cc0000")));
        m_tray->setToolTip("SCX: none (EEVDF)");
    }
}

// ── Scheduler info ────────────────────────────────────────────────────────────

void MainWindow::updateSchedInfo(const QString &bare) {
    for (const auto &si : ALL_SCHEDULERS) {
        if (si.bare != bare) continue;
        m_infoTitle->setText(si.display);
        m_infoCat->setText(si.category);
        m_infoDesc->setText(si.desc);
        QStringList modeLabels;
        for (const QString &m : si.modes) modeLabels << humanizeMode(m);
        m_infoModes->setText(QString("Modes: %1").arg(modeLabels.join(", ")));
        return;
    }
    m_infoTitle->clear();
    m_infoCat->clear();
    m_infoDesc->clear();
    m_infoModes->clear();
}

// ── Error pages ───────────────────────────────────────────────────────────────

void MainWindow::buildUnsupportedPage() {
    auto *page = new QWidget;
    auto *l    = new QVBoxLayout(page);
    l->setAlignment(Qt::AlignCenter);
    l->setSpacing(12);

    auto *icon = new QLabel("\u26a0\ufe0f");
    QFont if2 = icon->font();
    if2.setPointSize(32);
    icon->setFont(if2);
    icon->setAlignment(Qt::AlignCenter);
    l->addWidget(icon);

    auto *title = new QLabel("Kernel does not support sched_ext");
    QFont tf = title->font();
    tf.setPointSize(13);
    tf.setBold(true);
    title->setFont(tf);
    title->setAlignment(Qt::AlignCenter);
    l->addWidget(title);

    auto *sub = new QLabel("A Linux 6.12+ kernel with CONFIG_SCHED_CLASS_EXT is required.");
    sub->setAlignment(Qt::AlignCenter);
    sub->setStyleSheet("color: #999;");
    l->addWidget(sub);

    auto *hint = new QLabel("Install a supported kernel on Debian 13 Trixie:");
    hint->setAlignment(Qt::AlignCenter);
    hint->setStyleSheet("color: #ccc; margin-top: 8px;");
    l->addWidget(hint);

    const QString cmd = "sudo apt install linux-image-amd64 linux-headers-amd64";
    auto *cmdEdit = new QLineEdit(cmd);
    cmdEdit->setReadOnly(true);
    cmdEdit->setAlignment(Qt::AlignCenter);
    cmdEdit->setStyleSheet(
        "QLineEdit { background: #2a2a2a; color: #88dd88;"
        "border: 1px solid #555; border-radius: 4px;"
        "padding: 6px 10px; font-family: monospace; font-size: 11px; }");
    QFontMetrics fm(cmdEdit->font());
    cmdEdit->setMinimumWidth(fm.horizontalAdvance(cmd) + 32);
    l->addWidget(cmdEdit);

    auto *note = new QLabel("Reboot after installing to activate the new kernel.");
    note->setAlignment(Qt::AlignCenter);
    note->setStyleSheet("color: #666; font-size: 10px; margin-top: 4px;");
    l->addWidget(note);

    m_contentStack->addWidget(page);
    m_contentStack->setCurrentWidget(page);
}

void MainWindow::buildSetupPage() {
    auto *page = new QWidget;
    auto *l    = new QVBoxLayout(page);
    l->setAlignment(Qt::AlignCenter);
    l->setSpacing(12);

    auto *icon = new QLabel("⚠️");
    QFont if2 = icon->font();
    if2.setPointSize(32);
    icon->setFont(if2);
    icon->setAlignment(Qt::AlignCenter);
    l->addWidget(icon);

    auto *title = new QLabel("scxctl not found");
    QFont tf = title->font();
    tf.setPointSize(13);
    tf.setBold(true);
    title->setFont(tf);
    title->setAlignment(Qt::AlignCenter);
    l->addWidget(title);

    auto *hint = new QLabel(
        "Install <b>scx-scheds</b> and <b>scx-tools</b> from the Debian repositories:");
    hint->setAlignment(Qt::AlignCenter);
    hint->setStyleSheet("color: #ccc;");
    l->addWidget(hint);

    const QString cmd = "sudo apt install scx-scheds scx-tools";
    auto *cmdEdit = new QLineEdit(cmd);
    cmdEdit->setReadOnly(true);
    cmdEdit->setAlignment(Qt::AlignCenter);
    cmdEdit->setStyleSheet(
        "QLineEdit { background: #2a2a2a; color: #88dd88;"
        "border: 1px solid #555; border-radius: 4px;"
        "padding: 6px 10px; font-family: monospace; font-size: 11px; }");
    QFontMetrics fm(cmdEdit->font());
    cmdEdit->setMinimumWidth(fm.horizontalAdvance(cmd) + 32);
    l->addWidget(cmdEdit);

    auto *btn = new QPushButton("Exit");
    btn->setMaximumWidth(90);
    connect(btn, &QPushButton::clicked, qApp, &QApplication::quit);
    auto *br = new QHBoxLayout;
    br->setAlignment(Qt::AlignCenter);
    br->addWidget(btn);
    l->addLayout(br);

    m_contentStack->addWidget(page);
    m_contentStack->setCurrentWidget(page);
}

// ── Log ───────────────────────────────────────────────────────────────────────

void MainWindow::toggleLog() {
    m_logVisible = m_logToggle->isChecked();
    m_log->setVisible(m_logVisible);
    m_logToggle->setArrowType(m_logVisible ? Qt::DownArrow : Qt::RightArrow);
}

void MainWindow::appendLog(const QString &msg) {
    if (!m_log) return;
    const QString ts = QDateTime::currentDateTime().toString("hh:mm:ss");
    m_log->append(QString("[%1] %2").arg(ts, msg));
    m_log->verticalScrollBar()->setValue(m_log->verticalScrollBar()->maximum());
}

void MainWindow::onStopClicked() {
    if (m_ctrlTab) m_ctrlTab->stopScheduler();
}

// ── Window close ──────────────────────────────────────────────────────────────

void MainWindow::closeEvent(QCloseEvent *event) {
    if (m_tray && m_tray->isVisible() && QSystemTrayIcon::isSystemTrayAvailable()) {
        hide();
        event->ignore();
    } else {
        if (m_pollTimer) m_pollTimer->stop();
        event->accept();
    }
}
