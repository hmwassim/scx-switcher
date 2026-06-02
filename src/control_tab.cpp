#include "control_tab.h"
#include "scx_utils.h"
#include "priv_ops.h"

#include <QVBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QFrame>
#include <QMessageBox>
#include <QTimer>

ControlTab::ControlTab(QWidget *parent) : QWidget(parent) {
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);

    auto *group = new QGroupBox("Scheduler Selection");
    auto *grid = new QGridLayout(group);
    grid->setSpacing(8);

    grid->addWidget(new QLabel("Scheduler:"), 0, 0);
    m_schedCombo = new QComboBox;
    m_schedCombo->setMinimumWidth(280);
    grid->addWidget(m_schedCombo, 0, 1);

    grid->addWidget(new QLabel("Mode:"), 1, 0);
    m_modeCombo = new QComboBox;
    m_modeCombo->setMinimumWidth(280);
    grid->addWidget(m_modeCombo, 1, 1);

    layout->addWidget(group);

    auto *btnFrame = new QFrame;
    auto *btnRow = new QHBoxLayout(btnFrame);
    btnRow->setContentsMargins(0, 0, 0, 0);

    m_startSwitchBtn = new QPushButton("Start / Switch");
    m_refreshBtn = new QPushButton("Refresh List");

    for (auto *b : {m_startSwitchBtn, m_refreshBtn}) {
        b->setMinimumHeight(34);
        btnRow->addWidget(b);
    }

    connect(m_startSwitchBtn, &QPushButton::clicked, this, &ControlTab::onStartSwitch);
    connect(m_refreshBtn, &QPushButton::clicked, this, &ControlTab::refreshSchedulerList);
    connect(m_schedCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ControlTab::onSchedChanged);

    layout->addWidget(btnFrame);

    m_persistCb = new QCheckBox("Auto-apply last scheduler at boot");
    connect(m_persistCb, &QCheckBox::toggled, this, &ControlTab::onPersistToggled);
    layout->addWidget(m_persistCb);

    layout->addStretch();

    refreshSchedulerList();

    QTimer::singleShot(0, this, [this]() {
        m_ignorePersist = true;
        m_persistCb->setChecked(scx_utils::isServiceEnabled());
        m_ignorePersist = false;
    });
}

void ControlTab::refreshSchedulerList() {
    m_schedCombo->blockSignals(true);
    m_modeCombo->blockSignals(true);

    m_schedCombo->clear();
    m_modeCombo->clear();

    QStringList scheds = scx_utils::listSchedulers();
    if (scheds.isEmpty()) {
        emit logMessage("scxctl list failed, using fallback list");
        scheds = {"bpfland", "lavd", "rusty", "flash", "layered",
                  "cosmos", "nest", "p2dq", "simple", "tickless"};
    }

    for (const auto &s : scheds) {
        QString bare = s;
        if (bare.startsWith("scx_"))
            bare = bare.mid(4);
        m_schedCombo->addItem(scx_utils::humanizeScheduler("scx_" + bare), bare);
    }

    auto state = scx_utils::loadState();
    if (!state.first.isEmpty()) {
        int idx = m_schedCombo->findData(state.first);
        if (idx >= 0)
            m_schedCombo->setCurrentIndex(idx);
    }

    m_schedCombo->blockSignals(false);
    onSchedChanged();

    if (!state.second.isEmpty()) {
        int modeIdx = m_modeCombo->findData(state.second);
        if (modeIdx >= 0)
            m_modeCombo->setCurrentIndex(modeIdx);
    }

    m_modeCombo->blockSignals(false);
}

void ControlTab::onSchedChanged() {
    QString bare = m_schedCombo->currentData().toString();
    if (bare.isEmpty()) return;

    m_modeCombo->blockSignals(true);
    m_modeCombo->clear();
    QStringList modes = scx_utils::supportedModes(bare);
    for (const auto &m : modes)
        m_modeCombo->addItem(scx_utils::humanizeMode(m), m);
    m_modeCombo->blockSignals(false);
}

void ControlTab::onStartSwitch() {
    QString sched = m_schedCombo->currentData().toString();
    if (sched.isEmpty()) {
        QMessageBox::warning(this, "Error", "Select a scheduler");
        return;
    }
    QString mode = m_modeCombo->currentData().toString();

    scx_utils::saveState(sched, mode);

    if (!priv_ops::isPolkitAgentRunning()) {
        QMessageBox::warning(this, "PolKit Agent Not Found",
            "No PolKit authentication agent is running.\n\n"
            "Run this command in a terminal:\n"
            "  pkexec scxctl start --sched " + sched + " --mode " + mode);
        return;
    }

    auto current = scx_utils::getSchedulerStatus();
    bool ok = true;

    if (!current.active) {
        emit logMessage(QString("Starting %1 (%2)...").arg(sched, mode));
        ok = priv_ops::startScheduler(sched, mode);
    } else if (current.name == sched && current.mode == mode) {
        emit logMessage(QString("%1 (%2) is already running").arg(sched, mode));
        return;
    } else {
        emit logMessage(QString("Switching to %1 (%2)...").arg(sched, mode));
        ok = priv_ops::switchScheduler(sched, mode);
    }

    if (ok) {
        emit logMessage("Done");
        priv_ops::writeConfigToml(sched, mode);
    } else {
        emit logMessage("Failed \u2014 check PolKit authorization");
    }
    emit statusRefreshRequested();
}

void ControlTab::stopScheduler() {
    onStop();
}

void ControlTab::onStop() {
    if (!priv_ops::isPolkitAgentRunning()) {
        QMessageBox::warning(this, "PolKit Agent Not Found",
            "No PolKit authentication agent is running.\n\n"
            "Run this in a terminal:\n  pkexec scxctl stop");
        return;
    }
    emit logMessage("Stopping scheduler...");
    if (priv_ops::stopScheduler())
        emit logMessage("Stopped \u2014 back to EEVDF");
    else
        emit logMessage("Failed to stop \u2014 check PolKit authorization");
    emit statusRefreshRequested();
}

void ControlTab::onPersistToggled(bool checked) {
    if (m_ignorePersist) return;

    if (!priv_ops::isPolkitAgentRunning()) {
        QMessageBox::warning(this, "PolKit Agent Not Found",
            "No PolKit authentication agent is running.\n\n"
            "Run this in a terminal:\n  pkexec systemctl " +
            QString(checked ? "enable" : "disable") + " --now scx_loader.service");
        m_ignorePersist = true;
        m_persistCb->setChecked(!checked);
        m_ignorePersist = false;
        return;
    }

    if (checked) {
        emit logMessage("Enabling auto-start on boot...");
        if (priv_ops::enableService())
            emit logMessage("Enabled \u2014 last scheduler will apply at next boot");
        else {
            emit logMessage("Failed to enable");
            m_ignorePersist = true;
            m_persistCb->setChecked(false);
            m_ignorePersist = false;
        }
    } else {
        emit logMessage("Disabling auto-start on boot...");
        if (priv_ops::disableService())
            emit logMessage("Disabled \u2014 boot stays on EEVDF");
        else {
            emit logMessage("Failed to disable");
            m_ignorePersist = true;
            m_persistCb->setChecked(true);
            m_ignorePersist = false;
        }
    }
}
