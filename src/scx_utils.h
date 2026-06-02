#pragma once

#include <QString>
#include <QStringList>
#include <QPair>

struct SchedStatus {
    bool active = false;
    QString name;
    QString mode;
};

namespace scx_utils {

bool isScxctlInstalled();
bool isToolInstalled(const QString &name);

std::pair<bool, QString> checkKernelSupport();

SchedStatus getSchedulerStatus();
QStringList listSchedulers();
bool isServiceEnabled();

QStringList supportedModes(const QString &schedBareName);

void saveState(const QString &sched, const QString &mode);
QPair<QString, QString> loadState();

QString humanizeScheduler(const QString &name);
QString humanizeMode(const QString &mode);

} // namespace scx_utils
