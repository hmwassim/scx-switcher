#include "distro.h"

#include <QFile>
#include <QRegularExpression>
#include <QStandardPaths>

namespace {

QStringList readOsRelease(const QString &path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return QString::fromUtf8(f.readAll()).split('\n');
}

QString osval(const QStringList &lines, const QString &key) {
    const QRegularExpression re("^" + QRegularExpression::escape(key) + "=(.*)$");
    for (const QString &line : lines) {
        const auto m = re.match(line);
        if (!m.hasMatch())
            continue;
        QString v = m.captured(1).trimmed();
        if (v.startsWith('"') && v.endsWith('"') && v.size() >= 2)
            v = v.mid(1, v.size() - 2);
        return v;
    }
    return {};
}

} // namespace

// ── Singleton ─────────────────────────────────────────────────────────────────

DistroInfo *DistroInfo::get() {
    static DistroInfo *inst = new DistroInfo;
    return inst;
}

// ── Construction ──────────────────────────────────────────────────────────────

DistroInfo::DistroInfo() {
    const QString path =
        qEnvironmentVariable("SCX_OS_RELEASE", QStringLiteral("/etc/os-release"));
    const QStringList lines = readOsRelease(path);

    m_id = osval(lines, "ID");
    m_idLike = osval(lines, "ID_LIKE");
    m_prettyName = osval(lines, "PRETTY_NAME");
    m_versionId = osval(lines, "VERSION_ID");

    const QString hay = m_id + ' ' + m_idLike;
    if (hay.contains("debian") || hay.contains("ubuntu") || m_id == "linuxmint" || m_id == "pop")
        m_pkgMgr = PkgMgr::Apt;
    else if (hay.contains("suse"))
        m_pkgMgr = PkgMgr::Zypper;
    else if (hay.contains("fedora") || m_id == "rhel" || m_id == "centos" || m_id == "rocky" ||
             m_id == "alma" || m_id == "ol")
        m_pkgMgr = PkgMgr::Dnf;
    else if (hay.contains("arch") || m_id == "manjaro" || m_id == "cachyos" ||
             m_id == "endeavouros" || m_id == "garuda")
        m_pkgMgr = PkgMgr::Pacman;
    else if (m_id == "solus")
        m_pkgMgr = PkgMgr::Eopkg;
    else if (m_id == "alpine")
        m_pkgMgr = PkgMgr::Apk;
}

// ── Accessors ─────────────────────────────────────────────────────────────────

QString DistroInfo::prettyName() const { return m_prettyName; }

DistroInfo::Escalation DistroInfo::escalation() const {
    static const bool hasPkexec = !QStandardPaths::findExecutable("pkexec").isEmpty();
    static const bool hasSudo = !QStandardPaths::findExecutable("sudo").isEmpty();
    if (hasPkexec)
        return Escalation::Pkexec;
    if (hasSudo)
        return Escalation::Sudo;
    return Escalation::None;
}

QString DistroInfo::escalationTool() const {
    switch (escalation()) {
    case Escalation::Pkexec: return QStringLiteral("pkexec");
    case Escalation::Sudo: return QStringLiteral("sudo");
    case Escalation::None: return {};
    }
    return {};
}

QStringList DistroInfo::scxPackages() const {
    switch (m_pkgMgr) {
    case PkgMgr::Apt:
    case PkgMgr::Pacman:
    case PkgMgr::Eopkg: return {QStringLiteral("scx-scheds"), QStringLiteral("scx-tools")};
    // Fedora's scx-scheds/scx-tools come from the CachyOS COPR (see install.sh).
    case PkgMgr::Dnf: return {QStringLiteral("scx-scheds"), QStringLiteral("scx-tools")};
    // openSUSE's "scx" ships schedulers only — no scxctl/scx_loader, so fall
    // back to the generic source-build hint instead of a misleading command.
    case PkgMgr::Zypper:
    case PkgMgr::Apk:
    case PkgMgr::Unknown: return {};
    }
    return {};
}

QString DistroInfo::installCommand(const QStringList &pkgs) const {
    if (pkgs.isEmpty())
        return {};
    const bool hasSudo = !QStandardPaths::findExecutable("sudo").isEmpty();
    const QString su = hasSudo ? QStringLiteral("sudo ") : QString();
    const QString list = pkgs.join(' ');
    switch (m_pkgMgr) {
    case PkgMgr::Apt: return su + "apt install " + list;
    case PkgMgr::Dnf: return su + "dnf install " + list;
    case PkgMgr::Zypper: return su + "zypper install " + list;
    case PkgMgr::Pacman: return su + "pacman -S " + list;
    case PkgMgr::Eopkg: return su + "eopkg install " + list;
    case PkgMgr::Apk: return su + "apk add " + list;
    case PkgMgr::Unknown: return {};
    }
    return {};
}

QString DistroInfo::kernelUpgradeHint() const {
    if (m_pkgMgr == PkgMgr::Apt) {
        if (m_id == "debian" && m_versionId == "13")
            return "sudo apt install -t trixie-backports linux-image-amd64 linux-headers-amd64";
        return "sudo apt install linux-image-amd64 linux-headers-amd64";
    }
    return {};
}

QString DistroInfo::escalationHint() const {
    const bool hasSudo = !QStandardPaths::findExecutable("sudo").isEmpty();
    const QString su = hasSudo ? QStringLiteral("sudo ") : QString();
    QString cmd;
    switch (m_pkgMgr) {
    case PkgMgr::Apt: cmd = su + "apt install polkitd pkexec"; break;
    case PkgMgr::Dnf: cmd = su + "dnf install polkit"; break;
    case PkgMgr::Zypper: cmd = su + "zypper install polkit"; break;
    case PkgMgr::Pacman: cmd = su + "pacman -S polkit"; break;
    case PkgMgr::Eopkg: cmd = su + "eopkg install polkit"; break;
    case PkgMgr::Apk: cmd = su + "apk add polkit"; break;
    case PkgMgr::Unknown: cmd = "polkit"; break;
    }
    if (cmd == "polkit")
        return "Install polkit (providing pkexec) or sudo, then log out and back in.";
    return "Install it with:\n  " + cmd + "\n\nThen log out and back in so a PolKit agent starts.";
}
