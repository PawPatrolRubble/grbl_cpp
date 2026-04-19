#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QTimer>
#include <QMutex>
#include <QByteArray>
#include <QQueue>

class MachineState;

/// GRBL RX buffer size (127 bytes, leave 1 free)
constexpr int GRBL_RX_BUFFER_SIZE = 127;

/// Cross-platform serial communication with GRBL devices.
/// Uses Qt's QSerialPort for async I/O instead of platform-specific APIs.
///
/// Implements GRBL's character-counting streaming protocol: commands are
/// queued internally and only transmitted when there is confirmed room in
/// GRBL's 128-byte serial RX buffer.  Each "ok" / "error:" response from
/// GRBL frees the corresponding number of bytes, which triggers the next
/// queued command to be sent automatically.
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

    /// Enqueue a line command (e.g., "G0 X0 Y0", "$H").
    /// The command will be sent immediately if there is room in GRBL's RX
    /// buffer, otherwise it is queued and sent automatically when an "ok"
    /// or "error:" frees enough space.
    /// Returns the encoded byte length including the trailing \r\n.
    int sendCommand(const QString &command);

    /// Send a single-byte realtime command (e.g., 0x18 for reset, '?' for status).
    /// Realtime commands bypass the buffer and are sent immediately.
    void sendRealtimeCommand(char command);

    /// How many bytes of GRBL's RX buffer are occupied by
    /// commands we've sent but not yet ack'd.
    int rxBufUsed() const;
    bool rxBufHasRoom(int bytes) const;

    /// Number of commands waiting in the outgoing queue.
    int pendingQueueSize() const;

    /// Jog: single step move.
    void jogStep(const QString &axis, double step, double feedRate);

    /// Jog: continuous move (simulated via long distance + cancel).
    void jogContinuous(const QString &axis, int direction, double feedRate);

    /// Cancel an active jog move.
    void jogCancel();

    /// Clear all queued outgoing commands and reset buffer tracking.
    /// Used when aborting a streaming session.
    void clearQueue();

    /// Set status poll interval (default 250ms, use longer during streaming).
    void setStatusPollInterval(int ms);

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
    void drainQueue();      ///< Send queued commands while GRBL's RX buffer has room.
    void closePort();

    MachineState *m_machineState;
    QSerialPort *m_port;
    QTimer *m_pollTimer;
    mutable QMutex m_mutex;
    QByteArray m_buffer;                ///< Incoming data accumulator.

    /// --- Character-counting protocol state ---
    QQueue<QByteArray> m_txQueue;       ///< Commands waiting to be written to the port.
    QQueue<int> m_sentLineLengths;      ///< Byte-lengths of lines written but not yet ack'd.
    int m_rxBufUsed = 0;               ///< Estimated bytes occupying GRBL's RX buffer.
    bool m_grblReady = false;
};
