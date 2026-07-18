#include "controltab.h"
#include "privops.h"
#include "schedcatalog.h"
#include "schedulercontroller.h"
#include "scxutils.h"

#include <QGridLayout>
#include <QLabel>
#include <QTimer>
#include <QVBoxLayout>
#include <memory>

ControlTab::ControlTab(QWidget *parent) : QWidget(parent) {
    m_schedCtrl = new SchedulerController(this);

    connect(m_schedCtrl, &SchedulerController::log, this, &ControlTab::log);
    connect(m_schedCtrl, &SchedulerController::statusChanged, this, &ControlTab::statusChanged);
    connect(m_schedCtrl, &SchedulerController::operationInProgress, this,
            &ControlTab::operationInProgress);
    connect(m_schedCtrl, &SchedulerController::operationInProgress, this,
            [this](bool busy) { setControlsEnabled(!busy); });
    connect(m_schedCtrl, &SchedulerController::persistToggled, this, [this](bool enabled) {
        const QSignalBlocker blocker(m_persistCb);
        m_persistCb->setChecked(enabled);
    });

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(8);

    auto *grid = new QGridLayout;
    grid->setSpacing(6);
    grid->setColumnStretch(1, 1);

    grid->addWidget(new QLabel("Scheduler:"), 0, 0);
    m_schedCombo = new QComboBox;
    m_schedCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    grid->addWidget(m_schedCombo, 0, 1);

    grid->addWidget(new QLabel("Mode:"), 1, 0);
    m_modeCombo = new QComboBox;
    m_modeCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    grid->addWidget(m_modeCombo, 1, 1);

    root->addLayout(grid);

    auto *btnRow = new QHBoxLayout;
    m_startBtn = new QPushButton("Apply");
    m_startBtn->setMinimumWidth(80);
    m_startBtn->setMinimumHeight(30);
    m_refreshBtn = new QPushButton("Refresh");
    m_refreshBtn->setMinimumWidth(80);
    m_refreshBtn->setMinimumHeight(30);
    btnRow->addWidget(m_startBtn);
    btnRow->addWidget(m_refreshBtn);
    btnRow->addStretch();
    root->addLayout(btnRow);

    m_persistCb = new QCheckBox("Make default at boot");
    root->addWidget(m_persistCb);

    connect(m_schedCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &ControlTab::onSchedChanged);
    connect(m_startBtn, &QPushButton::clicked, this, &ControlTab::onStartSwitch);
    connect(m_refreshBtn, &QPushButton::clicked, this, &ControlTab::refreshList);
    connect(m_persistCb, &QCheckBox::toggled, this, &ControlTab::onPersistToggled);

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
    auto conn = std::make_shared<QMetaObject::Connection>();

    *conn =
        connect(utils, &ScxUtils::schedulersListed, this, [this, conn](const QStringList &scheds) {
            disconnect(*conn);

            QStringList list = scheds;
            if (list.isEmpty()) {
                emit log("scxctl list returned nothing \xe2\x80\x94 using built-in list");
                list = SchedCatalog::get()->allNames();
            }

            m_schedCombo->blockSignals(true);
            m_modeCombo->blockSignals(true);
            m_schedCombo->clear();
            m_modeCombo->clear();

            auto *cat = SchedCatalog::get();
            for (const QString &bare : list)
                m_schedCombo->addItem(cat->humanize(bare), bare);

            m_schedCombo->blockSignals(false);
            m_modeCombo->blockSignals(false);

            restoreState();
        });

    utils->listSchedulers();
}

void ControlTab::onSchedChanged() {
    const QString bare = m_schedCombo->currentData().toString();
    if (bare.isEmpty())
        return;

    emit schedulerSelected(bare);

    auto *cat = SchedCatalog::get();
    QStringList modes = cat->modes(bare);

    m_modeCombo->blockSignals(true);
    m_modeCombo->clear();
    for (const QString &m : modes)
        m_modeCombo->addItem(cat->humanizeMode(m), m);
    m_modeCombo->blockSignals(false);
}

void ControlTab::onStartSwitch() {
    const QString sched = m_schedCombo->currentData().toString();
    const QString mode = m_modeCombo->currentData().toString();
    m_schedCtrl->start(sched, mode);
}

void ControlTab::onStop() { m_schedCtrl->stop(); }

void ControlTab::onPersistToggled(bool checked) { m_schedCtrl->setPersist(checked); }

void ControlTab::restoreState() {
    const auto [sched, mode] = ScxUtils::loadState();
    if (!sched.isEmpty()) {
        const int i = m_schedCombo->findData(sched);
        if (i >= 0)
            m_schedCombo->setCurrentIndex(i);
    }
    onSchedChanged();
    if (!mode.isEmpty()) {
        const int j = m_modeCombo->findData(mode);
        if (j >= 0)
            m_modeCombo->setCurrentIndex(j);
    }
}

void ControlTab::setControlsEnabled(bool enabled) {
    m_schedCombo->setEnabled(enabled);
    m_modeCombo->setEnabled(enabled);
    m_startBtn->setEnabled(enabled);
    m_refreshBtn->setEnabled(enabled);
    m_persistCb->setEnabled(enabled);
}
