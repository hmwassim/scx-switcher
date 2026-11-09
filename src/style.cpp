#include "style.h"

#include <QApplication>
#include <QLabel>
#include <QPainter>
#include <QPalette>

namespace style {

static bool darkWindow() {
   return QApplication::palette().color(QPalette::Window).lightness() < 128;
}

/* Two sets, picked from the window's lightness, so each tone reads on any theme */
QColor color(Tone tone) {
   const bool dark = darkWindow();
   switch (tone) {
   case Tone::Green: return dark ? QColor("#3fb950") : QColor("#1a7f37");
   case Tone::Red: return dark ? QColor("#f85149") : QColor("#cf222e");
   case Tone::Amber: return dark ? QColor("#d29922") : QColor("#9a6700");
   case Tone::Blue: return dark ? QColor("#58a6ff") : QColor("#0969da");
   case Tone::Grey: break;
   }
   return dark ? QColor("#8b949e") : QColor("#6e7781");
}

QPixmap dot(Tone tone, int size, qreal ratio) {
   QPixmap pixmap(QSize(size, size) * ratio);
   pixmap.setDevicePixelRatio(ratio);
   pixmap.fill(Qt::transparent);
   QPainter painter(&pixmap);
   painter.setRenderHint(QPainter::Antialiasing);
   painter.setPen(Qt::NoPen);
   painter.setBrush(color(tone));
   painter.drawEllipse(QRectF(1, 1, size - 2, size - 2));
   return pixmap;
}

/* The outline chip from version 2, drawn in the status tone */
QIcon trayIcon(Tone tone) {
   const QColor c = color(tone);
   QIcon icon;
   for (const int size : {16, 22, 32, 48}) {
      QPixmap pixmap(size, size);
      pixmap.fill(Qt::transparent);
      QPainter painter(&pixmap);
      painter.setRenderHint(QPainter::Antialiasing);
      painter.scale(size / 32.0, size / 32.0);
      painter.setPen(QPen(c, 2.5));
      painter.drawRoundedRect(QRectF(8, 8, 16, 16), 3, 3);
      painter.setPen(Qt::NoPen);
      painter.setBrush(c);
      painter.drawRoundedRect(QRectF(12.5, 12.5, 7, 7), 1.5, 1.5);
      for (const qreal at : {11.0, 18.0}) {
         painter.drawRect(QRectF(at, 2, 3, 4));
         painter.drawRect(QRectF(at, 26, 3, 4));
         painter.drawRect(QRectF(2, at, 4, 3));
         painter.drawRect(QRectF(26, at, 4, 3));
      }
      icon.addPixmap(pixmap);
   }
   return icon;
}

/* Solid tag: the tone as background, white or near-black text for contrast */
void setTag(QLabel *label, const QString &text, Tone tone) {
   label->setText(text);
   label->setVisible(!text.isEmpty());
   const QColor background = color(tone);
   const QString foreground = darkWindow() ? "#0d1117" : "#ffffff";
   const qreal points = QApplication::font().pointSizeF() * 0.85;
   label->setStyleSheet(QString("QLabel { background: %1; color: %2; border-radius: 4px;"
                                " padding: 1px 7px; font-weight: 600; font-size: %3pt; }")
                           .arg(background.name(), foreground)
                           .arg(points));
}

} // namespace style
