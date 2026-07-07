#pragma once

#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QWidget>

class ControlTab : public QWidget {
    Q_OBJECT
    public:
    explicit ControlTab(QWidget *parent = nullptr);

    void stopScheduler();

    signals:
    void log(const QString &msg);
    void statusChanged();
    void operationInProgress(bool inFlight);
    void schedulerSelected(const QString &bare);

    private slots:
    void onSchedChanged();
    void onStartSwitch();
    void onStop();
    void onPersistToggled(bool checked);
    void refreshList();

    private:
    void restoreState();
    void setControlsEnabled(bool enabled);

    QComboBox *m_schedCombo = nullptr;
    QComboBox *m_modeCombo = nullptr;
    QPushButton *m_startBtn = nullptr;
    QPushButton *m_refreshBtn = nullptr;
    QCheckBox *m_persistCb = nullptr;
};
