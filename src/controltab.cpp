#include "controltab.h"
#include "scxutils.h"
#include "privops.h"
#include "config.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QMessageBox>
#include <QTimer>
#include <memory>

ControlTab::ControlTab(QWidget *parent) : QWidget(parent) {
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(10);

    auto *selGroup = new QGroupBox("Scheduler");
    auto *grid     = new QGridLayout(selGroup);
    grid->setSpacing(8);
    grid->setColumnStretch(1, 1);

    grid->addWidget(new QLabel("Scheduler:"), 0, 0);
    m_schedCombo = new QComboBox;
    m_schedCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    grid->addWidget(m_schedCombo, 0, 1);

    grid->addWidget(new QLabel("Mode:"), 1, 0);
    m_modeCombo = new QComboBox;
    m_modeCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    grid->addWidget(m_modeCombo, 1, 1);

    root->addWidget(selGroup);

    auto *btnRow = new QHBoxLayout;
    m_startBtn   = new QPushButton("Start / Switch");
    m_refreshBtn = new QPushButton("Refresh List");
    m_startBtn->setMinimumHeight(34);
    m_refreshBtn->setMinimumHeight(34);
    btnRow->addWidget(m_startBtn);
    btnRow->addWidget(m_refreshBtn);
    root->addLayout(btnRow);

    m_persistCb = new QCheckBox("Auto-apply last scheduler at boot");
    root->addWidget(m_persistCb);

    root->addStretch();

    connect(m_schedCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ControlTab::onSchedChanged);
    connect(m_startBtn,   &QPushButton::clicked, this, &ControlTab::onStartSwitch);
    connect(m_refreshBtn, &QPushButton::clicked, this, &ControlTab::refreshList);
    connect(m_persistCb,  &QCheckBox::toggled,   this, &ControlTab::onPersistToggled);

    refreshList();

    auto *utils = ScxUtils::get();
    connect(utils, &ScxUtils::serviceEnabled, this, [this](bool enabled) {
        const QSignalBlocker blocker(m_persistCb);
        m_persistCb->setChecked(enabled);
    });
    QTimer::singleShot(0, utils, &ScxUtils::checkServiceEnabled);
}

void ControlTab::stopScheduler() { onStop(); }

void ControlTab::refreshList() {
    auto *utils = ScxUtils::get();
    auto  conn  = std::make_shared<QMetaObject::Connection>();

    *conn = connect(utils, &ScxUtils::schedulersListed, this,
        [this, conn](const QStringList &scheds) {
            disconnect(*conn);

            QStringList list = scheds;
            if (list.isEmpty()) {
                emit log("scxctl list returned nothing \xe2\x80\x94 using built-in list");
                for (const auto &si : ALL_SCHEDULERS)
                    list << si.bare;
            }

            m_schedCombo->blockSignals(true);
            m_modeCombo->blockSignals(true);
            m_schedCombo->clear();
            m_modeCombo->clear();

            for (const QString &bare : list)
                m_schedCombo->addItem(humanizeSched(bare), bare);

            m_schedCombo->blockSignals(false);
            m_modeCombo->blockSignals(false);

            restoreState();
        });

    utils->listSchedulers();
}

void ControlTab::onSchedChanged() {
    const QString bare = m_schedCombo->currentData().toString();
    if (bare.isEmpty()) return;

    static QHash<QString, QStringList> modeMap;
    if (modeMap.isEmpty()) {
        for (const auto &si : ALL_SCHEDULERS)
            modeMap.insert(si.bare, si.modes);
    }

    QStringList modes = modeMap.value(bare);
    if (modes.isEmpty()) modes = {"auto"};

    m_modeCombo->blockSignals(true);
    m_modeCombo->clear();
    for (const QString &m : modes)
        m_modeCombo->addItem(humanizeMode(m), m);
    m_modeCombo->blockSignals(false);
}

