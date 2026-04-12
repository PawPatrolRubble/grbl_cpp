#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QTimer>
#include <QMutex>
#include <QByteArray>

class MachineState;

/// Cross-platform serial communication with GRBL devices.
/// Uses Qt's QSerialPort for async I/O instead of platform-specific APIs.
class SerialManager : public QObject
{
    Q_OBJECT

public:
    explicit SerialManager(MachineState *machineState, QObject *parent = nullptr);
    ~SerialManager() override;

    /// Get list of available serial ports with descriptions.
    static QStringList availablePorts();
    static QStringList commonBaudRates();

    /// Connect to a GRBL device. Returns true on success.
    bool connectToGrbl(const QString &portName, int baudRate);

    /// Disconnect from the GRBL device.
    void disconnectFromGrbl();

    bool isConnected() const noexcept;

    /// Send a line command (e.g., "G0 X0 Y0", "$H").
    void sendCommand(const QString &command);

    /// Send a single-byte realtime command (e.g., 0x18 for reset, '?' for status).
    void sendRealtimeCommand(char command);

    /// Jog: single step move.
    void jogStep(const QString &axis, double step, double feedRate);

    /// Jog: continuous move (simulated via long distance + cancel).
    void jogContinuous(const QString &axis, int direction, double feedRate);

    /// Cancel an active jog move.
    void jogCancel();

signals:
    void connectedChanged(bool connected);
    void responseReceived(const QString &response);
    void ackReceived();
    void commandErrorReceived(const QString &error);
    void errorOccurred(const QString &error);
    void commandSent(const QString &command);

private slots:
    void handleReadyRead();
    void handleSerialError(QSerialPort::SerialPortError error);
    void pollStatus();

private:
    void processBuffer();
    void closePort();

    MachineState *m_machineState;
    QSerialPort *m_port;
    QTimer *m_pollTimer;
    mutable QMutex m_mutex;
    QByteArray m_buffer;
    bool m_grblReady = false;
};
