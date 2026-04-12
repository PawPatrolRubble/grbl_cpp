#include "StatusPanel.h"
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>

StatusPanel::StatusPanel(QWidget *parent)
    : QWidget(parent)
{
    auto *group = new QGroupBox("Machine Status");
    auto *mainLayout = new QVBoxLayout(group);

    // State label
    m_stateLabel = new QLabel("Disconnected");
    m_stateLabel->setStyleSheet("font-weight: bold; font-size: 16px; padding: 4px;");
    m_stateLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(m_stateLabel);

    // Position grid
    auto *posGrid = new QGridLayout();
    posGrid->setSpacing(4);

    m_mposX = createAxisLabel("X", "#00e5ff");
    m_mposY = createAxisLabel("Y", "#76ff03");
    m_mposZ = createAxisLabel("Z", "#ff6d00");

    m_wposX = createAxisLabel("X", "#00e5ff");
    m_wposY = createAxisLabel("Y", "#76ff03");
    m_wposZ = createAxisLabel("Z", "#ff6d00");

    auto *zeroXBtn = createZeroButton("X");
    auto *zeroYBtn = createZeroButton("Y");
    auto *zeroZBtn = createZeroButton("Z");

    auto *mposTitle = new QLabel("Machine Position");
    mposTitle->setAlignment(Qt::AlignCenter);
    mposTitle->setStyleSheet("font-weight: bold; color: #aaa;");
    posGrid->addWidget(mposTitle, 0, 0, 1, 3);
    posGrid->addWidget(new QLabel("X:"), 1, 0);
    posGrid->addWidget(m_mposX, 1, 1);
    posGrid->addWidget(new QLabel("Y:"), 2, 0);
    posGrid->addWidget(m_mposY, 2, 1);
    posGrid->addWidget(new QLabel("Z:"), 3, 0);
    posGrid->addWidget(m_mposZ, 3, 1);

    auto *wposTitle = new QLabel("Work Position");
    wposTitle->setAlignment(Qt::AlignCenter);
    wposTitle->setStyleSheet("font-weight: bold; color: #aaa;");
    posGrid->addWidget(wposTitle, 0, 3, 1, 3);
    posGrid->addWidget(new QLabel("X:"), 1, 3);
    posGrid->addWidget(m_wposX, 1, 4);
    posGrid->addWidget(zeroXBtn, 1, 5);
    posGrid->addWidget(new QLabel("Y:"), 2, 3);
    posGrid->addWidget(m_wposY, 2, 4);
    posGrid->addWidget(zeroYBtn, 2, 5);
    posGrid->addWidget(new QLabel("Z:"), 3, 3);
    posGrid->addWidget(m_wposZ, 3, 4);
    posGrid->addWidget(zeroZBtn, 3, 5);

    posGrid->setColumnStretch(1, 1);
    posGrid->setColumnStretch(4, 1);
    mainLayout->addLayout(posGrid);

    // Zero All button
    auto *zeroAllBtn = new QPushButton("Zero All (G10 L20 P0 X0 Y0 Z0)");
    zeroAllBtn->setStyleSheet("QPushButton { padding: 4px; }");
    mainLayout->addWidget(zeroAllBtn);

    // Go to Zero button
    auto *goToZeroBtn = new QPushButton("Go to Zero (G90 G0 X0 Y0 Z0)");
    goToZeroBtn->setStyleSheet("QPushButton { padding: 4px; }");
    mainLayout->addWidget(goToZeroBtn);

    // Feed / Spindle info
    auto *infoLayout = new QHBoxLayout();
    m_feedLabel = new QLabel("Feed: 0.0 mm/min");
    m_spindleLabel = new QLabel("Spindle: 0 RPM");
    m_feedLabel->setStyleSheet("color: #aaa;");
    m_spindleLabel->setStyleSheet("color: #aaa;");
    infoLayout->addWidget(m_feedLabel);
    infoLayout->addWidget(m_spindleLabel);
    infoLayout->addStretch();
    mainLayout->addLayout(infoLayout);

    // Outer layout
    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->addWidget(group);

    // Connections
    connect(zeroXBtn, &QPushButton::clicked, this, [this]() { emit zeroAxisRequested("X"); });
    connect(zeroYBtn, &QPushButton::clicked, this, [this]() { emit zeroAxisRequested("Y"); });
    connect(zeroZBtn, &QPushButton::clicked, this, [this]() { emit zeroAxisRequested("Z"); });
    connect(zeroAllBtn, &QPushButton::clicked, this, &StatusPanel::zeroAllRequested);
    connect(goToZeroBtn, &QPushButton::clicked, this, &StatusPanel::goToZeroRequested);
}

QLabel *StatusPanel::createAxisLabel(const QString &axis, const QString &color)
{
    auto *lbl = new QLabel(QString("%1: 0.000").arg(axis));
    lbl->setStyleSheet(QString(
        "QLabel { background-color: #2b2b2b; color: %1; "
        "font-family: Consolas; font-size: 14px; font-weight: bold; "
        "padding: 4px 8px; border-radius: 3px; }").arg(color));
    lbl->setAlignment(Qt::AlignCenter);
    lbl->setMinimumWidth(100);
    return lbl;
}

QPushButton *StatusPanel::createZeroButton(const QString &axis)
{
    auto *btn = new QPushButton("0");
    btn->setFixedSize(30, 24);
    btn->setToolTip(QString("Set %1 work position to zero").arg(axis));
    btn->setStyleSheet("QPushButton { font-size: 10px; padding: 0; }");
    return btn;
}

void StatusPanel::setMachinePosition(const QVector3D &pos)
{
    m_mposX->setText(QString("X: %1").arg(pos.x(), 0, 'f', 3));
    m_mposY->setText(QString("Y: %1").arg(pos.y(), 0, 'f', 3));
    m_mposZ->setText(QString("Z: %1").arg(pos.z(), 0, 'f', 3));
}

void StatusPanel::setWorkPosition(const QVector3D &pos)
{
    m_wposX->setText(QString("X: %1").arg(pos.x(), 0, 'f', 3));
    m_wposY->setText(QString("Y: %1").arg(pos.y(), 0, 'f', 3));
    m_wposZ->setText(QString("Z: %1").arg(pos.z(), 0, 'f', 3));
}

void StatusPanel::setState(MachineState::State state)
{
    static const QHash<MachineState::State, QString> stateNames = {
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

    QString name = stateNames.value(state, "Unknown");
    m_stateLabel->setText(name);

    if (state == MachineState::State::Alarm)
        m_stateLabel->setStyleSheet("font-weight: bold; font-size: 14px; color: red;");
    else if (state == MachineState::State::Idle)
        m_stateLabel->setStyleSheet("font-weight: bold; font-size: 14px; color: green;");
    else if (state == MachineState::State::Run)
        m_stateLabel->setStyleSheet("font-weight: bold; font-size: 14px; color: blue;");
    else
        m_stateLabel->setStyleSheet("font-weight: bold; font-size: 14px; color: orange;");
}

void StatusPanel::setFeedRate(double rate)
{
    m_feedLabel->setText(QString("%1 mm/min").arg(rate, 0, 'f', 1));
}

void StatusPanel::setSpindleSpeed(double speed)
{
    m_spindleLabel->setText(QString("%1 RPM").arg(speed, 0, 'f', 1));
}
