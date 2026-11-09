#pragma once

#include <QList>
#include <QString>
#include <QStringList>

/*
 * Groups follow sched-ext/scx services/catalog.json. Descriptions come from that file where it
 * has one, otherwise from each scheduler's README. Mode options are scx_loader's own presets
 * (crates/scx_loader/src/config.rs). What is installed always comes from the system.
 */
namespace catalog {

enum class Group { Featured, Stable, Experimental, Other };

struct Scheduler {
   QString binary; // "scx_bpfland"
   QString name;   // "BPFland"
   QString focus;  // "Interactive"
   QString summary;
   QString bestFor;
   Group group = Group::Other;
};

/* Installed schedulers in list order, without the ones upstream hides */
QList<Scheduler> installed();

const Scheduler *find(const QString &binary);
QString name(const QString &binary);
QString groupLabel(Group group);

/* Modes: "auto", "gaming", "powersave", "lowlatency", "server", "custom" */
QStringList modes(const QString &binary);
QString modeLabel(const QString &mode);
QStringList options(const QString &binary, const QString &mode);
QString modeFor(const QString &binary, const QStringList &options);

/* Same rule the helper enforces */
bool validOptions(const QStringList &options);

} // namespace catalog
