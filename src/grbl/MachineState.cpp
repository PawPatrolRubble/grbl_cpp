#include "MachineState.h"

MachineState::MachineState(QObject *parent)
    : QObject(parent)
{
}

MachineState::State MachineState::state() const { return m_state; }
MachineState::SubState MachineState::subState() const { return m_subState; }
QVector3D MachineState::machinePosition() const { return m_machinePos; }
QVector3D MachineState::workPosition() const { return m_workPos; }
QVector3D MachineState::wco() const { return m_wco; }
double MachineState::feedRate() const { return m_feedRate; }
double MachineState::spindleSpeed() const { return m_spindleSpeed; }
bool MachineState::isConnected() const { return m_connected; }
bool MachineState::isAlarm() const { return m_state == State::Alarm; }

void MachineState::setState(State state)
{
    if (m_state != state) {
        m_state = state;
        emit stateChanged(state);
        if (state == State::Alarm)
            emit alarmChanged(true);
        else
            emit alarmChanged(false);
    }
}

void MachineState::setSubState(SubState subState)
{
    if (m_subState != subState) {
        m_subState = subState;
        emit subStateChanged(subState);
    }
}

void MachineState::setMachinePosition(const QVector3D &pos)
{
    if (m_machinePos != pos) {
        m_machinePos = pos;
        emit machinePositionChanged(pos);
    }
}

void MachineState::setWorkPosition(const QVector3D &pos)
{
    if (m_workPos != pos) {
        m_workPos = pos;
        emit workPositionChanged(pos);
    }
}

void MachineState::setWco(const QVector3D &wco)
{
    if (m_wco != wco) {
        m_wco = wco;
        emit wcoChanged(wco);
    }
}

void MachineState::setFeedRate(double rate)
{
    if (!qFuzzyCompare(m_feedRate, rate)) {
        m_feedRate = rate;
        emit feedRateChanged(rate);
    }
}

void MachineState::setSpindleSpeed(double speed)
{
    if (!qFuzzyCompare(m_spindleSpeed, speed)) {
        m_spindleSpeed = speed;
        emit spindleSpeedChanged(speed);
    }
}

void MachineState::setConnected(bool connected)
{
    if (m_connected != connected) {
        m_connected = connected;
        emit connectedChanged(connected);
    }
}

void MachineState::setMessage(const QString &msg)
{
    emit messageReceived(msg);
}

void MachineState::setGrblSetting(int id, double value)
{
    m_grblSettings[id] = value;
    emit grblSettingsChanged(m_grblSettings);
}

QMap<int, double> MachineState::grblSettings() const
{
    return m_grblSettings;
}

void MachineState::reset()
{
    setState(State::Unknown);
    setSubState(SubState::None);
    setMachinePosition(QVector3D());
    setWorkPosition(QVector3D());
    setWco(QVector3D());
    setFeedRate(0.0);
    setSpindleSpeed(0.0);
}