#include "SerialManager.h"
#include "MachineState.h"

#include <QDebug>
#include <QEventLoop>

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

SerialManager::~SerialManager()
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

    // Extract just the port name (strip description if present)
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

    // Small delay to let GRBL reset
    QEventLoop loop;
    QTimer::singleShot(500, &loop, &QEventLoop::quit);
    loop.exec();

    m_port->clear();
    m_buffer.clear();
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

bool SerialManager::isConnected() const
{
    QMutexLocker locker(&m_mutex);
    return m_port && m_port->isOpen();
}

void SerialManager::sendCommand(const QString &command)
{
    QMutexLocker locker(&m_mutex);
    if (!m_port || !m_port->isOpen()) return;

    QString cmd = command.trimmed();
    if (cmd.isEmpty()) return;

    QByteArray data = cmd.toUtf8() + "\r\n";
    m_port->write(data);
    m_port->flush();
    locker.unlock();
    emit commandSent(cmd);
}

void SerialManager::sendRealtimeCommand(char command)
{
    QMutexLocker locker(&m_mutex);
    if (!m_port || !m_port->isOpen()) return;
    m_port->write(&command, 1);
    m_port->flush();
}

void SerialManager::jogStep(const QString &axis, double step, double feedRate)
{
    if (!isConnected()) return;
    QString cmd = QString("$J=G91 %1%2 F%3")
        .arg(axis)
        .arg(step, 0, 'f', 3)
        .arg(feedRate, 0, 'f', 0);
    sendCommand(cmd);
}

void SerialManager::jogContinuous(const QString &axis, int direction, double feedRate)
{
    if (!isConnected()) return;
    double dist = (direction >= 0) ? 9999.0 : -9999.0;
    QString cmd = QString("$J=G91 %1%2 F%3")
        .arg(axis)
        .arg(dist, 0, 'f', 0)
        .arg(feedRate, 0, 'f', 0);
    sendCommand(cmd);
}

void SerialManager::jogCancel()
{
    if (!isConnected()) return;
    sendRealtimeCommand(static_cast<char>(0x85));
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

    QString msg;
    if (error == QSerialPort::ResourceError) {
        msg = tr("Device disconnected");
        disconnectFromGrbl();
    } else {
        msg = tr("Serial error: %1").arg(m_port->errorString());
    }
    emit errorOccurred(msg);
}

void SerialManager::pollStatus()
{
    sendRealtimeCommand('?');
}

void SerialManager::processBuffer()
{
    QMutexLocker locker(&m_mutex);

    // Process line-based responses
    while (true) {
        // Find first newline or carriage return
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
            break;  // No line terminator found

        // Extract line (handle \r\n case)
        int endPos = splitPos;
        if (splitPos + 1 < m_buffer.size() && m_buffer[splitPos] == '\r' && m_buffer[splitPos + 1] == '\n')
            endPos = splitPos + 1;
        else if (splitPos + 1 < m_buffer.size() && m_buffer[splitPos] == '\n' && m_buffer[splitPos + 1] == '\r')
            endPos = splitPos + 1;

        QString line = QString::fromUtf8(m_buffer.left(splitPos)).trimmed();
        m_buffer.remove(0, endPos + 1);
        locker.unlock();

        if (!line.isEmpty()) {
            if (line.startsWith(QLatin1String("<")) && line.endsWith(QLatin1String(">"))) {
                // Status report — GrblParser handles this
                emit responseReceived(line);
            } else if (line.startsWith(QLatin1String("ok"))) {
                emit ackReceived();
                emit responseReceived(line);
            } else if (line.startsWith(QLatin1String("error"))) {
                emit commandErrorReceived(line);
                emit responseReceived(line);
            } else if (line.startsWith(QLatin1String("Grbl"))) {
                m_grblReady = true;
                emit responseReceived(line);
            } else {
                // Info messages, settings echo, etc.
                emit responseReceived(line);
            }
        }

        locker.relock();
    }
}

void SerialManager::closePort()
{
    QMutexLocker locker(&m_mutex);
    if (m_port && m_port->isOpen()) {
        m_port->close();
    }
    m_buffer.clear();
    m_grblReady = false;
}
