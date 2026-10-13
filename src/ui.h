#pragma once

#include "catalog.h"

#include <QColor>
#include <QIcon>
#include <QString>

class QLabel;

// Small shared look: one accent palette that adapts to light and dark themes.
namespace ui {

enum class Tone { Green, Red, Amber, Blue, Purple, Grey };

QColor color(Tone tone);
QIcon dot(Tone tone, int size = 12);
QIcon chip(Tone tone); // tray icon

QLabel *badge(const QString &text, Tone tone);
void setBadge(QLabel *label, const QString &text, Tone tone);

Tone maturityTone(catalog::Maturity maturity);

// Rich-text log line: grey timestamp, message in the given tone.
QString logLine(const QString &message, Tone tone);

} // namespace ui
