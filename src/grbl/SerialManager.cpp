#include "SerialManager.h"
#include "MachineState.h"
#include "GrblConstants.h"
#include "GcodeStreamer.h"

#include <QDebug>
#include <QEventLoop>
#include <QPair>

SerialManager::SerialManager(MachineState *machineState, QObject *parent)
    : QObject(parent)
    , m_machineState(machineState)
    , m_port(new QSerialPort(this))
    , m_pollTimer(new QTimer(this))
{
    connect(m_port, &QSerialPort::readyRead, this, &SerialManager::handleReadyRead, Qt::DirectConnection);
    connect(m_port, QOverload<QSerialPort::SerialPortError>::of(&QSerialPort::errorOccurred),
            this, &SerialManager::handleSerialError, Qt::DirectConnection);

    m_pollTimer->setInterval(250);
    connect(m_pollTimer, &QTimer::timeout, this, &SerialManager::pollStatus);
}

SerialManager::~SerialManager() noexcept
{
    disconnectFromGrbl();
}

QStringList SerialManager::availablePorts()
{
    QStringList result;
    const auto ports = QSerialPortInfo::availablePorts();
    for (const auto &info : ports) {
        QString desc = info.description().isEmpty() ? info.portName() : info.description();
        result << QString("%1 - %2").arg(info.portName(), desc);
    }
    std::sort(result.begin(), result.end());
    return result;
}

QStringList SerialManager::commonBaudRates()
{
    return {"9600", "19200", "38400", "57600", "115200", "230400", "460800"};
}

bool SerialManager::connectToGrbl(const QString &portName, int baudRate)
{
    disconnectFromGrbl();

    QMutexLocker locker(&m_mutex);

    QString cleanPortName = portName;
    int dashIdx = portName.indexOf(QLatin1String(" - "));
    if (dashIdx > 0) {
        cleanPortName = portName.left(dashIdx).trimmed();
    }

    m_port->setPortName(cleanPortName);
    m_port->setBaudRate(baudRate);
    m_port->setDataBits(QSerialPort::Data8);
    m_port->setParity(QSerialPort::NoParity);
    m_port->setStopBits(QSerialPort::OneStop);
    m_port->setFlowControl(QSerialPort::NoFlowControl);

    if (!m_port->open(QIODevice::ReadWrite)) {
        emit errorOccurred(tr("Failed to open port %1: %2").arg(cleanPortName, m_port->errorString()));
        return false;
    }

    m_port->setRequestToSend(true);
    m_port->setDataTerminalReady(true);

    QEventLoop loop;
    QTimer::singleShot(500, &loop, &QEventLoop::quit);
    loop.exec();

    m_port->clear();
    m_buffer.clear();
    m_txQueue.clear();
    m_sentLineLengths.clear();
    m_rxBufUsed = 0;
    m_grblReady = false;
    locker.unlock();

    m_machineState->setConnected(true);
    m_machineState->setState(MachineState::State::Idle);
    emit connectedChanged(true);

    m_pollTimer->start();
    return true;
}

void SerialManager::disconnectFromGrbl()
{
    m_pollTimer->stop();
    closePort();

    if (m_machineState->isConnected()) {
        m_machineState->setConnected(false);
        m_machineState->setState(MachineState::State::Unknown);
        emit connectedChanged(false);
    }
}

bool SerialManager::isConnected() const noexcept
{
    QMutexLocker locker(&m_mutex);
    return m_port && m_port->isOpen();
}

// ---------------------------------------------------------------------------
// sendCommand — Enqueue a G-code line.
//
// Instead of writing directly to the serial port, we push the encoded
// payload into m_txQueue and then call drainQueue() which will write as
// many queued commands as fit inside GRBL's 128-byte RX buffer.
//
// This is the character-counting streaming protocol recommended by the
// GRBL project:
//   https://github.com/gnea/grbl/wiki/Grbl-v1.1-Interface#streaming-a-g-code-program-to-grbl
// ---------------------------------------------------------------------------
int SerialManager::sendCommand(const QString &command)
{
    QMutexLocker locker(&m_mutex);
    if (!m_port || !m_port->isOpen()) return 0;

    QString cmd = command.trimmed();
    if (cmd.isEmpty()) return 0;

    QByteArray data = cmd.toUtf8() + "\n";

    // Reject lines that can never fit, even in an empty buffer.
    if (data.size() > GRBL_RX_BUFFER_SIZE) {
        locker.unlock();
        emit errorOccurred(tr("Command too long for GRBL buffer (%1 bytes): %2")
                               .arg(data.size()).arg(cmd));
        return 0;
    }

    m_txQueue.enqueue(data);
    locker.unlock();

    // Try to push queued data to the port immediately.
    drainQueue();
    return data.size();
}

void SerialManager::sendRealtimeCommand(char command)
{
    QMutexLocker locker(&m_mutex);
    if (!m_port || !m_port->isOpen()) return;
    m_port->write(&command, 1);
    m_port->flush();
}

int SerialManager::rxBufUsed() const
{
    QMutexLocker locker(&m_mutex);
    return m_rxBufUsed;
}

bool SerialManager::rxBufHasRoom(int bytes) const
{
    QMutexLocker locker(&m_mutex);
    return m_rxBufUsed + bytes <= GRBL_RX_BUFFER_SIZE;
}

int SerialManager::pendingQueueSize() const
{
    QMutexLocker locker(&m_mutex);
    return m_txQueue.size();
}

