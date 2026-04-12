#include "SerialManager.h"
#include "MachineState.h"

#include <QDebug>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

SerialManager::SerialManager(MachineState *machineState, QObject *parent)
    : QObject(parent)
    , m_machineState(machineState)
    , m_handle(nullptr)
{
}

SerialManager::~SerialManager()
{
    disconnectFromGrbl();
}

QStringList SerialManager::availablePorts()
{
    QStringList ports;
#ifdef Q_OS_WIN
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"HARDWARE\\DEVICEMAP\\SERIALCOMM", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD index = 0;
        WCHAR name[256];
        BYTE data[256];
        DWORD nameSize, dataSize;
        while (true) {
            nameSize = sizeof(name);
            dataSize = sizeof(data);
            LONG result = RegEnumValueW(hKey, index, name, &nameSize, nullptr, nullptr, data, &dataSize);
            if (result != ERROR_SUCCESS)
                break;
            QString portName = QString::fromWCharArray(reinterpret_cast<wchar_t*>(data), dataSize / sizeof(wchar_t));
            ports << portName;
            index++;
        }
        RegCloseKey(hKey);
    }
#endif
    std::sort(ports.begin(), ports.end());
    return ports;
}

QStringList SerialManager::commonBaudRates()
{
    return {"9600", "19200", "38400", "57600", "115200", "230400", "460800"};
}

bool SerialManager::connectToGrbl(const QString &portName, int baudRate)
{
    disconnectFromGrbl();

    m_baudRate = baudRate;

#ifdef Q_OS_WIN
    QString path = "\\\\.\\" + portName;
    HANDLE hPort = CreateFileW(
        reinterpret_cast<LPCWSTR>(path.utf16()),
        GENERIC_READ | GENERIC_WRITE,
        0,
        nullptr,
        OPEN_EXISTING,
        0,
        nullptr
    );

    if (hPort == INVALID_HANDLE_VALUE) {
        emit errorOccurred(tr("Failed to open port %1: %2").arg(portName).arg(GetLastError()));
        return false;
    }

    DCB dcb = {};
    dcb.DCBlength = sizeof(DCB);
    if (!GetCommState(hPort, &dcb)) {
        CloseHandle(hPort);
        emit errorOccurred(tr("Failed to get comm state"));
        return false;
    }

    dcb.BaudRate = baudRate;
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    dcb.fDtrControl = DTR_CONTROL_ENABLE;
    dcb.fRtsControl = RTS_CONTROL_ENABLE;

    if (!SetCommState(hPort, &dcb)) {
        CloseHandle(hPort);
        emit errorOccurred(tr("Failed to set comm state"));
        return false;
    }

    COMMTIMEOUTS timeouts = {};
    timeouts.ReadIntervalTimeout = MAXDWORD;
    timeouts.ReadTotalTimeoutMultiplier = 0;
    timeouts.ReadTotalTimeoutConstant = 10;
    timeouts.WriteTotalTimeoutMultiplier = 0;
    timeouts.WriteTotalTimeoutConstant = 1000;
    SetCommTimeouts(hPort, &timeouts);

    PurgeComm(hPort, PURGE_RXCLEAR | PURGE_TXCLEAR | PURGE_RXABORT | PURGE_TXABORT);
    SetCommMask(hPort, EV_RXCHAR);

    {
        QMutexLocker locker(&m_handleMutex);
        m_handle = hPort;
    }
#endif

    m_buffer.clear();
    m_grblReady = false;
    m_machineState->setConnected(true);
    m_machineState->setState(MachineState::State::Idle);

    m_readTimer = new QTimer();
    m_readTimer->moveToThread(&m_ioThread);
    connect(m_readTimer, &QTimer::timeout, this, &SerialManager::handleReadData, Qt::DirectConnection);

    m_pollTimer = new QTimer();
    m_pollTimer->moveToThread(&m_ioThread);
    connect(m_pollTimer, &QTimer::timeout, this, &SerialManager::pollStatus, Qt::DirectConnection);

    m_ioThread.start();

    QMetaObject::invokeMethod(m_readTimer, "start", Qt::QueuedConnection, Q_ARG(int, 20));
    QMetaObject::invokeMethod(m_pollTimer, "start", Qt::QueuedConnection, Q_ARG(int, 250));

    emit connectedChanged(true);
    return true;
}

