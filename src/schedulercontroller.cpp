#include "schedulercontroller.h"

#include "appcontroller.h"
#include "distro.h"
#include "privops.h"
#include "schedcatalog.h"
#include "scxutils.h"

#include <memory>

static const char *ERR_SWITCH_FAILED = "Switching the scheduler failed.\n\n"
                                       "Common causes:\n"
                                       "  - No PolKit authentication agent is running\n"
                                       "  - scx_loader.service is not active\n\n"
                                       "Try:  systemctl status scx_loader.service";

static QString errNoEscalation() {
    return "No privilege escalation tool (pkexec or sudo) was found.\n\n" +
           DistroInfo::get()->escalationHint();
}

static QString errStopFailed() {
    const QString tool = PrivOps::escalationTool();
    return tool.isEmpty()
               ? QStringLiteral("Stopping the scheduler failed.")
               : QStringLiteral("Stopping the scheduler failed.\n\nTry:  %1 scxctl stop")
                     .arg(tool);
}

static const char *ERR_PERSIST = "The scheduler switched, but its boot configuration could "
                                 "not be saved.\n\n"
                                 "Run as root:  systemctl enable --now scx_loader.service";

SchedulerController::SchedulerController(QObject *parent) : QObject(parent) {}

void SchedulerController::setEnabled(bool enabled) { emit operationInProgress(!enabled); }

void SchedulerController::start(const QString &sched, const QString &mode) {
    if (sched.isEmpty())
        return;

    if (!PrivOps::escalationAvailable()) {
        emit log(errNoEscalation());
        return;
    }

    setEnabled(false);

    auto *utils = ScxUtils::get();
    auto conn = std::make_shared<QMetaObject::Connection>();

    *conn = connect(
        utils, &ScxUtils::statusReady, this, [this, conn, sched, mode](const SchedStatus &cur) {
            disconnect(*conn);

            if (cur.active && cur.name == sched && cur.mode == mode) {
                ScxUtils::saveState(sched, mode);
                auto *app = AppController::get();
                emit log(QString("%1 (%2) is already running")
                             .arg(app->humanize(sched), app->humanizeMode(mode)));
                persist(sched, mode);
                return;
            }

            auto onDone = [this, sched, mode](bool ok, const QString &msg) {
                if (ok) {
                    ScxUtils::saveState(sched, mode);
                    auto *app = AppController::get();
                    emit log(QString("Now running %1 (%2)")
                                 .arg(app->humanize(sched), app->humanizeMode(mode)));
                    persist(sched, mode);
                    return;
                }
                emit log(msg.isEmpty() ? ERR_SWITCH_FAILED
                                       : QString("Failed: %1").arg(msg));
                setEnabled(true);
                emit statusChanged();
            };

            auto *app = AppController::get();
            const QString toml = SchedCatalog::get()->tomlMode(mode);
            if (!cur.active) {
                emit log(QString("Starting %1 (%2)\xe2\x80\xa6")
                             .arg(app->humanize(sched), app->humanizeMode(mode)));
                PrivOps::get()->runCtl({"start", sched, mode, toml}, onDone);
            } else {
                emit log(QString("Switching to %1 (%2)\xe2\x80\xa6")
                             .arg(app->humanize(sched), app->humanizeMode(mode)));
                PrivOps::get()->runCtl({"switch", sched, mode, toml}, onDone);
            }
        });

    utils->queryStatus();
}

void SchedulerController::stop() {
    if (!PrivOps::escalationAvailable()) {
        emit log(errNoEscalation());
        return;
    }

    emit log("Stopping scheduler\xe2\x80\xa6");
    setEnabled(false);

    PrivOps::get()->runCtl({"stop"}, [this](bool ok, const QString &msg) {
        emit log(ok ? "Stopped \xe2\x80\x94 back to EEVDF"
                    : (msg.isEmpty() ? errStopFailed() : QString("Failed: %1").arg(msg)));
        setEnabled(true);
        emit statusChanged();
    });
}

void SchedulerController::persist(const QString &sched, const QString &mode) {
    PrivOps::get()->runCtl({"persist", sched, SchedCatalog::get()->tomlMode(mode)},
                           [this](bool ok, const QString &msg) {
                               if (!ok)
                                   emit log(msg.isEmpty() ? ERR_PERSIST
                                                          : QString("Failed: %1").arg(msg));
                               setEnabled(true);
                               emit statusChanged();
                           });
}
