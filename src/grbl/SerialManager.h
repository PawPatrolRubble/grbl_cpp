#pragma once
#include "GrblTransport.h"
#include "GrblCommandQueue.h"
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QTimer>
#include <QByteArray>
#include <QQueue>

class MachineState;

// Async adapter: the port and queues are used on their owning QObject thread.
// Ordinary commands enter through the job controller, including manual requests.
class SerialManager : public GrblTransport
{
    Q_OBJECT
public:
    explicit SerialManager(MachineState* machineState, QObject* parent = nullptr);
    ~SerialManager() override;
    static QStringList availablePorts();
    static QStringList commonBaudRates();
    bool connectToGrbl(const QString& portName, int baudRate);
    void disconnectFromGrbl();
    bool isConnected() const noexcept override;
    bool isReady() const override { return m_grblReady; }
    bool submit(const GrblCommand& command) override;
    void sendRealtimeCommand(char command) override;
    void clearPendingCommands() override;
    int rxBufUsed() const { return m_queue.bytesUsed(); }
    int pendingQueueSize() const { return m_queue.waitingCount(); }
    void setStatusPollInterval(int ms);
private:
    void handleReadyRead();
    void handleSerialError(QSerialPort::SerialPortError error);
    void processBuffer();
    void drainQueue();
    void resetTracking();
    void setReady(bool ready);
    MachineState* m_machineState;
    QSerialPort* m_port;
    QTimer* m_pollTimer;
    QByteArray m_buffer;
    GrblCommandQueue m_queue;
    bool m_grblReady = false;
    bool m_awaitingResetBanner = false;
};
