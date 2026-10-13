#include "ui.h"

#include <QApplication>
#include <QLabel>
#include <QPainter>
#include <QPalette>
#include <QTime>

namespace ui {

static bool darkTheme() {
    return QApplication::palette().color(QPalette::Window).lightness() < 128;
}

QColor color(Tone tone) {
    const bool dark = darkTheme();
    switch (tone) {
    case Tone::Green: return dark ? QColor("#3fb950") : QColor("#1a7f37");
    case Tone::Red: return dark ? QColor("#f85149") : QColor("#cf222e");
    case Tone::Amber: return dark ? QColor("#d29922") : QColor("#9a6700");
    case Tone::Blue: return dark ? QColor("#58a6ff") : QColor("#0969da");
    case Tone::Purple: return dark ? QColor("#bc8cff") : QColor("#8250df");
    case Tone::Grey: break;
    }
    return dark ? QColor("#8b949e") : QColor("#6e7781");
}

QIcon dot(Tone tone, int size) {
    QPixmap pm(size * 2, size * 2);
    pm.setDevicePixelRatio(2);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(color(tone));
    p.drawEllipse(QRectF(size * 0.2, size * 0.2, size * 0.6, size * 0.6));
    return QIcon(pm);
}

QIcon chip(Tone tone) {
    const QColor c = color(tone);
    QPixmap pm(32, 32);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(c, 2.5));
    p.drawRoundedRect(QRectF(8, 8, 16, 16), 3, 3);
    p.setPen(Qt::NoPen);
    p.setBrush(c);
    p.drawRoundedRect(QRectF(12.5, 12.5, 7, 7), 1.5, 1.5);
    for (const qreal at : {11.0, 18.0}) {
        p.drawRect(QRectF(at, 2, 3, 4));
        p.drawRect(QRectF(at, 26, 3, 4));
        p.drawRect(QRectF(2, at, 4, 3));
        p.drawRect(QRectF(26, at, 4, 3));
    }
    return QIcon(pm);
}

void setBadge(QLabel *label, const QString &text, Tone tone) {
    const QColor c = color(tone);
    const QString rgb = QString("%1, %2, %3").arg(c.red()).arg(c.green()).arg(c.blue());
    label->setText(text);
    label->setVisible(!text.isEmpty());
    // Stylesheet fonts replace the widget font, so the size goes here too.
    const qreal size = QApplication::font().pointSizeF() * 0.85;
    label->setStyleSheet(QString("QLabel { color: %1; background: rgba(%2, 0.14);"
                                 " border: 1px solid rgba(%2, 0.45); border-radius: 6px;"
                                 " padding: 2px 8px; font-weight: 600; font-size: %3pt; }")
                             .arg(c.name(), rgb)
                             .arg(size));
}

QLabel *badge(const QString &text, Tone tone) {
    auto *label = new QLabel;
    setBadge(label, text, tone);
    return label;
}

Tone maturityTone(catalog::Maturity maturity) {
    switch (maturity) {
    case catalog::Maturity::Production: return Tone::Green;
    case catalog::Maturity::Experimental: return Tone::Amber;
    case catalog::Maturity::NotForProduction: return Tone::Red;
    case catalog::Maturity::Unstated: break;
    }
    return Tone::Grey;
}

QString logLine(const QString &message, Tone tone) {
    QString body = message.toHtmlEscaped();
    body.replace('\n', "<br>&nbsp;&nbsp;");
    return QString("<span style='color:%1'>%2</span>&nbsp; <span style='color:%3'>%4</span>")
        .arg(color(Tone::Grey).name(), QTime::currentTime().toString("HH:mm:ss"),
             color(tone).name(), body);
}

} // namespace ui
