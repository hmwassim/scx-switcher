#include "appcontroller.h"

#include "schedcatalog.h"
#include "scxutils.h"

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
        emit statusChanged(s.active, s.name, s.mode);
    });
}

void AppController::startPolling() {
    refreshStatus();
    m_pollTimer->start();
}

void AppController::stopPolling() { m_pollTimer->stop(); }

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
