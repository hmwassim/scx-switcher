#include "activity.h"

#include <QTime>

static constexpr int MAX_LINES = 200;

void Activity::add(const QString &text) {
   const QString stamp = QTime::currentTime().toString("HH:mm:ss");
   for (const QString &part : text.split('\n', Qt::SkipEmptyParts)) {
      const QString line = stamp + "  " + part;
      m_lines << line;
      emit added(line);
   }
   while (m_lines.size() > MAX_LINES)
      m_lines.removeFirst();
}
