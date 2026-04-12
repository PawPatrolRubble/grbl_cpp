#include "StatusPanel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFrame>
#include <QStyle>

StatusPanel::StatusPanel(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("statusPanel");
    setAttribute(Qt::WA_StyledBackground, true);
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);

    // State label — styled via QSS with #stateLabel
    m_stateLabel = new QLabel("Disconnected");
    m_stateLabel->setObjectName("stateLabel");
    m_stateLabel->setAlignment(Qt::AlignCenter);
    m_stateLabel->setProperty("state", "disconnected");
    mainLayout->addWidget(m_stateLabel);

    // Position grid
    auto *posFrame = new QFrame();
    posFrame->setObjectName("posFrame");
    auto *posGrid = new QGridLayout(posFrame);
    posGrid->setSpacing(6);
    posGrid->setContentsMargins(6, 6, 6, 6);

    m_mposX = createAxisLabel("X", "posValueX");
    m_mposY = createAxisLabel("Y", "posValueY");
    m_mposZ = createAxisLabel("Z", "posValueZ");

    m_wposX = createAxisLabel("X", "posValueX");
    m_wposY = createAxisLabel("Y", "posValueY");
    m_wposZ = createAxisLabel("Z", "posValueZ");

    auto *zeroXBtn = createZeroButton("X");
    auto *zeroYBtn = createZeroButton("Y");
    auto *zeroZBtn = createZeroButton("Z");

    auto *mposTitle = new QLabel("Machine Position");
    mposTitle->setObjectName("sectionTitle");
    mposTitle->setAlignment(Qt::AlignCenter);
    posGrid->addWidget(mposTitle, 0, 0, 1, 3);
    posGrid->addWidget(createSideLabel("X:", "sideLabelX"), 1, 0);
    posGrid->addWidget(m_mposX, 1, 1);
    posGrid->addWidget(createSideLabel("Y:", "sideLabelY"), 2, 0);
    posGrid->addWidget(m_mposY, 2, 1);
    posGrid->addWidget(createSideLabel("Z:", "sideLabelZ"), 3, 0);
    posGrid->addWidget(m_mposZ, 3, 1);

    auto *wposTitle = new QLabel("Work Position");
    wposTitle->setObjectName("sectionTitle");
    wposTitle->setAlignment(Qt::AlignCenter);
    posGrid->addWidget(wposTitle, 0, 3, 1, 3);
    posGrid->addWidget(createSideLabel("X:", "sideLabelX"), 1, 3);
    posGrid->addWidget(m_wposX, 1, 4);
    posGrid->addWidget(zeroXBtn, 1, 5);
    posGrid->addWidget(createSideLabel("Y:", "sideLabelY"), 2, 3);
    posGrid->addWidget(m_wposY, 2, 4);
    posGrid->addWidget(zeroYBtn, 2, 5);
    posGrid->addWidget(createSideLabel("Z:", "sideLabelZ"), 3, 3);
    posGrid->addWidget(m_wposZ, 3, 4);
    posGrid->addWidget(zeroZBtn, 3, 5);

    posGrid->setColumnStretch(1, 1);
    posGrid->setColumnStretch(4, 1);
    mainLayout->addWidget(posFrame);

    // Action buttons
    auto *zeroAllBtn = new QPushButton("Zero All (G10 L20 P0 X0 Y0 Z0)");
    zeroAllBtn->setObjectName("zeroAllBtn");
    mainLayout->addWidget(zeroAllBtn);

    auto *goToZeroBtn = new QPushButton("Go to Zero (G90 G0 X0 Y0 Z0)");
    goToZeroBtn->setObjectName("goToZeroBtn");
    mainLayout->addWidget(goToZeroBtn);

    auto *resetOffsetBtn = new QPushButton("Reset Work Offset (G10 L2 P1 X0 Y0 Z0)");
    resetOffsetBtn->setObjectName("resetOffsetBtn");
    resetOffsetBtn->setToolTip("Set work position = machine position (clear G54 offset)");
    mainLayout->addWidget(resetOffsetBtn);

    // Feed / Spindle info
    auto *infoLayout = new QHBoxLayout();
    m_feedLabel = new QLabel("Feed: 0.0 mm/min");
    m_feedLabel->setObjectName("infoLabel");
    m_spindleLabel = new QLabel("Spindle: 0 RPM");
    m_spindleLabel->setObjectName("infoLabel");
    infoLayout->addWidget(m_feedLabel);
    infoLayout->addWidget(m_spindleLabel);
    infoLayout->addStretch();
    mainLayout->addLayout(infoLayout);

    connect(zeroXBtn, &QPushButton::clicked, this, [this]() { emit zeroAxisRequested("X"); });
    connect(zeroYBtn, &QPushButton::clicked, this, [this]() { emit zeroAxisRequested("Y"); });
    connect(zeroZBtn, &QPushButton::clicked, this, [this]() { emit zeroAxisRequested("Z"); });
    connect(zeroAllBtn, &QPushButton::clicked, this, &StatusPanel::zeroAllRequested);
    connect(goToZeroBtn, &QPushButton::clicked, this, &StatusPanel::goToZeroRequested);
    connect(resetOffsetBtn, &QPushButton::clicked, this, &StatusPanel::resetWorkOffsetRequested);
}