void SerialManager::jogStep(const QString &axis, double step, double feedRate)
{
    if (!isConnected()) return;
    sendCommand(QString("$J=G91 %1%2 F%3").arg(axis).arg(step, 0, 'f', 3).arg(feedRate, 0, 'f', 0));
}

void SerialManager::jogContinuous(const QString &axis, int direction, double feedRate)
{
    if (!isConnected()) return;
    double dist = (direction >= 0) ? 9999.0 : -9999.0;
    sendCommand(QString("$J=G91 %1%2 F%3").arg(axis).arg(dist, 0, 'f', 0).arg(feedRate, 0, 'f', 0));
}

void SerialManager::jogCancel()
{
    if (!isConnected()) return;
    sendRealtimeCommand(GrblRealtime::JogCancel);
}

// ---------------------------------------------------------------------------
// drainQueue — Write queued commands to the serial port while there is room
//              in GRBL's RX buffer.
//
// The character-counting protocol allows us to have multiple commands
// in-flight simultaneously, as long as the total byte count of unsent (not yet
// ack'd) data does not exceed GRBL_RX_BUFFER_SIZE.
// ---------------------------------------------------------------------------
void SerialManager::drainQueue()
{
    QList<QString> emitted;   // collect outside lock, emit after

    {
        QMutexLocker locker(&m_mutex);
        if (!m_port || !m_port->isOpen()) return;

        while (!m_txQueue.isEmpty()) {
            const QByteArray &head = m_txQueue.head();
            int bytes = head.size();

            if (m_rxBufUsed + bytes > GRBL_RX_BUFFER_SIZE) {
                break;    // No room — wait for an ack to free space.
            }

            QByteArray data = m_txQueue.dequeue();
            m_rxBufUsed += bytes;
            m_sentLineLengths.enqueue(bytes);

            m_port->write(data);
            m_port->flush();

            // Recover the command string (strip trailing \n) for the signal.
            QString cmdStr = QString::fromUtf8(data.left(data.size() - 1));
            emitted.append(cmdStr);
        }
    }

    for (const QString &cmd : emitted) {
        emit commandSent(cmd);
    }
}

void SerialManager::handleReadyRead()
{
    QByteArray data = m_port->readAll();
    if (data.isEmpty()) return;

    QMutexLocker locker(&m_mutex);
    m_buffer.append(data);
    locker.unlock();

    processBuffer();
}

void SerialManager::handleSerialError(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::NoError) return;

    if (error == QSerialPort::ResourceError) {
        disconnectFromGrbl();
        emit errorOccurred(tr("Device disconnected"));
    } else {
        emit errorOccurred(tr("Serial error: %1").arg(m_port->errorString()));
    }
}

void SerialManager::pollStatus()
{
    sendRealtimeCommand(GrblRealtime::StatusReport);
}

void SerialManager::processBuffer()
{
    QList<QPair<QString, bool>> linesToEmit;

    {
        QMutexLocker locker(&m_mutex);

        while (true) {
            int nlPos = m_buffer.indexOf('\n');
            int crPos = m_buffer.indexOf('\r');

            int splitPos = -1;
            if (nlPos >= 0 && crPos >= 0)
                splitPos = qMin(nlPos, crPos);
            else if (nlPos >= 0)
                splitPos = nlPos;
            else if (crPos >= 0)
                splitPos = crPos;
            else
                break;

            int endPos = splitPos;
            if (splitPos + 1 < m_buffer.size() && m_buffer[splitPos] == '\r' && m_buffer[splitPos + 1] == '\n')
                endPos = splitPos + 1;
            else if (splitPos + 1 < m_buffer.size() && m_buffer[splitPos] == '\n' && m_buffer[splitPos + 1] == '\r')
                endPos = splitPos + 1;

            QString line = QString::fromUtf8(m_buffer.left(splitPos)).trimmed();
            m_buffer.remove(0, endPos + 1);

            if (!line.isEmpty()) {
                bool isAck = line.startsWith(GrblResponse::OkPrefix)
                          || line.startsWith(GrblResponse::ErrorPrefix);
                if (isAck && !m_sentLineLengths.isEmpty())
                    m_rxBufUsed -= m_sentLineLengths.dequeue();

                linesToEmit.append({line, isAck});
            }
        }
    }

    // Emit signals outside the lock.
    for (const auto &entry : linesToEmit) {
        const QString &line = entry.first;
        bool isAck = entry.second;

        if (line.startsWith(GrblResponse::StatusStart) && line.endsWith(GrblResponse::StatusEnd)) {
            emit responseReceived(line);
        } else if (line.startsWith(GrblResponse::OkPrefix)) {
            emit ackReceived();
            emit responseReceived(line);
        } else if (line.startsWith(GrblResponse::ErrorPrefix)) {
            emit commandErrorReceived(line);
            emit responseReceived(line);
        } else if (line.startsWith(GrblResponse::WelcomeStart)) {
            m_grblReady = true;
            emit responseReceived(line);
        } else {
            emit responseReceived(line);
        }
    }

    // Each ack frees space in the RX buffer — try to push more queued
    // commands now.
    drainQueue();
}

void SerialManager::closePort()
{
    QMutexLocker locker(&m_mutex);
    if (m_port && m_port->isOpen()) {
        m_port->close();
    }
    m_buffer.clear();
    m_txQueue.clear();
    m_sentLineLengths.clear();
    m_rxBufUsed = 0;
    m_grblReady = false;
}
