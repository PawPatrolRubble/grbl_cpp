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
    QVector3D wco() const;
    double feedRate() const;
    double spindleSpeed() const;
    bool isConnected() const;
    bool isAlarm() const;

    void setState(State state);
    void setSubState(SubState subState);
    void setMachinePosition(const QVector3D &pos);
    void setWorkPosition(const QVector3D &pos);
    void setWco(const QVector3D &wco);
    void setFeedRate(double rate);
    void setSpindleSpeed(double speed);
    void setConnected(bool connected);
    void setMessage(const QString &msg);

    void reset();

signals:
    void stateChanged(State state);
    void subStateChanged(SubState subState);
    void machinePositionChanged(const QVector3D &pos);
    void workPositionChanged(const QVector3D &pos);
    void wcoChanged(const QVector3D &wco);
    void feedRateChanged(double rate);
    void spindleSpeedChanged(double speed);
    void connectedChanged(bool connected);
    void alarmChanged(bool alarm);
    void messageReceived(const QString &msg);

private:
    State m_state = State::Unknown;
    SubState m_subState = SubState::None;
    QVector3D m_machinePos;
    QVector3D m_workPos;
    QVector3D m_wco;
    double m_feedRate = 0.0;
    double m_spindleSpeed = 0.0;
    bool m_connected = false;
};