QLabel *StatusPanel::createSideLabel(const QString &text, const QString &objectName)
{
    auto *lbl = new QLabel(text);
    lbl->setObjectName(objectName);
    return lbl;
}

QLabel *StatusPanel::createAxisLabel(const QString &axis, const QString &objectName)
{
    auto *lbl = new QLabel(QString("%1: 0.000").arg(axis));
    lbl->setObjectName(objectName);
    lbl->setAlignment(Qt::AlignCenter);
    lbl->setMinimumWidth(80);
    return lbl;
}

QPushButton *StatusPanel::createZeroButton(const QString &axis)
{
    auto *btn = new QPushButton("0");
    btn->setObjectName(QString("zero%1Btn").arg(axis));
    btn->setFixedSize(28, 22);
    btn->setToolTip(QString("Set %1 work position to zero").arg(axis));
    return btn;
}

void StatusPanel::setMachinePosition(const QVector3D &pos)
{
    m_mposX->setText(QString("%1").arg(pos.x(), 0, 'f', 3));
    m_mposY->setText(QString("%1").arg(pos.y(), 0, 'f', 3));
    m_mposZ->setText(QString("%1").arg(pos.z(), 0, 'f', 3));
}

void StatusPanel::setWorkPosition(const QVector3D &pos)
{
    m_wposX->setText(QString("%1").arg(pos.x(), 0, 'f', 3));
    m_wposY->setText(QString("%1").arg(pos.y(), 0, 'f', 3));
    m_wposZ->setText(QString("%1").arg(pos.z(), 0, 'f', 3));
}

void StatusPanel::setState(MachineState::State state)
{
    static const QHash<MachineState::State, QString> stateNames = {
        {MachineState::State::Idle, "idle"},
        {MachineState::State::Run, "running"},
        {MachineState::State::Hold, "hold"},
        {MachineState::State::Home, "homing"},
        {MachineState::State::Alarm, "alarm"},
        {MachineState::State::Jog, "jog"},
        {MachineState::State::Door, "door"},
        {MachineState::State::Check, "check"},
        {MachineState::State::Sleep, "sleep"},
        {MachineState::State::Unknown, "unknown"},
    };
    static const QHash<MachineState::State, QString> displayNames = {
        {MachineState::State::Idle, "Idle"},
        {MachineState::State::Run, "Running"},
        {MachineState::State::Hold, "Hold"},
        {MachineState::State::Home, "Homing"},
        {MachineState::State::Alarm, "ALARM"},
        {MachineState::State::Jog, "Jog"},
        {MachineState::State::Door, "Door"},
        {MachineState::State::Check, "Check"},
        {MachineState::State::Sleep, "Sleep"},
        {MachineState::State::Unknown, "Unknown"},
    };

    const QString key = stateNames.value(state, "unknown");
    m_stateLabel->setText(displayNames.value(state, "Unknown"));
    m_stateLabel->setProperty("state", key);
    // Force QSS re-evaluation
    m_stateLabel->style()->unpolish(m_stateLabel);
    m_stateLabel->style()->polish(m_stateLabel);
}

void StatusPanel::setFeedRate(double rate)
{
    m_feedLabel->setText(QString("Feed: %1 mm/min").arg(rate, 0, 'f', 1));
}

void StatusPanel::setSpindleSpeed(double speed)
{
    m_spindleLabel->setText(QString("Spindle: %1 RPM").arg(speed, 0, 'f', 1));
}