#pragma once

#include <QObject>
#include <QVector3D>

class MachineState : public QObject
{
    Q_OBJECT

public:
    enum class State
    {
        Idle,
        Run,
        Hold,
        Home,
        Alarm,
        Jog,
        Door,
        Check,
        Sleep,
        Unknown
    };
    Q_ENUM(State)

    enum class SubState
    {
        None,
        Hold0,
        Hold1,
        Door0,
        Door1,
        Door2,
        Door3
    };
    Q_ENUM(SubState)

    explicit MachineState(QObject *parent = nullptr);

    State state() const;
    SubState subState() const;
    QVector3D machinePosition() const;
    QVector3D workPosition() const;
    double feedRate() const;
    double spindleSpeed() const;
    bool isConnected() const;
    bool isAlarm() const;

    void setState(State state);
    void setSubState(SubState subState);
    void setMachinePosition(const QVector3D &pos);
    void setWorkPosition(const QVector3D &pos);
    void setFeedRate(double rate);
    void setSpindleSpeed(double speed);
    void setConnected(bool connected);

    void reset();

signals:
    void stateChanged(State state);
    void subStateChanged(SubState subState);
    void machinePositionChanged(const QVector3D &pos);
    void workPositionChanged(const QVector3D &pos);
    void feedRateChanged(double rate);
    void spindleSpeedChanged(double speed);
    void connectedChanged(bool connected);
    void alarmChanged(bool alarm);

private:
    State m_state = State::Unknown;
    SubState m_subState = SubState::None;
    QVector3D m_machinePos;
    QVector3D m_workPos;
    double m_feedRate = 0.0;
    double m_spindleSpeed = 0.0;
    bool m_connected = false;
};