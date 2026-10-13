#pragma once

#include <QString>

// Per-distribution install hints, derived from /etc/os-release.
namespace distro {

QString prettyName();
QString scxInstallCommand();    // empty if the distro has no packages
QString kernelInstallCommand(); // empty if there is no specific advice

} // namespace distro
