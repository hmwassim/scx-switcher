#pragma once

#include <QTimer>
#include <QWidget>

/* A small ring in the theme's highlight color, animated only while shown */
class Spinner : public QWidget {
   Q_OBJECT
public:
   explicit Spinner(QWidget *parent = nullptr);

protected:
   void paintEvent(QPaintEvent *event) override;
   void showEvent(QShowEvent *event) override;
   void hideEvent(QHideEvent *event) override;

private:
   QTimer m_timer;
   int m_angle = 0;
};