void ControlTab::onStartSwitch() {
    const QString sched = m_schedCombo->currentData().toString();
    if (sched.isEmpty()) {
        QMessageBox::warning(this, "No scheduler selected",
                             "Please select a scheduler first.");
        return;
    }
    const QString mode = m_modeCombo->currentData().toString();

    if (!PrivOps::pkexecPresent()) {
        QMessageBox::warning(this, "pkexec not found", ERR_NO_POLKIT);
        return;
    }

    setControlsEnabled(false);

    auto *utils = ScxUtils::get();
    auto  conn  = std::make_shared<QMetaObject::Connection>();

    *conn = connect(utils, &ScxUtils::statusReady, this,
        [this, conn, sched, mode](const SchedStatus &cur) {
            disconnect(*conn);

            if (cur.active && cur.name == sched && cur.mode == mode) {
                ScxUtils::saveState(sched, mode);
                emit log(QString("%1 (%2) is already running")
                         .arg(humanizeSched(sched), humanizeMode(mode)));
                setControlsEnabled(true);
                return;
            }

            auto onDone = [this, sched, mode](bool ok, const QString &msg) {
                if (ok) {
                    ScxUtils::saveState(sched, mode);
                    emit log(QString("Now running %1 (%2)")
                             .arg(humanizeSched(sched), humanizeMode(mode)));
                    PrivOps::get()->writeConfig(sched, mode);
                } else {
                    emit log(msg.isEmpty() ? ERR_SWITCH_FAILED
                                            : QString("Failed: %1").arg(msg));
                }
                setControlsEnabled(true);
                emit statusChanged();
            };

            if (!cur.active) {
                emit log(QString("Starting %1 (%2)\xe2\x80\xa6")
                         .arg(humanizeSched(sched), humanizeMode(mode)));
                PrivOps::get()->startScheduler(sched, mode, onDone);
            } else {
                emit log(QString("Switching to %1 (%2)\xe2\x80\xa6")
                         .arg(humanizeSched(sched), humanizeMode(mode)));
                PrivOps::get()->switchScheduler(sched, mode, onDone);
            }
        });

    utils->queryStatus();
}

void ControlTab::onStop() {
    if (!PrivOps::pkexecPresent()) {
        QMessageBox::warning(this, "pkexec not found", ERR_NO_POLKIT);
        return;
    }

    emit log("Stopping scheduler\xe2\x80\xa6");
    setControlsEnabled(false);

    PrivOps::get()->stopScheduler([this](bool ok, const QString &msg) {
        emit log(ok ? "Stopped \xe2\x80\x94 back to EEVDF"
                    : (msg.isEmpty() ? ERR_STOP_FAILED
                                     : QString("Failed: %1").arg(msg)));
        setControlsEnabled(true);
        emit statusChanged();
    });
}

void ControlTab::onPersistToggled(bool checked) {
    if (!PrivOps::pkexecPresent()) {
        QMessageBox::warning(this, "pkexec not found", ERR_NO_POLKIT);
        const QSignalBlocker blocker(m_persistCb);
        m_persistCb->setChecked(!checked);
        return;
    }

    auto onDone = [this, checked](bool ok, const QString &msg) {
        if (ok) {
            emit log(checked
                ? "Auto-start enabled \xe2\x80\x94 scheduler will apply at next boot"
                : "Auto-start disabled");
        } else {
            const char *canned = checked ? ERR_PERSIST_ENABLE : ERR_PERSIST_DISABLE;
            emit log(msg.isEmpty() ? canned : QString("Failed: %1").arg(msg));
            const QSignalBlocker blocker(m_persistCb);
            m_persistCb->setChecked(!checked);
        }
    };

    if (checked) {
        emit log("Enabling auto-start\xe2\x80\xa6");
        PrivOps::get()->enableService(onDone);
    } else {
        emit log("Disabling auto-start\xe2\x80\xa6");
        PrivOps::get()->disableService(onDone);
    }
}

void ControlTab::restoreState() {
    const auto [sched, mode] = ScxUtils::loadState();
    if (!sched.isEmpty()) {
        const int i = m_schedCombo->findData(sched);
        if (i >= 0) m_schedCombo->setCurrentIndex(i);
    }
    onSchedChanged();
    if (!mode.isEmpty()) {
        const int j = m_modeCombo->findData(mode);
        if (j >= 0) m_modeCombo->setCurrentIndex(j);
    }
}

void ControlTab::setControlsEnabled(bool enabled) {
    m_schedCombo->setEnabled(enabled);
    m_modeCombo->setEnabled(enabled);
    m_startBtn->setEnabled(enabled);
    m_persistCb->setEnabled(enabled);
    emit operationInProgress(!enabled);
}