void SerialManager::disconnectFromGrbl()
{
    if (m_readTimer) {
        QMetaObject::invokeMethod(m_readTimer, "stop", Qt::BlockingQueuedConnection);
        m_readTimer->deleteLater();
        m_readTimer = nullptr;
    }

    if (m_pollTimer) {
        QMetaObject::invokeMethod(m_pollTimer, "stop", Qt::BlockingQueuedConnection);
        m_pollTimer->deleteLater();
        m_pollTimer = nullptr;
    }

    if (m_ioThread.isRunning()) {
        m_ioThread.quit();
        m_ioThread.wait();
    }

    {
        QMutexLocker locker(&m_handleMutex);
#ifdef Q_OS_WIN
        if (m_handle) {
            CloseHandle(m_handle);
            m_handle = nullptr;
        }
#endif
    }

    if (m_machineState->isConnected()) {
        m_machineState->setConnected(false);
        m_machineState->setState(MachineState::State::Unknown);
        emit connectedChanged(false);
    }
}

bool SerialManager::isConnected() const
{
    QMutexLocker locker(&m_handleMutex);
    return m_handle != nullptr;
}

void SerialManager::sendCommand(const QString &command)
{
    QMutexLocker locker(&m_handleMutex);
#ifdef Q_OS_WIN
    if (!m_handle) return;

    QString cmd = command.trimmed();
    if (cmd.isEmpty()) return;

    QByteArray data = cmd.toUtf8() + "\r\n";
    DWORD bytesWritten = 0;
    WriteFile(m_handle, data.constData(), data.size(), &bytesWritten, nullptr);
    locker.unlock();
    emit commandSent(cmd);
#endif
}

void SerialManager::sendRealtimeCommand(char command)
{
    QMutexLocker locker(&m_handleMutex);
#ifdef Q_OS_WIN
    if (!m_handle) return;
    WriteFile(m_handle, &command, 1, nullptr, nullptr);
#endif
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

void SerialManager::handleReadData()
{
#ifdef Q_OS_WIN
    QMutexLocker locker(&m_handleMutex);
    if (!m_handle) return;

    DWORD errors = 0;
    COMSTAT comstat = {};
    ClearCommError(m_handle, &errors, &comstat);

    if (comstat.cbInQue == 0)
        return;

    char buf[4096];
    DWORD bytesRead = 0;

    while (comstat.cbInQue > 0) {
        DWORD toRead = qMin(comstat.cbInQue, static_cast<DWORD>(sizeof(buf) - 1));
        BOOL result = ReadFile(m_handle, buf, toRead, &bytesRead, nullptr);
        if (!result || bytesRead == 0)
            break;

        buf[bytesRead] = '\0';
        m_buffer += QString::fromUtf8(buf, bytesRead);
        locker.unlock();
        processBuffer();
        locker.relock();

        ClearCommError(m_handle, &errors, &comstat);
        if (comstat.cbInQue == 0)
            break;
    }
#endif
}

void SerialManager::pollStatus()
{
    sendRealtimeCommand('?');
}

void SerialManager::processBuffer()
{
    while (m_buffer.contains('\n') || m_buffer.contains('\r')) {
        int newlinePos = m_buffer.indexOf('\n');
        int crPos = m_buffer.indexOf('\r');
        int splitPos;

        if (newlinePos >= 0 && crPos >= 0)
            splitPos = qMin(newlinePos, crPos);
        else if (newlinePos >= 0)
            splitPos = newlinePos;
        else
            splitPos = crPos;

        QString line = m_buffer.left(splitPos).trimmed();
        m_buffer.remove(0, splitPos + 1);

        if (line.isEmpty()) continue;

        if (line.startsWith('<') && line.endsWith('>')) {
            emit responseReceived(line);
        } else if (line.startsWith("ok")) {
            emit ackReceived();
            emit responseReceived(line);
        } else if (line.startsWith("error")) {
            emit commandErrorReceived(line);
            emit responseReceived(line);
        } else if (line.startsWith("Grbl")) {
            m_grblReady = true;
            emit responseReceived(line);
        } else {
            emit responseReceived(line);
        }
    }
}