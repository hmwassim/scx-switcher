#include "mainwindow.h"
#include "control_tab.h"
#include "scx_utils.h"
#include "config.h"

#include <QApplication>
#include <memory>
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
const QString MainWindow::APP_VERSION = APP_BINARY_VERSION;

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
    connect(m_controlTab, &ControlTab::logMessage, this, [this](const QString &msg) {
        log(msg);
    });
    connect(m_controlTab, &ControlTab::statusRefreshRequested, this, &MainWindow::refreshStatus);

    m_tabs->addTab(m_controlTab, "Control");

    m_tray = new QSystemTrayIcon(makeDotIcon(QColor("#888888")), this);
    auto *menu = new QMenu;
    menu->addAction("Show Window", this, [this]() { show(); raise(); activateWindow(); });
    menu->addSeparator();
    menu->addAction("Quit", this, [this]() {
        if (m_statusTimer) m_statusTimer->stop();
        QApplication::quit();
    });
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

    buildReferenceTab();

    m_statusTimer = new QTimer(this);
    m_statusTimer->setInterval(3000);
    connect(m_statusTimer, &QTimer::timeout, this, &MainWindow::refreshStatus);
    refreshStatus();
    m_statusTimer->start();
}

void MainWindow::buildReferenceTab() {
    auto *utils = scx_utils::instance();
    auto conn = std::make_shared<QMetaObject::Connection>();
    *conn = connect(utils, &ScxUtils::schedulersListed, this,
        [this, conn](const QStringList &installed) {
            disconnect(*conn);

            if (installed.isEmpty()) {
                log("No schedulers installed, skipping reference tab");
                return;
            }

            auto *refTab = new QWidget;
            auto *refLayout = new QVBoxLayout(refTab);
            auto *refTitle = new QLabel("Scheduler Reference");
            QFont rf = refTitle->font();
            rf.setPointSize(13);
            rf.setBold(true);
            refTitle->setFont(rf);
            refLayout->addWidget(refTitle);

            for (const auto &schedInfo : ALL_SCHEDULERS) {
                if (!installed.contains(schedInfo.bare))
                    continue;

                auto *card = new QGroupBox;
                auto *cl = new QVBoxLayout(card);
                cl->setContentsMargins(10, 6, 10, 6);

                auto *hl = new QHBoxLayout;
                auto *nl = new QLabel(schedInfo.display);
                QFont nf = nl->font();
                nf.setPointSize(12);
                nf.setBold(true);
                nl->setFont(nf);
                hl->addWidget(nl);
                hl->addWidget(new QLabel(QString("(scx_%1)").arg(schedInfo.bare)));
                hl->addStretch();
                auto *cl2 = new QLabel(schedInfo.category);
                cl2->setStyleSheet("color: #88aaff; font-size: 11px; padding: 2px 6px; "
                                   "border: 1px solid #6688cc; border-radius: 4px;");
                hl->addWidget(cl2);
                cl->addLayout(hl);

                auto *dl = new QLabel(schedInfo.desc);
                dl->setWordWrap(true);
                dl->setStyleSheet("color: #bbb; font-size: 12px;");
                cl->addWidget(dl);

                QStringList modeLabels;
                for (const auto &m : ScxUtils::supportedModes(schedInfo.bare))
                    modeLabels << ScxUtils::humanizeMode(m);
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

            m_tabs->addTab(scrollArea, "Reference");

            adjustSize();
            setFixedSize(size());
        });

    utils->listSchedulers();
}

void MainWindow::checkKernelAndBuildUi() {
    auto *utils = scx_utils::instance();
    auto conn = std::make_shared<QMetaObject::Connection>();
    *conn = connect(utils, &ScxUtils::kernelSupportChecked, this,
        [this, conn, utils](bool supported, const QString &msg) {
            disconnect(*conn);
            m_kernelChecked = true;

            if (!supported) {
                m_statusDot->setStyleSheet("color: #cc0000;");
                m_statusLabel->setText("Kernel unsupported \u2014 " + msg);
                m_stopBtn->setEnabled(false);
                m_tabs->addTab(buildUnsupportedPage(), "Unsupported Kernel");
            } else if (ScxUtils::isScxctlInstalled()) {
                buildNormalMode();
            } else {
                buildSetupMode();
                m_statusLabel->setText("scxctl not found");
            }

            adjustSize();
            setFixedSize(size());
        });
    utils->checkKernelSupport();
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
    if (m_refreshing) return;
    m_refreshing = true;

    auto *utils = scx_utils::instance();

    auto fetchStatus = [this, utils]() {
        auto conn = std::make_shared<QMetaObject::Connection>();
        *conn = connect(utils, &ScxUtils::schedulerStatusReady, this,
            [this, conn](const SchedStatus &s) {
                disconnect(*conn);
                m_refreshing = false;

                if (s.active) {
                    m_statusDot->setStyleSheet("color: #00cc00;");
                    QString schedName = ScxUtils::humanizeScheduler(s.name);
                    QString modeName = ScxUtils::humanizeMode(s.mode);
                    QString text = QString("Running: %1 (%2)").arg(schedName, modeName);

                    m_statusLabel->setText(text);
                    m_statusLabel->setToolTip(text);
                    m_stopBtn->setEnabled(true);
                    toggleTrayIcon(true, s.name);

                    if (text.length() > 50) {
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
            });
        utils->getSchedulerStatus();
    };

    if (!m_kernelChecked) {
        m_kernelChecked = true;
        auto conn = std::make_shared<QMetaObject::Connection>();
        *conn = connect(utils, &ScxUtils::kernelSupportChecked, this,
            [this, conn, fetchStatus](bool kernelOk, const QString &kernelMsg) {
                disconnect(*conn);

                if (!kernelOk) {
                    m_statusDot->setStyleSheet("color: #cc0000;");
                    m_statusLabel->setText("Kernel unsupported");
                    m_statusLabel->setToolTip(kernelMsg);
                    m_stopBtn->setEnabled(false);
                    toggleTrayIcon(false);
                    m_marqueeTimer->stop();
                    m_refreshing = false;
                    return;
                }

                fetchStatus();
            });
        utils->checkKernelSupport();
    } else {
        fetchStatus();
    }
}

void MainWindow::updateMarquee() {
    m_marqueeOffset += 2;
    if (m_marqueeOffset > m_marqueeText.length())
        m_marqueeOffset = 0;

    int splitPos = m_marqueeOffset - 4;
    if (splitPos < 0) splitPos = 0;

    m_statusLabel->setText(
        m_marqueeText.mid(splitPos) + "    " +
        m_marqueeText.left(splitPos));
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
