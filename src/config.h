#pragma once

#include <QString>
#include <QStringList>

struct SchedulerInfo {
    QString bare;
    QString display;
    QString category;
    QString desc;
};

inline const QList<SchedulerInfo> ALL_SCHEDULERS = {
    {"bpfland", "BPFland", "Gaming / Interactive",
     "A vruntime-based scheduler prioritising interactive tasks that block frequently. "
     "Cache-topology aware \u2014 keeps tasks near their L2/L3 cache. Recommended default for "
     "desktop and gaming."},
    {"lavd", "LAVD", "Gaming / Low Latency",
     "Latency-Aware Virtual Deadline scheduler. Computes a latency-criticality score per "
     "task and assigns virtual deadlines. Core Compaction keeps active cores at high "
     "frequency. Autopilot mode auto-switches between performance/balanced/powersave."},
    {"rusty", "Rusty", "Desktop / Server",
     "Partitions CPUs by last-level cache domain to minimise cross-cache migration. "
     "Good scalability on high core-count systems. Hybrid BPF + userspace design."},
    {"flash", "Flash", "Desktop / Soft RT",
     "Emphasises fairness and predictability over prioritising interactive tasks. "
     "Good for batch, encoding, and audio workloads."},
    {"cosmos", "Cosmos", "General Purpose",
     "Lightweight locality-first scheduler. Keeps tasks on the same CPU using local "
     "DSQs when not saturated. Under load switches to deadline-based policy. "
     "10\u00b5s default time slices. Low overhead general-purpose choice."},
    {"layered", "Layered", "Power Users",
     "Classifies threads into named layers (like cgroups) with independent scheduling "
     "policies per layer. Highly flexible but requires manual JSON config."},
    {"nest", "Nest", "Lightly-Loaded",
     "Places tasks on already-warm, high-frequency cores to keep turbo boost active. "
     "Effective when CPU utilisation is low to moderate."},
    {"p2dq", "P2DQ", "Mixed Desktop/Server",
     "Pick-2 randomised load balancing keeps queues shallow. Simple design means low "
     "scheduler overhead. PELT-based load tracking."},
    {"tickless", "Tickless", "Cloud / HPC",
     "Routes scheduling through a small pool of primary CPUs, allowing others to run "
     "tickless (no scheduler interrupts). Reduces OS noise for VMs and HPC."},
    {"simple", "Simple", "Reference / Testing",
     "Minimal FIFO/least-runtime policy. No topology awareness. Useful as a baseline "
     "for benchmarking and understanding sched_ext."},
    {"beerland", "Beerland", "Gaming / Desktop",
     "A reduced-overhead variant of scx_bpfland by the same author. Strips back "
     "expensive per-task tracking for lower scheduler overhead on busy systems."},
    {"rustland", "Rustland", "Userspace / Educational",
     "Predecessor to bpfland with similar logic but running in userspace (Rust). "
     "More readable for learning but adds context-switch overhead."},
};

inline const QString ERROR_NO_POLKIT = R"(
pkexec is not available on this system.

Please ensure polkitd and pkexec are installed:
  sudo apt install polkitd pkexec
)";

inline const QString ERROR_SCHED_NOT_INSTALLED = R"(
The selected scheduler is not installed on this system.

Install the scx-scheds package, or check scxctl list for available options.
)";

inline const QString ERROR_SWITCH_FAILED = R"(
Failed to switch scheduler.

This usually means:
- You're not in a PolKit authentication session
- The scx_loader service is not running
- Another scheduler is already active

Try running: pkexec scxctl status
)";

inline const QString ERROR_STOP_FAILED = R"(
Failed to stop scheduler.

This usually means:
- You're not in a PolKit authentication session
- The current scheduler doesn't respond to stop commands

Try running: pkexec scxctl status
)";

inline const QString ERROR_PERSIST_FAILED_ENABLE = R"(
Failed to enable auto-start on boot.

Try running manually:
  sudo systemctl enable --now scx_loader.service
)";

inline const QString ERROR_PERSIST_FAILED_DISABLE = R"(
Failed to disable auto-start on boot.

Try running manually:
  sudo systemctl disable --now scx_loader.service
)";
