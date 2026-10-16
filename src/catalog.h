#pragma once

#include <QString>

// Descriptions of sched_ext components, summarised from the READMEs in
// github.com/sched-ext/scx (v1.1.4). What is installed and which modes each
// scheduler has always come from the system, not from here.
namespace catalog {

enum class Maturity { Unstated, Production, Experimental, NotForProduction };

struct Component {
    QString binary;  // e.g. "scx_bpfland", "scxctl"
    QString display; // e.g. "BPFland"
    bool scheduler;
    QString tag; // short focus, e.g. "Gaming"
    Maturity maturity;
    QString summary;
    QString bestFor;
};

const Component *find(const QString &binary);
const Component *scheduler(const QString &name); // bare name, e.g. "bpfland"

QString displayName(const QString &name); // bare scheduler name
QString modeLabel(const QString &mode);
QString maturityLabel(Maturity maturity);

} // namespace catalog
