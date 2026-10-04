#include "SerialManager.h"
#include "MachineState.h"
#include "GrblConstants.h"
#include <algorithm>

SerialManager::SerialManager(MachineState* machineState, QObject* parent)
    : GrblTransport(parent), m_machineState(machineState), m_port(new QSerialPort(this)),
      m_pollTimer(new QTimer(this))
{
    connect(m_port, &QSerialPort::readyRead, this, &SerialManager::handleReadyRead);
    connect(m_port, &QSerialPort::errorOccurred, this, &SerialManager::handleSerialError);
    m_pollTimer->setInterval(250);
    connect(m_pollTimer, &QTimer::timeout, this, [this] { sendRealtimeCommand(GrblRealtime::StatusReport); });
}
// QObject owns the port and timer. Do not emit state changes during teardown:
// ApplicationCore's sibling state model may already have been destroyed.
SerialManager::~SerialManager() = default;
QStringList SerialManager::availablePorts()
{
    QStringList result;
    for (const auto& info : QSerialPortInfo::availablePorts())
        result << QString("%1 - %2").arg(info.portName(), info.description().isEmpty() ? info.portName() : info.description());
    std::sort(result.begin(), result.end());
    return result;
}
QStringList SerialManager::commonBaudRates()
{
    return {"9600", "19200", "38400", "57600", "115200", "230400", "460800"};
}
bool SerialManager::connectToGrbl(const QString& portName, int baudRate)
{
    disconnectFromGrbl();
    QString name = portName;
    const int separator = name.indexOf(" - ");
    if (separator > 0) name = name.left(separator).trimmed();
    m_port->setPortName(name);
    m_port->setBaudRate(baudRate);
    m_port->setDataBits(QSerialPort::Data8);
    m_port->setParity(QSerialPort::NoParity);
    m_port->setStopBits(QSerialPort::OneStop);
    m_port->setFlowControl(QSerialPort::NoFlowControl);
    if (!m_port->open(QIODevice::ReadWrite)) {
        emit errorOccurred(tr("Failed to open port %1: %2").arg(name, m_port->errorString()));
        return false;
    }
    m_buffer.clear();
    resetTracking();
    m_awaitingResetBanner = false;
    m_port->setRequestToSend(true);
    m_port->setDataTerminalReady(true);
    m_machineState->reset();
    m_machineState->setConnected(true);
    emit connectedChanged(true);
    m_pollTimer->start();
    sendRealtimeCommand(GrblRealtime::StatusReport);
    return true;
}
void SerialManager::disconnectFromGrbl()
{
    m_pollTimer->stop();
    const bool wasConnected = isConnected();
    if (m_port->isOpen()) m_port->close();
    resetTracking();
    m_buffer.clear();
    m_awaitingResetBanner = false;
    m_machineState->setConnected(false);
    m_machineState->setState(MachineState::State::Unknown);
    if (wasConnected) emit connectedChanged(false);
}
bool SerialManager::isConnected() const noexcept { return m_port->isOpen(); }
void SerialManager::setReady(bool ready)
{
    if (m_grblReady == ready) return;
    m_grblReady = ready;
    emit readyChanged(ready);
}
void SerialManager::resetTracking()
{
    m_queue.reset();
    setReady(false);
}
bool SerialManager::submit(const GrblCommand& command)
{
    if (!isConnected() || !isReady() || command.id == 0) return false;
    if (!m_queue.enqueue(command)) return false;
    drainQueue();
    return true;
}
void SerialManager::clearPendingCommands() { m_queue.clearWaiting(); }
void SerialManager::sendRealtimeCommand(char command)
{
    if (!isConnected()) return;
    if (command == GrblRealtime::Reset) {
        m_awaitingResetBanner = true;
        resetTracking();
        m_port->clear(QSerialPort::Output);
        emit controllerReset();
    }
    if (m_port->write(&command, 1) != 1) emit errorOccurred(m_port->errorString());
    m_port->flush();
}
void SerialManager::setStatusPollInterval(int ms) { m_pollTimer->setInterval(ms); }
void SerialManager::drainQueue()
{
    while (isConnected() && isReady()) {
        const auto entry = m_queue.takeWritable();
        if (!entry) break;
        if (m_port->write(entry->bytes) != entry->bytes.size()) {
            emit errorOccurred(tr("Failed to write complete command: %1").arg(m_port->errorString()));
            return;
        }
        emit commandSent(entry->command.text);
    }
}
void SerialManager::handleReadyRead()
{
    m_buffer.append(m_port->readAll());
    processBuffer();
}
void SerialManager::handleSerialError(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::NoError) return;
    const QString message = m_port->errorString();
    if (error == QSerialPort::ResourceError) disconnectFromGrbl();
    emit errorOccurred(tr("Serial error: %1").arg(message));
}
void SerialManager::processBuffer()
{
    while (true) {
        const int nl = m_buffer.indexOf('\n'), cr = m_buffer.indexOf('\r');
        const int separator = nl < 0 ? cr : (cr < 0 ? nl : qMin(nl, cr));
        if (separator < 0) break;
        const QString line = QString::fromUtf8(m_buffer.left(separator)).trimmed();
        m_buffer.remove(0, separator + 1);
        if (line.isEmpty()) continue;
        if (line.startsWith(GrblResponse::WelcomeStart)) {
            resetTracking();
            m_awaitingResetBanner = false;
            emit controllerReset();
            emit responseReceived(line);
            setReady(true);
        } else {
            emit responseReceived(line);
            if (!m_awaitingResetBanner && !isReady() && line.startsWith('<') && line.endsWith('>'))
                setReady(true);
            if (line == "ok" || line.startsWith("error:")) {
                const auto entry = m_queue.acknowledge();
                if (entry) emit commandCompleted(entry->command.id, line == "ok" ? QString{} : line);
            }
        }
        drainQueue();
    }
}
