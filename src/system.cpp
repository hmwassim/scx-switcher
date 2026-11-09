#include "system.h"

#include <QFile>
#include <QProcess>
#include <QStandardPaths>
#include <QTimer>
#include <memory>

namespace sys {

static const QString CONFIG = QStringLiteral("/etc/default/scx");
static constexpr int HELPER_TIMEOUT_MS = 120000;

void run(QObject *context, const QString &program, const QStringList &args, Done done,
         int timeoutMs) {
   auto *proc = new QProcess(context);
   proc->setProcessChannelMode(QProcess::MergedChannels);

   /* Answering once, finished and errorOccurred can both fire */
   auto pending = std::make_shared<Done>(std::move(done));
   auto finish = [proc, pending](int code, const QString &output) {
      if (!*pending)
         return;
      const Done callback = std::exchange(*pending, nullptr);
      proc->deleteLater();
      callback(code, output);
   };

   QObject::connect(proc, &QProcess::finished, proc,
                    [proc, finish](int code, QProcess::ExitStatus status) {
                       const QString output = QString::fromUtf8(proc->readAll()).trimmed();
                       finish(status == QProcess::NormalExit ? code : -1, output);
                    });
   QObject::connect(proc, &QProcess::errorOccurred, proc,
                    [proc, finish](QProcess::ProcessError error) {
                       if (error == QProcess::FailedToStart)
                          finish(-1, proc->errorString());
                    });
   QTimer::singleShot(timeoutMs, proc, [proc] { proc->kill(); });

   proc->start(program, args);
}

void helper(QObject *context, const QStringList &args, Done done) {
   run(context, "pkexec", QStringList{SCX_HELPER_PATH} + args, std::move(done),
       HELPER_TIMEOUT_MS);
}

void units(QObject *context, std::function<void(const Units &)> done) {
   const QStringList args = {"show",          "-p", "ActiveState", "-p", "UnitFileState",
                             "scx.service",   "scx_loader.service"};
   run(context, "systemctl", args, [done](int, const QString &output) {
      /* One block per unit, in the order asked */
      Units units;
      const QStringList blocks = output.split("\n\n");
      const auto value = [&blocks](int block, const QString &key) {
         if (block >= blocks.size())
            return QString();
         for (const QString &line : blocks.at(block).split('\n'))
            if (line.startsWith(key + '='))
               return line.mid(key.size() + 1).trimmed();
         return QString();
      };
      units.state = value(0, "ActiveState");
      units.active = units.state == "active";
      units.enabled = value(0, "UnitFileState") == "enabled";
      units.loaderActive = value(1, "ActiveState") == "active";
      done(units);
   });
}

static QString unquote(QString value) {
   value = value.trimmed();
   if (value.size() >= 2 && (value.front() == '\'' || value.front() == '"') &&
       value.back() == value.front())
      value = value.mid(1, value.size() - 2);
   return value;
}

Config config() {
   Config config;
   QFile file(CONFIG);
   if (!file.open(QIODevice::ReadOnly))
      return config;
   while (!file.atEnd()) {
      const QString line = QString::fromUtf8(file.readLine()).trimmed();
      if (line.startsWith("SCX_SCHEDULER="))
         config.sched = unquote(line.mid(14));
      else if (line.startsWith("SCX_FLAGS="))
         config.options = unquote(line.mid(10)).split(' ', Qt::SkipEmptyParts);
   }
   return config;
}

bool pkexecAvailable() {
   return !QStandardPaths::findExecutable("pkexec").isEmpty();
}

} // namespace sys
