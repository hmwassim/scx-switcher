#include "mainwindow.h"
#include "appcontroller.h"
#include "config.h"
#include "controltab.h"
#include "privops.h"
#include "scxutils.h"

#include <QApplication>
#include <QFont>
#include <QFontMetrics>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMenu>
#include <QScrollArea>
#include <QScrollBar>
#include <QStackedWidget>
#include <QSystemTrayIcon>
#include <QToolButton>
#include <QVBoxLayout>
#include <memory>

// ── Construction ──────────────────────────────────────────────────────────────

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setWindowTitle(APP_NAME);
    setWindowFlags(windowFlags() & ~Qt::WindowMaximizeButtonHint);

    m_app = AppController::get();

    buildShell();

    auto *utils = ScxUtils::get();
    auto conn = std::make_shared<QMetaObject::Connection>();
    *conn = connect(utils, &ScxUtils::kernelChecked, this,
                    [this, conn](bool ok, const QString &detail) {
                        disconnect(*conn);
                        onKernelResult(ok, detail);
                    });
    utils->checkKernel();
}

// ── Shell ─────────────────────────────────────────────────────────────────────

void MainWindow::buildShell() {
    auto *cw = new QWidget;
    auto *root = new QVBoxLayout(cw);
    root->setContentsMargins(12, 10, 12, 10);
    root->setSpacing(0);
    setCentralWidget(cw);

    auto *card = new QFrame;
    card->setObjectName("statusCard");
    card->setStyleSheet("#statusCard { border: 1px solid #333;"
                        "  border-radius: 6px; padding: 0px; }");
    auto *cl = new QHBoxLayout(card);
    cl->setContentsMargins(12, 10, 12, 10);
    cl->setSpacing(10);

    m_dot = new QLabel("\u25cf");
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

    m_contentStack = new QStackedWidget;
    root->addWidget(m_contentStack, 1);

    adjustSize();
    setMinimumWidth(640);
    setMinimumHeight(560);
}

// ── Post-kernel-check build ───────────────────────────────────────────────────

void MainWindow::onKernelResult(bool supported, const QString &detail) {
    m_kernelOk = supported;
    m_app->appendLog(detail);

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
    auto *ctrlL = new QVBoxLayout(ctrlBox);
    ctrlL->setContentsMargins(10, 10, 10, 10);

    m_ctrlTab = new ControlTab;
    connect(m_ctrlTab, &ControlTab::log, m_app, &AppController::appendLog);
    connect(m_ctrlTab, &ControlTab::statusChanged, m_app, &AppController::refreshStatus);
    connect(m_ctrlTab, &ControlTab::operationInProgress, this, [this](bool inFlight) {
        m_opInFlight = inFlight;
        m_stopBtn->setEnabled(m_schedActive && !m_opInFlight);
    });
    connect(m_ctrlTab, &ControlTab::schedulerSelected, this, &MainWindow::updateSchedInfo);
    ctrlL->addWidget(m_ctrlTab);
    pageLayout->addWidget(ctrlBox);

    // ── Scheduler info section ─────────────────────────────────────────────────
    auto *infoFrame = new QFrame;
    infoFrame->setObjectName("schedInfo");
    infoFrame->setStyleSheet("#schedInfo { border: 1px solid #333;"
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
    m_infoCat->setStyleSheet("color: #88aaff; font-size: 10px; padding: 1px 6px;"
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
    m_log->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_log, &QWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        auto *menu = m_log->createStandardContextMenu();
        menu->addSeparator();
        auto *clearAct = menu->addAction("Clear");
        clearAct->setEnabled(!m_log->document()->isEmpty());
        connect(clearAct, &QAction::triggered, m_log, &QTextEdit::clear);
        menu->popup(m_log->viewport()->mapToGlobal(pos));
        connect(menu, &QMenu::aboutToHide, menu, &QObject::deleteLater);
    });
    logL->addWidget(m_log);

    pageLayout->addWidget(logSection);

    m_contentStack->addWidget(m_normalPage);
    m_contentStack->setCurrentWidget(m_normalPage);

    // ── Log forwarding ─────────────────────────────────────────────────────────
    connect(m_app, &AppController::logMessage, this, [this](const QString &msg) {
        if (!m_log)
            return;
        m_log->append(msg);
        m_log->verticalScrollBar()->setValue(m_log->verticalScrollBar()->maximum());
    });

    // ── Tray show requests ─────────────────────────────────────────────────────
    connect(m_app, &AppController::showRequested, this, [this] {
        show();
        raise();
        activateWindow();
    });

    m_app->appendLog(QString("%1 v%2 ready").arg(APP_NAME, APP_VERSION));

    const QString pathWarn = PrivOps::checkPolicyPath();
    if (!pathWarn.isEmpty())
        m_app->appendLog(pathWarn);

    // Connect AppController status updates to UI
    connect(m_app, &AppController::statusChanged, this, &MainWindow::updateStatusBar);

    // Trigger initial scheduler list load
    auto *utils = ScxUtils::get();
    auto conn = std::make_shared<QMetaObject::Connection>();
    *conn = connect(utils, &ScxUtils::schedulersListed, this,
                    [this, conn](const QStringList &) { disconnect(*conn); });
    utils->listSchedulers();

    // Start status polling via AppController
    m_app->start();
}

