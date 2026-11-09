#pragma once

#include <QColor>
#include <QIcon>
#include <QString>

class QLabel;

/* The only colors the app picks itself: the status dot, the tags and the tray icon */
namespace style {

enum class Tone { Grey, Blue, Green, Red, Amber };

QColor color(Tone tone);
QPixmap dot(Tone tone, int size, qreal ratio);
QIcon trayIcon(Tone tone);
void setTag(QLabel *label, const QString &text, Tone tone);

} // namespace style
