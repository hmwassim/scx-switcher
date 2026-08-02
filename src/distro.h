#pragma once

#include <QString>
#include <QStringList>

// DistroInfo — distribution-aware behavior.
//
// Detects the running distribution from /etc/os-release (overridable via the
// SCX_OS_RELEASE environment variable, mainly for testing) and derives:
//   - the package manager in use
//   - whether systemd is the init system
//   - the available privilege-escalation tool (pkexec preferred, sudo fallback)
//   - per-distribution package names and install/upgrade command hints
class DistroInfo {
    public:
    enum class PkgMgr { Apt, Dnf, Zypper, Pacman, Eopkg, Apk, Unknown };
    enum class Escalation { None, Pkexec, Sudo };

    static DistroInfo *get();

    QString id() const;
    QString prettyName() const;
    PkgMgr pkgMgr() const;
    QString pkgMgrName() const;

    bool systemdActive() const;

    Escalation escalation() const;
    QString escalationTool() const;

    QStringList scxPackages() const;
    QString installCommand(const QStringList &pkgs) const;
    QString kernelUpgradeHint() const;
    QString escalationHint() const;

    private:
    explicit DistroInfo();

    QString m_id;
    QString m_idLike;
    QString m_prettyName;
    QString m_versionId;
    PkgMgr m_pkgMgr = PkgMgr::Unknown;
};
