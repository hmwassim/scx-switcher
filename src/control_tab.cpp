#include "control_tab.h"
#include "scx_utils.h"
#include "priv_ops.h"
#include "config.h"

#include <memory>
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

    auto *utils = scx_utils::instance();
    connect(utils, &ScxUtils::serviceEnabledChecked, this, [this](bool enabled) {
        m_ignorePersist = true;
        m_persistCb->setChecked(enabled);
        m_ignorePersist = false;
    });
    QTimer::singleShot(0, utils, [utils]() {
        utils->checkServiceEnabled();
    });
}

void ControlTab::refreshSchedulerList() {
    auto *utils = scx_utils::instance();
    auto conn = std::make_shared<QMetaObject::Connection>();
    *conn = connect(utils, &ScxUtils::schedulersListed, this,
        [this, conn](const QStringList &scheds) {
            disconnect(*conn);

            m_schedCombo->blockSignals(true);
            m_modeCombo->blockSignals(true);

            m_schedCombo->clear();
            m_modeCombo->clear();

            QStringList list = scheds;
            if (list.isEmpty()) {
                emit logMessage("scxctl list failed, using fallback list");
                list = {"bpfland", "lavd", "rusty", "flash", "layered",
                        "cosmos", "nest", "p2dq", "simple", "tickless",
                        "beerland", "rustland"};
            }

            for (const auto &s : list) {
                QString bare = s;
                if (bare.startsWith("scx_"))
                    bare = bare.mid(4);
                m_schedCombo->addItem(ScxUtils::humanizeScheduler("scx_" + bare), bare);
            }

            auto state = ScxUtils::loadState();
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
        });

    utils->listSchedulers();
}

void ControlTab::setKernelUnsupported(bool unsupported) {
    m_schedCombo->setEnabled(!unsupported);
    m_modeCombo->setEnabled(!unsupported);
    m_startSwitchBtn->setEnabled(!unsupported);
    m_refreshBtn->setEnabled(!unsupported);
    m_persistCb->setEnabled(!unsupported);
    if (unsupported) {
        m_schedCombo->setToolTip("Kernel does not support sched-ext");
        m_modeCombo->setToolTip("Kernel does not support sched-ext");
        m_startSwitchBtn->setToolTip("Kernel does not support sched-ext");
        m_refreshBtn->setToolTip("Kernel does not support sched-ext");
        m_persistCb->setToolTip("Kernel does not support sched-ext");
    } else {
        m_schedCombo->setToolTip({});
        m_modeCombo->setToolTip({});
        m_startSwitchBtn->setToolTip({});
        m_refreshBtn->setToolTip({});
        m_persistCb->setToolTip({});
    }
}

void ControlTab::onSchedChanged() {
    QString bare = m_schedCombo->currentData().toString();
    if (bare.isEmpty()) return;

    m_modeCombo->blockSignals(true);
    m_modeCombo->clear();
    QStringList modes = ScxUtils::supportedModes(bare);
    for (const auto &m : modes)
        m_modeCombo->addItem(ScxUtils::humanizeMode(m), m);
    m_modeCombo->blockSignals(false);
}

void ControlTab::onStartSwitch() {
    QString sched = m_schedCombo->currentData().toString();
    if (sched.isEmpty()) {
        QMessageBox::warning(this, "Error", ERROR_SCHED_NOT_INSTALLED);
        return;
    }
    QString mode = m_modeCombo->currentData().toString();

    ScxUtils::saveState(sched, mode);

    if (!PrivOps::isPolkitAgentRunning()) {
        QMessageBox::warning(this, "PolKit Agent Not Found", ERROR_NO_POLKIT);
        return;
    }

    auto *utils = scx_utils::instance();
    auto conn = std::make_shared<QMetaObject::Connection>();
    *conn = connect(utils, &ScxUtils::schedulerStatusReady, this,
        [this, conn, sched, mode](const SchedStatus &current) {
            disconnect(*conn);

            if (current.active && current.name == sched && current.mode == mode) {
                emit logMessage(QString("%1 (%2) is already running").arg(sched, mode));
                return;
            }

            auto onSchedOp = [this, sched, mode](bool ok, const QString &) {
                if (ok) {
                    emit logMessage("Done");
                    PrivOps::instance()->writeConfigToml(sched, mode);
                } else {
                    emit logMessage(ERROR_SWITCH_FAILED);
                }
                emit statusRefreshRequested();
            };

            if (!current.active) {
                emit logMessage(QString("Starting %1 (%2)...").arg(sched, mode));
                PrivOps::instance()->startScheduler(sched, mode, std::move(onSchedOp));
            } else {
                emit logMessage(QString("Switching to %1 (%2)...").arg(sched, mode));
                PrivOps::instance()->switchScheduler(sched, mode, std::move(onSchedOp));
            }
        });

    utils->getSchedulerStatus();
}

void ControlTab::stopScheduler() {
    onStop();
}

void ControlTab::onStop() {
    if (!PrivOps::isPolkitAgentRunning()) {
        QMessageBox::warning(this, "PolKit Agent Not Found", ERROR_NO_POLKIT);
        return;
    }
    emit logMessage("Stopping scheduler...");
    PrivOps::instance()->stopScheduler([this](bool ok, const QString &) {
        if (ok)
            emit logMessage("Stopped \u2014 back to EEVDF");
        else
            emit logMessage(ERROR_STOP_FAILED);
        emit statusRefreshRequested();
    });
}

void ControlTab::onPersistToggled(bool checked) {
    if (m_ignorePersist) return;

    if (!PrivOps::isPolkitAgentRunning()) {
        QMessageBox::warning(this, "PolKit Agent Not Found", ERROR_NO_POLKIT);
        m_ignorePersist = true;
        m_persistCb->setChecked(!checked);
        m_ignorePersist = false;
        return;
    }

    auto onDone = [this, checked](bool ok, const QString &) {
        if (checked) {
            if (ok)
                emit logMessage("Enabled \u2014 last scheduler will apply at next boot");
            else {
                emit logMessage("Failed to enable: " + ERROR_PERSIST_FAILED_ENABLE);
                m_ignorePersist = true;
                m_persistCb->setChecked(false);
                m_ignorePersist = false;
            }
        } else {
            if (ok)
                emit logMessage("Disabled \u2014 boot stays on EEVDF");
            else {
                emit logMessage("Failed to disable: " + ERROR_PERSIST_FAILED_DISABLE);
                m_ignorePersist = true;
                m_persistCb->setChecked(true);
                m_ignorePersist = false;
            }
        }
    };

    if (checked) {
        emit logMessage("Enabling auto-start on boot...");
        PrivOps::instance()->enableService(std::move(onDone));
    } else {
        emit logMessage("Disabling auto-start on boot...");
        PrivOps::instance()->disableService(std::move(onDone));
    }
}
