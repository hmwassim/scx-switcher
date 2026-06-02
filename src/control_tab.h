#pragma once

#include <QWidget>
#include <QComboBox>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>

class ControlTab : public QWidget {
    Q_OBJECT
public:
    explicit ControlTab(QWidget *parent = nullptr);
    void refreshSchedulerList();
    void stopScheduler();

signals:
    void logMessage(const QString &msg);
    void statusRefreshRequested();

private slots:
    void onSchedChanged();
    void onStartSwitch();
    void onStop();
    void onPersistToggled(bool checked);

private:
    QComboBox *m_schedCombo = nullptr;
    QComboBox *m_modeCombo = nullptr;
    QPushButton *m_startSwitchBtn = nullptr;
    QPushButton *m_refreshBtn = nullptr;
    QCheckBox *m_persistCb = nullptr;
    bool m_ignorePersist = false;
};
