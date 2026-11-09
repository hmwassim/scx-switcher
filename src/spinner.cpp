#include "spinner.h"

#include <QPainter>

static constexpr int FRAME_MS = 40;
static constexpr int STEP = 30;
static constexpr int RING = 18;

Spinner::Spinner(QWidget *parent) : QWidget(parent) {
   m_timer.setInterval(FRAME_MS);
   connect(&m_timer, &QTimer::timeout, this, [this] {
      m_angle = (m_angle + STEP) % 360;
      update();
   });
}

void Spinner::paintEvent(QPaintEvent *) {
   QPainter painter(this);
   painter.setRenderHint(QPainter::Antialiasing);
   const QRectF ring(width() / 2.0 - RING / 2.0, height() / 2.0 - RING / 2.0, RING, RING);
   const QRectF inside = ring.adjusted(1.5, 1.5, -1.5, -1.5);

   painter.setPen(QPen(palette().color(QPalette::Mid), 2.5));
   painter.drawEllipse(inside);
   painter.setPen(QPen(palette().color(QPalette::Highlight), 2.5, Qt::SolidLine, Qt::RoundCap));
   painter.drawArc(inside, (90 - m_angle) * 16, -100 * 16);
}

void Spinner::showEvent(QShowEvent *) {
   m_timer.start();
}

void Spinner::hideEvent(QHideEvent *) {
   m_timer.stop();
}
