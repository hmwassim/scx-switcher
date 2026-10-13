#include "catalog.h"

#include <QList>

namespace catalog {

using M = Maturity;

static const QList<Component> COMPONENTS = {
    // Schedulers
    {"scx_beerland", "Beerland", true, "Locality", M::Production,
     "Keeps tasks close to their caches with a deadline-ordered queue per CPU. Tasks only "
     "migrate on wakeup while the system has spare capacity; under saturation, CPUs pull the "
     "earliest-deadline task from other queues.",
     "Cache-intensive workloads and machines with many CPUs."},
    {"scx_bpfland", "BPFland", true, "Interactive", M::Production,
     "Fully-BPF descendant of scx_rustland. Tasks that voluntarily switch context often are "
     "classed as interactive and served from a higher-priority queue ahead of regular tasks.",
     "Gaming, live streaming, multimedia and real-time audio, especially alongside "
     "CPU-heavy background work."},
    {"scx_cake", "Cake", true, "Gaming", M::Unstated,
     "Gaming-first scheduler named after and inspired by the CAKE network queue manager. Aims "
     "for steady frame times, low input lag and small worst-case stutters while staying a "
     "competent general-purpose scheduler.",
     "Gaming desktops."},
    {"scx_chaos", "Chaos", true, "Testing", M::NotForProduction,
     "Deliberately injects latency and performance degradation to expose race conditions and "
     "timing-dependent bugs. Built on p2dq. May make the system unstable.",
     "Race-condition hunting, stress tests and CI. Never for daily use."},
    {"scx_cosmos", "Cosmos", true, "General purpose", M::Production,
     "Lightweight and locality-first: keeps tasks on the same CPU while the system isn't "
     "saturated, then switches to a deadline policy on a shared queue so interactive tasks run "
     "promptly. Batches CPU wakeups to cut overhead.",
     "Desktops and servers alike; it adapts to the workload."},
    {"scx_flash", "Flash", true, "Low latency", M::Production,
     "Earliest-deadline-first scheduling with a dynamic latency weight: tasks that give up the "
     "CPU before their slice ends are prioritised over tasks that use it all. Fair and "
     "predictable even when overcommitted.",
     "Latency-sensitive work such as multimedia and real-time audio."},
    {"scx_flow", "Flow", true, "Deterministic", M::Production,
     "Budget-driven, with zero heuristics: tasks earn budget while sleeping and spend it while "
     "running. Each task gets an equal share of its CPU, so there's no starvation or priority "
     "inversion.",
     "Deterministic workloads (embedded, robotics, automotive), and desktops, where "
     "interactive tasks separate from bulk work naturally."},
    {"scx_forge", "Forge", true, "AI research", M::NotForProduction,
     "Base scheduler meant to be rewritten in place by the LLM-driven scx_forge_agent, which "
     "builds, runs and scores each policy change. Not meant to be used unchanged.",
     "Developing scheduling policies with AI tooling."},
    {"scx_lavd", "LAVD", true, "Gaming", M::Production,
     "Latency-criticality Aware Virtual Deadline: measures how latency-critical each task is "
     "and uses that for its deadline and time slice. Separate domains per LLC, core type and "
     "NUMA node; autopilot picks a power profile.",
     "Gaming and other highly interactive apps. Mainly targets single-socket machines."},
    {"scx_layered", "Layered", true, "Configurable", M::Production,
     "Sorts tasks into layers (for example by cgroup) and gives each layer its own policy, such "
     "as guaranteed CPU share on chosen cores. Needs a JSON layer config to be useful.",
     "Tuned deployments that need priority access to cores for specific services."},
    {"scx_mitosis", "Mitosis", true, "Datacenter", M::Unstated,
     "Cgroup-aware scheduler that isolates workloads into cells of dedicated CPUs, with the goal "
     "of safely overcommitting datacenter servers.",
     "Servers running several isolated workloads."},
    {"scx_mlfq", "MLFQ", true, "Desktop", M::Experimental,
     "Multilevel feedback queue: interactive, unclassified and CPU-bound queues ordered by "
     "virtual deadline. An interactivity gauge promotes tasks that sleep and demotes ones that "
     "keep running.",
     "Gaming, desktop responsiveness during updates or compiles, mixed laptop workloads."},
    {"scx_p2dq", "P2DQ", true, "General purpose", M::Production,
     "Pick-two load balancing across caches and NUMA nodes, with all decisions made in BPF. "
     "Tested on services at Meta with performance similar to EEVDF.",
     "Gaming, desktop, batch and server work, with tuning per use case."},
    {"scx_pandemonium", "Pandemonium", true, "Adaptive", M::Unstated,
     "Gives every task a latency tier by its role and adapts in real time: CoDel-style stall "
     "detection, topology-aware placement, and work stealing that only crosses a cache "
     "boundary when it pays off.",
     "Desktops wanting low wakeup latency under heavy CPU load."},
    {"scx_rlfifo", "RLFIFO", true, "Template", M::NotForProduction,
     "Minimal round-robin scheduler running in user space on scx_rustland_core: FIFO order with "
     "dynamic slices.",
     "A baseline template for testing new policies."},
    {"scx_rustland", "Rustland", true, "Userspace", M::Unstated,
     "The whole policy runs in user-space Rust on top of scx_rustland_core. Prioritises "
     "interactive work, at some overhead compared with in-kernel BPF schedulers.",
     "Low-latency interactive apps, and a readable starting point for Rust schedulers."},
    {"scx_rusty", "Rusty", true, "Load balancing", M::Production,
     "Hybrid design: BPF round-robins within each LLC domain while user-space Rust computes load "
     "and tells BPF how to balance between domains. Highly tunable.",
     "Varied architectures and workloads, best on single-socket multi-CCX machines."},
    {"scx_tickless", "Tickless", true, "Cloud / HPC", M::Experimental,
     "Routes all scheduling events through a few primary CPUs (CPU 0 by default) so the others "
     "can run without the scheduler tick, reducing OS noise.",
     "Cloud, virtualization and HPC. Not for latency-sensitive work."},

    // Tools
    {"scx_loader", "scx_loader", false, "Daemon", M::Unstated,
     "D-Bus service that starts, stops and switches schedulers, and starts the boot default "
     "from /etc/scx_loader/config.toml.",
     "What SCX Switcher talks to."},
    {"scxctl", "scxctl", false, "CLI", M::Unstated,
     "Command-line client for scx_loader: get, list, start, switch, stop and inspect modes.",
     "Scripting and terminal use."},
    {"scxtop", "scxtop", false, "Monitoring", M::Unstated,
     "top-like observability for sched_ext: live per-CPU, LLC and NUMA metrics in a TUI, "
     "Perfetto trace capture, and an MCP server for AI-assisted analysis.",
     "Seeing what a scheduler is doing. Run it as root."},
    {"scx_characterize", "scx_characterize", false, "Analysis", M::Unstated,
     "Workload characterization tool for sched_ext schedulers.", "Profiling workloads."},
};

const Component *find(const QString &binary) {
    for (const auto &c : COMPONENTS)
        if (c.binary == binary)
            return &c;
    return nullptr;
}

const Component *scheduler(const QString &name) { return find("scx_" + name); }

QString displayName(const QString &name) {
    if (const auto *c = scheduler(name))
        return c->display;
    return name.isEmpty() ? name : name.at(0).toUpper() + name.mid(1);
}

QString modeLabel(const QString &mode) {
    if (mode == "lowlatency")
        return "Low Latency";
    if (mode == "powersave")
        return "Power Save";
    return mode.isEmpty() ? mode : mode.at(0).toUpper() + mode.mid(1);
}

QString maturityLabel(Maturity maturity) {
    switch (maturity) {
    case M::Production: return "Production ready";
    case M::Experimental: return "Experimental";
    case M::NotForProduction: return "Not for production";
    case M::Unstated: break;
    }
    return {};
}

} // namespace catalog