// ── Status ────────────────────────────────────────────────────────────────────

void MainWindow::updateStatusBar(bool active, const QString &name, const QString &mode) {
    m_schedActive = active;
    if (active) {
        m_dot->setStyleSheet("color: #00cc00;");
        m_statusText->setText(QString("%1 (%2)")
                                  .arg(m_app->humanize(name), m_app->humanizeMode(mode)));
        m_stopBtn->setEnabled(!m_opInFlight);
        m_stopBtn->setVisible(true);
    } else {
        m_dot->setStyleSheet("color: #cc0000;");
        m_statusText->setText("EEVDF (default)");
        m_stopBtn->setEnabled(false);
        m_stopBtn->setVisible(false);
    }
}

// ── Scheduler info ────────────────────────────────────────────────────────────

void MainWindow::updateSchedInfo(const QString &bare) {
    const auto si = m_app->schedulerInfo(bare);
    if (si.bare.isEmpty()) {
        m_infoTitle->clear();
        m_infoCat->clear();
        m_infoDesc->clear();
        m_infoModes->clear();
        return;
    }
    m_infoTitle->setText(si.display);
    m_infoCat->setText(si.category);
    m_infoDesc->setText(si.desc);
    QStringList modeLabels;
    for (const QString &m : si.modes)
        modeLabels << m_app->humanizeMode(m);
    m_infoModes->setText(QString("Modes: %1").arg(modeLabels.join(", ")));
}

// ── Error pages ───────────────────────────────────────────────────────────────

void MainWindow::buildUnsupportedPage() {
    auto *page = new QWidget;
    auto *l = new QVBoxLayout(page);
    l->setAlignment(Qt::AlignCenter);

    auto *frame = new QFrame;
    frame->setObjectName("errorCard");
    frame->setStyleSheet("#errorCard { border: 1px solid #333;"
                         "  border-radius: 6px; padding: 16px; }");
    auto *fl = new QVBoxLayout(frame);
    fl->setSpacing(10);

    auto *icon = new QLabel("\u26a0\ufe0f");
    QFont if2 = icon->font();
    if2.setPointSize(28);
    icon->setFont(if2);
    icon->setAlignment(Qt::AlignCenter);
    fl->addWidget(icon);

    auto *title = new QLabel("Kernel does not support sched_ext");
    QFont tf = title->font();
    tf.setPointSize(13);
    tf.setBold(true);
    title->setFont(tf);
    title->setAlignment(Qt::AlignCenter);
    fl->addWidget(title);

    auto *sub = new QLabel("A Linux 6.12+ kernel with CONFIG_SCHED_CLASS_EXT is required.");
    sub->setAlignment(Qt::AlignCenter);
    sub->setStyleSheet("color: #999;");
    fl->addWidget(sub);

    auto *hint = new QLabel("Install a supported kernel on Debian 13 Trixie:");
    hint->setAlignment(Qt::AlignCenter);
    hint->setStyleSheet("color: #ccc;");
    fl->addWidget(hint);

    const QString cmd =
        "sudo apt install -t trixie-backports linux-image-amd64 linux-headers-amd64";
    auto *cmdEdit = new QLineEdit(cmd);
    cmdEdit->setReadOnly(true);
    cmdEdit->setAlignment(Qt::AlignCenter);
    cmdEdit->setStyleSheet("QLineEdit { background: #2a2a2a; color: #88dd88;"
                           "border: 1px solid #555; border-radius: 4px;"
                           "padding: 6px 10px; font-family: monospace; font-size: 11px; }");
    QFontMetrics fm(cmdEdit->font());
    cmdEdit->setMinimumWidth(fm.horizontalAdvance(cmd) + 32);
    fl->addWidget(cmdEdit);

    auto *note = new QLabel("Reboot after installing to activate the new kernel.");
    note->setAlignment(Qt::AlignCenter);
    note->setStyleSheet("color: #666; font-size: 10px;");
    fl->addWidget(note);

    l->addWidget(frame);
    m_contentStack->addWidget(page);
    m_contentStack->setCurrentWidget(page);
}

