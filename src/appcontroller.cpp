#include "appcontroller.h"

#include "schedcatalog.h"
#include "scxutils.h"

#include <QApplication>
#include <QDateTime>
#include <QMenu>
#include <QPainter>
#include <QPixmap>
#include <QSystemTrayIcon>

static QIcon trayIcon(const QColor &color) {
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
    p.drawRect(QRectF(5.5, 1, pw, ph));
    p.drawRect(QRectF(9, 1, pw, ph));
    p.drawRect(QRectF(5.5, 13.5, pw, ph));
    p.drawRect(QRectF(9, 13.5, pw, ph));
    p.drawRect(QRectF(1, 5.5, ph, pw));
    p.drawRect(QRectF(1, 9, ph, pw));
    p.drawRect(QRectF(13.5, 5.5, ph, pw));
    p.drawRect(QRectF(13.5, 9, ph, pw));

    return QIcon(pm);
}

AppController *AppController::get() {
    static AppController *inst = new AppController;
    return inst;
}

AppController::AppController(QObject *parent) : QObject(parent) {
    m_pollTimer = new QTimer(this);
    m_pollTimer->setInterval(POLL_INTERVAL_MS);
    connect(m_pollTimer, &QTimer::timeout, this, &AppController::refreshStatus);

    auto *utils = ScxUtils::get();
    connect(utils, &ScxUtils::statusReady, this, [this](const SchedStatus &s) {
        updateTray(s.active, s.name);
        emit statusChanged(s.active, s.name, s.mode);
    });

    initTray();
}

void AppController::start() {
    refreshStatus();
    m_pollTimer->start();
}

void AppController::stop() { m_pollTimer->stop(); }

void AppController::refreshStatus() { ScxUtils::get()->queryStatus(); }

SchedInfo AppController::schedulerInfo(const QString &bare) const {
    return SchedCatalog::get()->find(bare);
}

QString AppController::humanize(const QString &bare) const {
    return SchedCatalog::get()->humanize(bare);
}

QString AppController::humanizeMode(const QString &mode) const {
    return SchedCatalog::get()->humanizeMode(mode);
}

void AppController::appendLog(const QString &msg) {
    const QString ts = QDateTime::currentDateTime().toString("hh:mm:ss");
    emit logMessage(QString("[%1] %2").arg(ts, msg));
}

// ── Tray ──────────────────────────────────────────────────────────────────────

void AppController::initTray() {
    if (!QSystemTrayIcon::isSystemTrayAvailable())
        return;

    m_tray = new QSystemTrayIcon(trayIcon(QColor("#888888")), this);
    m_trayMenu = new QMenu;
    m_trayMenu->addAction("Show", this, [this] { emit showRequested(); });
    m_trayMenu->addSeparator();
    m_trayMenu->addAction("Quit", this, [] { QApplication::quit(); });
    m_tray->setContextMenu(m_trayMenu);
    m_tray->setToolTip("SCX Switcher");
    m_tray->show();

    connect(m_tray, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason r) {
                if (r == QSystemTrayIcon::DoubleClick)
                    emit showRequested();
            });
}

void AppController::updateTray(bool active, const QString &schedName) {
    if (!m_tray)
        return;
    if (active) {
        m_tray->setIcon(trayIcon(QColor("#00cc00")));
        m_tray->setToolTip(QString("SCX: %1").arg(humanize(schedName)));
    } else {
        m_tray->setIcon(trayIcon(QColor("#cc0000")));
        m_tray->setToolTip("SCX: none (EEVDF)");
    }
}
