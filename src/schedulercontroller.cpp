#include "schedulercontroller.h"

#include "appcontroller.h"
#include "privops.h"
#include "scxutils.h"

#include <memory>

static const char *ERR_NO_POLKIT = "pkexec was not found.\n\n"
                                    "Install it with:\n  sudo apt install polkitd pkexec\n\n"
                                    "Then log out and back in so a PolKit agent starts.";

static const char *ERR_SWITCH_FAILED = "Switching the scheduler failed.\n\n"
                                       "Common causes:\n"
                                       "  - No PolKit authentication agent is running\n"
                                       "  - scx_loader.service is not active\n\n"
                                       "Try:  systemctl status scx_loader.service";

static const char *ERR_STOP_FAILED = "Stopping the scheduler failed.\n\n"
                                     "Try:  pkexec scxctl stop";

static const char *ERR_PERSIST_ENABLE = "Could not enable auto-start.\n\n"
                                        "Try:  sudo systemctl enable --now scx_loader.service";

static const char *ERR_PERSIST_DISABLE = "Could not disable auto-start.\n\n"
                                         "Try:  sudo systemctl disable --now scx_loader.service";

SchedulerController::SchedulerController(QObject *parent) : QObject(parent) {}

void SchedulerController::setEnabled(bool enabled) { emit operationInProgress(!enabled); }

void SchedulerController::start(const QString &sched, const QString &mode) {
    if (sched.isEmpty())
        return;

    if (!PrivOps::pkexecPresent()) {
        emit log(ERR_NO_POLKIT);
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
                setEnabled(true);
                return;
            }

            auto onDone = [this, sched, mode](bool ok, const QString &msg) {
                if (ok) {
                    ScxUtils::saveState(sched, mode);
                    auto *app = AppController::get();
                    emit log(QString("Now running %1 (%2)")
                                 .arg(app->humanize(sched), app->humanizeMode(mode)));
                } else {
                    emit log(msg.isEmpty() ? ERR_SWITCH_FAILED
                                           : QString("Failed: %1").arg(msg));
                }
                setEnabled(true);
                emit statusChanged();
            };

            auto *app = AppController::get();
            if (!cur.active) {
                emit log(QString("Starting %1 (%2)\xe2\x80\xa6")
                             .arg(app->humanize(sched), app->humanizeMode(mode)));
                PrivOps::get()->startScheduler(sched, mode, onDone);
            } else {
                emit log(QString("Switching to %1 (%2)\xe2\x80\xa6")
                             .arg(app->humanize(sched), app->humanizeMode(mode)));
                PrivOps::get()->switchScheduler(sched, mode, onDone);
            }
        });

    utils->queryStatus();
}

void SchedulerController::stop() {
    if (!PrivOps::pkexecPresent()) {
        emit log(ERR_NO_POLKIT);
        return;
    }

    emit log("Stopping scheduler\xe2\x80\xa6");
    setEnabled(false);

    PrivOps::get()->stopScheduler([this](bool ok, const QString &msg) {
        emit log(ok ? "Stopped \xe2\x80\x94 back to EEVDF"
                    : (msg.isEmpty() ? ERR_STOP_FAILED : QString("Failed: %1").arg(msg)));
        setEnabled(true);
        emit statusChanged();
    });
}

void SchedulerController::setPersist(bool enabled) {
    if (!PrivOps::pkexecPresent()) {
        emit log(ERR_NO_POLKIT);
        emit persistToggled(!enabled);
        return;
    }

    setEnabled(false);

    auto onDone = [this, enabled](bool ok, const QString &msg) {
        if (ok) {
            emit log(enabled ? "Auto-start enabled \xe2\x80\x94 scheduler will apply at next boot"
                             : "Auto-start disabled");
        } else {
            const char *canned = enabled ? ERR_PERSIST_ENABLE : ERR_PERSIST_DISABLE;
            emit log(msg.isEmpty() ? canned : QString("Failed: %1").arg(msg));
            emit persistToggled(!enabled);
        }
        setEnabled(true);
    };

    if (enabled) {
        emit log("Enabling auto-start\xe2\x80\xa6");
        PrivOps::get()->enableService(onDone);
    } else {
        emit log("Disabling auto-start\xe2\x80\xa6");
        PrivOps::get()->disableService(onDone);
    }
}
