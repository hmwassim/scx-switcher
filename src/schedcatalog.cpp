#include "schedcatalog.h"

SchedCatalog *SchedCatalog::get() {
    static SchedCatalog *inst = new SchedCatalog;
    return inst;
}

SchedCatalog::SchedCatalog()
    : m_schedulers({
          {"bpfland",
           "BPFland",
           "Gaming / Interactive",
           "vruntime-based scheduler that prioritises interactive tasks which block frequently. "
           "Cache-topology aware \u2014 keeps tasks near their L2/L3 cache. Good all-round desktop "
           "choice.",
           {"auto", "gaming", "lowlatency", "powersave", "server"}},

          {"beerland",
           "Beerland",
           "Gaming / Desktop",
           "Reduced-overhead variant of bpfland by the same author. Drops expensive per-task "
           "tracking for lower scheduler overhead on busy systems.",
           {"auto", "gaming", "lowlatency", "powersave", "server"}},

          {"cake",
           "Cake",
           "Gaming / Low Latency",
           "CAKE DRR++ adapted for CPU scheduling. 4-tier classification "
           "(Critical/Interactive/Frame/Bulk), "
           "auto game detection, EEVDF-inspired weighting, yield-gated quantum, per-LLC DSQ "
           "sharding. Profiles: gaming, esports, legacy.",
           {"auto", "gaming", "lowlatency", "powersave", "server"}},

          {"chaos",
           "Chaos",
           "Testing / Debug",
           "Intentionally degrades performance to expose race conditions. Random delays, CPU freq "
           "scaling, kprobe delays. Not for production use.",
           {"auto"}},

          {"cosmos",
           "Cosmos",
           "General Purpose",
           "Locality-first scheduler. Uses local DSQs when not saturated, switching to deadline "
           "policy under load. 10\u00b5s default time slices; low overhead.",
           {"auto", "gaming", "lowlatency", "powersave", "server"}},

          {"flash",
           "Flash",
           "Desktop / Soft RT",
           "Prioritises fairness and predictability. Suited to batch work, encoding, and audio.",
           {"auto", "gaming", "powersave"}},

          {"flow",
           "Flow",
           "Desktop / Deterministic",
           "Budget-driven scheduler with zero heuristics. Tasks accumulate budget while sleeping "
           "and spend it while running. 4 FIFO dispatch tiers rotate round-robin. No starvation. "
           "No modes \u2014 every decision comes from remaining budget.",
           {"auto"}},

          {"forge",
           "Forge",
           "Experimental / AI",
           "AI-optimised base scheduler evolved in place by scx_forge_agent. Default policy: "
           "wakeup placement, per-CPU DSQs with work stealing. Not production-ready without "
           "customisation.",
           {"auto"}},

          {"lavd",
           "LAVD",
           "Gaming / Low Latency",
           "Latency-Aware Virtual Deadline scheduler. Assigns deadlines based on latency "
           "criticality. Core Compaction keeps active cores at high frequency. Autopilot switches "
           "power profiles automatically.",
           {"auto", "gaming", "lowlatency", "powersave", "server"}},

          {"layered",
           "Layered",
           "Power Users",
           "Classifies threads into named layers with independent scheduling policies. "
           "Very flexible but needs a JSON config file.",
           {"auto"}},

          {"mitosis",
           "Mitosis",
           "Cloud / HPC",
           "Cgroup-aware cell isolation. Each cgroup child gets a dedicated CPU set with shared "
           "dispatch queue. LLC-aware on multi-LLC systems. Reduces cross-cell interference for "
           "predictable performance.",
           {"auto", "server"}},

          {"p2dq",
           "P2DQ",
           "Mixed Desktop / Server",
           "Pick-2 randomised load balancing keeps queues shallow. Low scheduler overhead. "
           "PELT-based load tracking.",
           {"auto", "gaming", "powersave", "server"}},

          {"pandemonium",
           "Pandemonium",
           "Desktop / Low Latency",
           "Behavioural-classification scheduler with topology-aware placement. 3-tier dispatch, "
           "damped harmonic oscillator CoDel stall detection, MWU adaptive control loop. "
           "Self-calibrates from 2C to 32C+.",
           {"auto", "gaming", "lowlatency", "powersave", "server"}},

          {"rlfifo",
           "RLFIFO",
           "Testing / Reference",
           "Simple FIFO round-robin based on scx_rustland_core. Educational template for building "
           "user-space schedulers. Not production-ready.",
           {"auto"}},

          {"rustland",
           "Rustland",
           "Userspace / Educational",
           "Predecessor to bpfland with similar logic but running in userspace (Rust). "
           "Higher context-switch overhead; more readable for learning.",
           {"auto"}},

          {"rusty",
           "Rusty",
           "Desktop / Server",
           "Partitions CPUs by last-level cache domain to minimise cross-cache migration. "
           "Scales well on high core-count machines. Hybrid BPF + userspace design.",
           {"auto", "gaming"}},

          {"tickless",
           "Tickless",
           "Cloud / HPC",
           "Routes scheduling through a small pool of primary CPUs so others can run tickless. "
           "Reduces OS noise for VMs and HPC workloads.",
           {"auto", "powersave", "server"}},
      }),
      m_displayOverrides({
          {"bpfland", "BPFland"}, {"beerland", "Beerland"}, {"cake", "Cake"},
          {"flow", "Flow"},       {"forge", "Forge"},       {"lavd", "LAVD"},
          {"p2dq", "P2DQ"},       {"rlfifo", "RLFIFO"},     {"rustland", "Rustland"},
      }) {}

SchedInfo SchedCatalog::find(const QString &bare) const {
    for (const auto &si : m_schedulers) {
        if (si.bare == bare)
            return si;
    }
    return {};
}

QStringList SchedCatalog::modes(const QString &bare) const {
    const auto info = find(bare);
    return info.modes.isEmpty() ? QStringList{"auto"} : info.modes;
}

QStringList SchedCatalog::allNames() const {
    QStringList names;
    names.reserve(m_schedulers.size());
    for (const auto &si : m_schedulers)
        names << si.bare;
    return names;
}

QString SchedCatalog::humanize(const QString &bare) const {
    const auto it = m_displayOverrides.constFind(bare);
    if (it != m_displayOverrides.cend())
        return it.value();
    if (bare.isEmpty())
        return bare;
    QString s = bare;
    s[0] = s[0].toUpper();
    return s;
}

QString SchedCatalog::tomlMode(const QString &mode) const {
    static const QHash<QString, QString> map = {
        {"auto", "Auto"},           {"gaming", "Gaming"}, {"lowlatency", "LowLatency"},
        {"powersave", "PowerSave"}, {"server", "Server"},
    };
    return map.value(mode.toLower(), "Auto");
}

QString SchedCatalog::humanizeMode(const QString &mode) const {
    static const QHash<QString, QString> map = {
        {"auto", "Auto"},
        {"gaming", "Gaming"},
        {"lowlatency", "Low Latency"},
        {"powersave", "Power Save"},
        {"server", "Server"},
    };
    return map.value(mode.toLower(), mode);
}
