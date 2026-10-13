#include "distro.h"

#include <QFile>
#include <QHash>

namespace distro {

static const QHash<QString, QString> &osRelease() {
    static const QHash<QString, QString> values = [] {
        QHash<QString, QString> out;
        QFile f("/etc/os-release");
        if (!f.open(QIODevice::ReadOnly))
            return out;
        for (const QByteArray &raw : f.readAll().split('\n')) {
            const QString line = QString::fromUtf8(raw).trimmed();
            const qsizetype eq = line.indexOf('=');
            if (eq <= 0)
                continue;
            QString value = line.mid(eq + 1);
            if (value.size() >= 2 && (value.front() == '"' || value.front() == '\''))
                value = value.mid(1, value.size() - 2);
            out.insert(line.left(eq), value);
        }
        return out;
    }();
    return values;
}

static bool is(const QString &id) {
    const auto &os = osRelease();
    return os.value("ID") == id || os.value("ID_LIKE").split(' ').contains(id);
}

QString prettyName() { return osRelease().value("PRETTY_NAME"); }

QString scxInstallCommand() {
    if (is("arch"))
        return "sudo pacman -S scx-scheds scx-tools";
    if (is("fedora"))
        return "sudo dnf copr enable bieszczaders/kernel-cachyos-addons && "
               "sudo dnf install scx-scheds scx-tools";
    return {};
}

QString kernelInstallCommand() {
    const auto &os = osRelease();
    if (os.value("ID") == "debian" && os.value("VERSION_ID") == "13")
        return "sudo apt install -t trixie-backports linux-image-amd64";
    return {};
}

} // namespace distro
