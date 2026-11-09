#include "catalog.h"

#include <QDir>
#include <QHash>
#include <QRegularExpression>
#include <QSet>

namespace catalog {

using G = Group;

static const QList<Scheduler> KNOWN = {
   {"scx_cosmos", "Cosmos", "General purpose", "Keeps the system responsive under load.",
    "Desktops and servers", G::Featured},
   {"scx_lavd", "LAVD", "Gaming", "Prioritizes tasks based on their latency criticality.",
    "Gaming and interactive apps", G::Featured},
   {"scx_eevdf", "EEVDF", "General purpose",
    "Brings newer kernel scheduler updates without requiring a kernel upgrade.",
    "Linux 7.2 or newer", G::Featured},
   {"scx_cake", "Cake", "Gaming", "Improves gaming responsiveness.", "Gaming desktops",
    G::Featured},
   {"scx_beerland", "Beerland", "Locality", "Prioritizes locality and scalability.",
    "Cache-heavy work, many CPUs", G::Stable},
   {"scx_bpfland", "BPFland", "Interactive", "Prioritizes interactive workloads.",
    "Gaming, streaming, audio", G::Stable},
   {"scx_flash", "Flash", "Low latency", "Fair and predictable, even under load.",
    "Multimedia and audio", G::Stable},
   {"scx_p2dq", "P2DQ", "General purpose", "Balances load across caches and NUMA nodes.",
    "Desktops and servers, with tuning", G::Stable},
   {"scx_pandemonium", "Pandemonium", "Adaptive", "Adapts every decision to a live latency bound.",
    "Low latency under heavy load", G::Stable},
   {"scx_rusty", "Rusty", "Load balancing", "Balances load between CPU domains.",
    "Multi-CCX machines, with tuning", G::Stable},
   {"scx_chaos", "Chaos", "Testing", "Adds delays on purpose to expose timing bugs.",
    "Testing only, never daily use", G::Experimental},
   {"scx_flow", "Flow", "Low latency", "Deadline scheduling with a burst prediction.",
    "Latency-sensitive apps", G::Experimental},
   {"scx_forge", "Forge", "AI research", "A base scheduler for AI-driven policy work.",
    "Developing policies", G::Experimental},
   {"scx_mavd", "MAVD", "Gaming", "LAVD on the new cid interface.", "Linux 7.2 or newer",
    G::Experimental},
   {"scx_mlfq", "MLFQ", "Desktop", "Three queues: interactive, normal, CPU-bound.",
    "Gaming and laptops, Linux 7.1 or newer", G::Experimental},
   {"scx_rlfifo", "RLFIFO", "Template", "Minimal round-robin scheduler in user space.",
    "A starting point for new schedulers", G::Experimental},
   {"scx_rustland", "Rustland", "Userspace", "Its policy runs in user space, written in Rust.",
    "Interactive apps, learning", G::Experimental},
   {"scx_tickless", "Tickless", "Cloud / HPC", "Frees most CPUs from the scheduler tick.",
    "Cloud, virtualization, HPC", G::Experimental},
};

/* Upstream keeps these out of every list: they need a configuration of their own */
static const QSet<QString> HIDDEN = {"scx_layered", "scx_mitosis", "scx_nitosis"};

/* Not schedulers, even though they are named scx_* */
static const QSet<QString> TOOLS = {"scx_loader", "scx_characterize", "scx_forge_agent"};

static const QStringList MODES = {"auto", "gaming", "powersave", "lowlatency", "server"};

static const QHash<QString, QHash<QString, QString>> PRESETS = {
   {"scx_bpfland",
    {{"auto", "-m auto"},
     {"gaming", "-m all"},
     {"powersave", "-s 20000 -m powersave -I 100 -t 100"},
     {"lowlatency", "-m performance -w"},
     {"server", "-s 20000 -S"}}},
   {"scx_lavd",
    {{"auto", "--autopilot --pinned-slice-us 500"},
     {"gaming", "--performance --pinned-slice-us 500"},
     {"powersave", "--powersave --pinned-slice-us 500"},
     {"lowlatency", "--performance --pinned-slice-us 500"},
     {"server", "--performance --slice-min-us 3000 --slice-max-us 10000 --pinned-slice-us 3000"}}},
   {"scx_p2dq",
    {{"auto", "--sched-mode default"},
     {"gaming", "--task-slice true -f --sched-mode performance"},
     {"powersave", "--sched-mode efficiency"},
     {"lowlatency", "-y -f --task-slice true"},
     {"server", "--keep-running"}}},
   {"scx_tickless",
    {{"auto", ""},
     {"gaming", "-f 5000 -s 5000"},
     {"powersave", "-f 50"},
     {"lowlatency", "-f 5000 -s 1000"},
     {"server", "-f 100"}}},
   {"scx_cosmos",
    {{"auto", ""},
     {"gaming", "-s 700"},
     {"powersave", "-m powersave"},
     {"lowlatency", "-s 700 -m performance -w"}}},
};

static QStringList programsInPath() {
   QStringList found;
   QSet<QString> seen;
   for (const QString &dir : qEnvironmentVariable("PATH").split(':', Qt::SkipEmptyParts)) {
      const QStringList names = QDir(dir).entryList({"scx_*"}, QDir::Files | QDir::Executable);
      for (const QString &name : names) {
         if (!seen.contains(name)) {
            seen.insert(name);
            found << name;
         }
      }
   }
   return found;
}

QList<Scheduler> installed() {
   const QStringList programs = programsInPath();
   QList<Scheduler> list;
   for (const Scheduler &known : KNOWN)
      if (programs.contains(known.binary))
         list << known;

   QStringList others;
   for (const QString &program : programs)
      if (!find(program) && !HIDDEN.contains(program) && !TOOLS.contains(program))
         others << program;
   others.sort();
   for (const QString &program : others)
      list << Scheduler{program, name(program), {}, {}, {}, G::Other};
   return list;
}

const Scheduler *find(const QString &binary) {
   for (const Scheduler &known : KNOWN)
      if (known.binary == binary)
         return &known;
   return nullptr;
}

QString name(const QString &binary) {
   if (const Scheduler *known = find(binary))
      return known->name;
   const QString bare = binary.startsWith("scx_") ? binary.mid(4) : binary;
   return bare.isEmpty() ? bare : bare.at(0).toUpper() + bare.mid(1);
}

QString groupLabel(Group group) {
   switch (group) {
   case G::Featured: return "Featured";
   case G::Stable: return "Stable";
   case G::Experimental: return "Experimental";
   case G::Other: break;
   }
   return "Other";
}

QStringList modes(const QString &binary) {
   QStringList list = {"auto"};
   const auto presets = PRESETS.value(binary);
   for (const QString &mode : MODES)
      if (mode != "auto" && presets.contains(mode))
         list << mode;
   list << "custom";
   return list;
}

QString modeLabel(const QString &mode) {
   static const QHash<QString, QString> LABELS = {
      {"auto", "Auto"},           {"gaming", "Gaming"}, {"powersave", "Power Save"},
      {"lowlatency", "Low Latency"}, {"server", "Server"}, {"custom", "Custom"},
   };
   return LABELS.value(mode, mode);
}

QStringList options(const QString &binary, const QString &mode) {
   return PRESETS.value(binary).value(mode).split(' ', Qt::SkipEmptyParts);
}

QString modeFor(const QString &binary, const QStringList &options) {
   for (const QString &mode : modes(binary))
      if (mode != "custom" && catalog::options(binary, mode) == options)
         return mode;
   return "custom";
}

bool validOptions(const QStringList &options) {
   static const QRegularExpression ALLOWED("^[A-Za-z0-9=.,:/+_-]{1,256}$");
   if (options.size() > 32)
      return false;
   for (const QString &option : options)
      if (!ALLOWED.match(option).hasMatch())
         return false;
   return true;
}

} // namespace catalog
