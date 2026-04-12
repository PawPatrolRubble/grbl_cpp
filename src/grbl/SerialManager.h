#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QQueue>
#include <QTimer>
#include <QThread>
#include <QMutex>

class MachineState;

class SerialManager : public QObject
{
    Q_OBJECT

public:
    explicit SerialManager(MachineState *machineState, QObject *parent = nullptr);
    ~SerialManager();

    static QStringList availablePorts();
    static QStringList commonBaudRates();

    bool connectToGrbl(const QString &portName, int baudRate);
    void disconnectFromGrbl();
    bool isConnected() const;

    void sendCommand(const QString &command);
    void sendRealtimeCommand(char command);

    void jogStep(const QString &axis, double step, double feedRate);
    void jogContinuous(const QString &axis, int direction, double feedRate);
    void jogCancel();

signals:
    void connectedChanged(bool connected);
    void responseReceived(const QString &response);
    void ackReceived();
    void commandErrorReceived(const QString &error);
    void errorOccurred(const QString &error);
    void commandSent(const QString &command);

private slots:
    void handleReadData();
    void pollStatus();

private:
    void processBuffer();

    MachineState *m_machineState;
    QString m_buffer;

    void *m_handle;
    mutable QMutex m_handleMutex;
    int m_baudRate = 115200;
    bool m_grblReady = false;

    QThread m_ioThread;
    QTimer *m_readTimer = nullptr;
    QTimer *m_pollTimer = nullptr;
};