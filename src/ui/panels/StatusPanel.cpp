#include "StatusPanel.h"
#include "ui_StatusPanel.h"
#include <QStyle>
#include <QRegularExpressionValidator>

StatusPanel::StatusPanel(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::StatusPanel)
{
    ui->setupUi(this);
    setObjectName("statusPanel");
    setAttribute(Qt::WA_StyledBackground, true);

    // Set initial state property for QSS
    ui->stateLabel->setProperty("state", "disconnected");

    // Validator for set-position line edits
    QRegularExpression rx(R"(^-?\d*\.?\d*$)");
    ui->setXEdit->setValidator(new QRegularExpressionValidator(rx, this));
    ui->setYEdit->setValidator(new QRegularExpressionValidator(rx, this));
    ui->setZEdit->setValidator(new QRegularExpressionValidator(rx, this));

    // Connections
    connect(ui->zeroXBtn, &QPushButton::clicked, this, [this]() { emit zeroAxisRequested("X"); });
    connect(ui->zeroYBtn, &QPushButton::clicked, this, [this]() { emit zeroAxisRequested("Y"); });
    connect(ui->zeroZBtn, &QPushButton::clicked, this, [this]() { emit zeroAxisRequested("Z"); });

    connect(ui->setXBtn, &QPushButton::clicked, this, [this]() {
        emit setAxisPositionRequested("X", ui->setXEdit->text().isEmpty() ? 0.0 : ui->setXEdit->text().toDouble());
    });
    connect(ui->setYBtn, &QPushButton::clicked, this, [this]() {
        emit setAxisPositionRequested("Y", ui->setYEdit->text().isEmpty() ? 0.0 : ui->setYEdit->text().toDouble());
    });
    connect(ui->setZBtn, &QPushButton::clicked, this, [this]() {
        emit setAxisPositionRequested("Z", ui->setZEdit->text().isEmpty() ? 0.0 : ui->setZEdit->text().toDouble());
    });

    connect(ui->zeroAllBtn, &QPushButton::clicked, this, &StatusPanel::zeroAllRequested);
    connect(ui->goToZeroBtn, &QPushButton::clicked, this, &StatusPanel::goToZeroRequested);
    connect(ui->resetOffsetBtn, &QPushButton::clicked, this, &StatusPanel::resetWorkOffsetRequested);
    connect(ui->eStopBtn, &QPushButton::clicked, this, &StatusPanel::emergencyStopRequested);
    connect(ui->homeBtn, &QPushButton::clicked, this, &StatusPanel::homeMachineRequested);
}

StatusPanel::~StatusPanel()
{
    delete ui;
}

void StatusPanel::setMachinePosition(const QVector3D &pos)
{
    ui->mposX->setText(QString("%1").arg(pos.x(), 0, 'f', 3));
    ui->mposY->setText(QString("%1").arg(pos.y(), 0, 'f', 3));
    ui->mposZ->setText(QString("%1").arg(pos.z(), 0, 'f', 3));
}

void StatusPanel::setWorkPosition(const QVector3D &pos)
{
    ui->wposX->setText(QString("%1").arg(pos.x(), 0, 'f', 3));
    ui->wposY->setText(QString("%1").arg(pos.y(), 0, 'f', 3));
    ui->wposZ->setText(QString("%1").arg(pos.z(), 0, 'f', 3));
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
    ui->stateLabel->setText(displayNames.value(state, "Unknown"));
    ui->stateLabel->setProperty("state", key);
    ui->stateLabel->style()->unpolish(ui->stateLabel);
    ui->stateLabel->style()->polish(ui->stateLabel);
}

void StatusPanel::setFeedRate(double rate)
{
    ui->feedLabel->setText(QString("Feed: %1 mm/min").arg(rate, 0, 'f', 1));
}

void StatusPanel::setSpindleSpeed(double speed)
{
    ui->spindleLabel->setText(QString("Spindle: %1 RPM").arg(speed, 0, 'f', 1));
}

void StatusPanel::setMessage(const QString &msg)
{
    if (msg.isEmpty()) {
        ui->messageLabel->setVisible(false);
    } else {
        ui->messageLabel->setText(msg);
        ui->messageLabel->setVisible(true);
    }
}

void StatusPanel::setMotionEnabled(bool enabled)
{
    ui->zeroXBtn->setEnabled(enabled);
    ui->zeroYBtn->setEnabled(enabled);
    ui->zeroZBtn->setEnabled(enabled);
    ui->zeroAllBtn->setEnabled(enabled);
    ui->goToZeroBtn->setEnabled(enabled);
    ui->resetOffsetBtn->setEnabled(enabled);
}