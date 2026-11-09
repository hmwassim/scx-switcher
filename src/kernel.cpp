#include "kernel.h"

#include <QFile>

namespace kernel {

static const QString SYSFS = QStringLiteral("/sys/kernel/sched_ext");

static QByteArray readLine(const QString &path) {
   QFile file(path);
   if (!file.open(QIODevice::ReadOnly))
      return {};
   return file.readLine().trimmed();
}

State state() {
   const QByteArray value = readLine(SYSFS + "/state");
   if (value.isEmpty())
      return State::Unsupported;
   if (value == "enabled")
      return State::Enabled;
   return value == "disabled" ? State::Disabled : State::Changing;
}

quint64 enableSeq() {
   return readLine(SYSFS + "/enable_seq").toULongLong();
}

QString running() {
   /* Ops names carry a version suffix: "bpfland_1.1.4_g1a2b3c_x86_64_..." */
   const QByteArray ops = readLine(SYSFS + "/root/ops");
   if (ops.isEmpty())
      return {};
   return "scx_" + QString::fromLatin1(ops).section('_', 0, 0);
}

} // namespace kernel
