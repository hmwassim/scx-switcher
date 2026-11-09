#pragma once

#include <QObject>
#include <QStringList>

/* The activity log, kept outside the window so it survives closing to the tray */
class Activity : public QObject {
   Q_OBJECT
public:
   using QObject::QObject;

   void add(const QString &text);
   const QStringList &lines() const { return m_lines; }

signals:
   void added(const QString &line);

private:
   QStringList m_lines;
};