void MainWindow::buildSetupPage() {
    auto *page = new QWidget;
    auto *l = new QVBoxLayout(page);
    l->setAlignment(Qt::AlignCenter);

    auto *frame = new QFrame;
    frame->setObjectName("errorCard");
    frame->setStyleSheet("#errorCard { border: 1px solid #333;"
                         "  border-radius: 6px; padding: 16px; }");
    auto *fl = new QVBoxLayout(frame);
    fl->setSpacing(10);

    auto *icon = new QLabel("\u26a0\ufe0f");
    QFont if2 = icon->font();
    if2.setPointSize(28);
    icon->setFont(if2);
    icon->setAlignment(Qt::AlignCenter);
    fl->addWidget(icon);

    auto *title = new QLabel("scxctl not found");
    QFont tf = title->font();
    tf.setPointSize(13);
    tf.setBold(true);
    title->setFont(tf);
    title->setAlignment(Qt::AlignCenter);
    fl->addWidget(title);

    auto *hint =
        new QLabel("Install <b>scx-scheds</b> and <b>scx-tools</b> from the Debian repositories:");
    hint->setAlignment(Qt::AlignCenter);
    hint->setStyleSheet("color: #ccc;");
    fl->addWidget(hint);

    const QString cmd = "sudo apt install scx-scheds scx-tools";
    auto *cmdEdit = new QLineEdit(cmd);
    cmdEdit->setReadOnly(true);
    cmdEdit->setAlignment(Qt::AlignCenter);
    cmdEdit->setStyleSheet("QLineEdit { background: #2a2a2a; color: #88dd88;"
                           "border: 1px solid #555; border-radius: 4px;"
                           "padding: 6px 10px; font-family: monospace; font-size: 11px; }");
    QFontMetrics fm(cmdEdit->font());
    cmdEdit->setMinimumWidth(fm.horizontalAdvance(cmd) + 32);
    fl->addWidget(cmdEdit);

    auto *btn = new QPushButton("Exit");
    btn->setMaximumWidth(90);
    connect(btn, &QPushButton::clicked, qApp, &QApplication::quit);
    auto *br = new QHBoxLayout;
    br->setAlignment(Qt::AlignCenter);
    br->addWidget(btn);
    fl->addLayout(br);

    l->addWidget(frame);
    m_contentStack->addWidget(page);
    m_contentStack->setCurrentWidget(page);
}

// ── Log ───────────────────────────────────────────────────────────────────────

void MainWindow::toggleLog() {
    m_logVisible = m_logToggle->isChecked();
    m_log->setVisible(m_logVisible);
    m_logToggle->setArrowType(m_logVisible ? Qt::DownArrow : Qt::RightArrow);
}

void MainWindow::onStopClicked() {
    if (m_ctrlTab)
        m_ctrlTab->stopScheduler();
}

// ── Window close ──────────────────────────────────────────────────────────────

void MainWindow::closeEvent(QCloseEvent *event) {
    if (QSystemTrayIcon::isSystemTrayAvailable()) {
        hide();
        event->ignore();
    } else {
        m_app->stop();
        event->accept();
    }
}
