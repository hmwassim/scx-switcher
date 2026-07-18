#pragma once

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

struct SchedInfo {
    QString bare;
    QString display;
    QString category;
    QString desc;
    QStringList modes;
};

class SchedCatalog {
    public:
    static SchedCatalog *get();

    SchedInfo find(const QString &bare) const;
    QStringList modes(const QString &bare) const;
    QStringList allNames() const;
    QString humanize(const QString &bare) const;
    QString tomlMode(const QString &mode) const;
    QString humanizeMode(const QString &mode) const;

    private:
    SchedCatalog();
    const QList<SchedInfo> m_schedulers;
    QHash<QString, QString> m_displayOverrides;
};
