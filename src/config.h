#pragma once

#include <QStringList>

struct SchedInfo {
    QString bare;
    QString display;
    QString category;
    QString desc;
    QStringList modes;
};
