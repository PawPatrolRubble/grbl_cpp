#include "SerialManager.h"
#include "MachineState.h"

#include <QTimer>
#include <QWinEventNotifier>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

SerialManager::SerialManager(MachineState *machineState, QObject *parent)
    : QObject(parent)
    , m_machineState(machineState)
    , m_handle(nullptr)
    , m_readNotifier(nullptr)
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
        FILE_FLAG_OVERLAPPED,
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
    timeouts.ReadTotalTimeoutMultiplier = MAXDWORD;
    timeouts.ReadTotalTimeoutConstant = 100;
    timeouts.WriteTotalTimeoutConstant = 1000;
    SetCommTimeouts(hPort, &timeouts);

    PurgeComm(hPort, PURGE_RXCLEAR | PURGE_TXCLEAR | PURGE_RXABORT | PURGE_TXABORT);

    m_handle = hPort;
#endif

    m_buffer.clear();
    m_grblReady = false;
    m_machineState->setConnected(true);
    m_machineState->setState(MachineState::State::Idle);

#ifdef Q_OS_WIN
    m_readNotifier = new QWinEventNotifier(m_handle, this);
    connect(m_readNotifier, &QWinEventNotifier::activated, this, &SerialManager::handleReadData);
    m_readNotifier->setEnabled(true);
#endif

    emit connectedChanged(true);
    return true;
}

void SerialManager::disconnectFromGrbl()
{
    if (m_readNotifier) {
        m_readNotifier->setEnabled(false);
        delete m_readNotifier;
        m_readNotifier = nullptr;
    }

#ifdef Q_OS_WIN
    if (m_handle) {
        CloseHandle(m_handle);
        m_handle = nullptr;
    }
#endif

    if (m_machineState->isConnected()) {
        m_machineState->setConnected(false);
        m_machineState->setState(MachineState::State::Unknown);
        emit connectedChanged(false);
    }

    m_streaming = false;
    m_paused = false;
    m_streamQueue.clear();
    m_pendingCommands.clear();
    m_linesSent = 0;
    m_okCount = 0;
    m_errorCount = 0;
}

bool SerialManager::isConnected() const
{
    return m_handle != nullptr;
}

bool SerialManager::writeData(const QByteArray &data)
{
#ifdef Q_OS_WIN
    if (!m_handle) return false;

    OVERLAPPED overlapped = {};
    overlapped.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    DWORD bytesWritten = 0;
    BOOL result = WriteFile(m_handle, data.constData(), data.size(), &bytesWritten, &overlapped);

    if (!result && GetLastError() == ERROR_IO_PENDING) {
        WaitForSingleObject(overlapped.hEvent, 2000);
        GetOverlappedResult(m_handle, &overlapped, &bytesWritten, TRUE);
    }

    CloseHandle(overlapped.hEvent);
    return bytesWritten == static_cast<DWORD>(data.size());
#else
    return false;
#endif
}

void SerialManager::sendCommand(const QString &command)
{
    if (!m_handle) return;

    QString cmd = command.trimmed();
    if (cmd.isEmpty()) return;

    QByteArray data = cmd.toUtf8() + "\r\n";
    writeData(data);
    emit commandSent(cmd);
}

void SerialManager::sendRealtimeCommand(char command)
{
    if (!m_handle) return;
    writeData(QByteArray(1, command));
}

void SerialManager::jogStep(const QString &axis, double step, double feedRate)
{
    if (!m_handle) return;
    QString cmd = QString("$J=G91 %1%2 F%3")
        .arg(axis)
        .arg(step, 0, 'f', 3)
        .arg(feedRate, 0, 'f', 0);
    sendCommand(cmd);
}

void SerialManager::jogContinuous(const QString &axis, int direction, double feedRate)
{
    if (!m_handle) return;
    // Use a large distance (9999 mm) so the move effectively runs until cancelled.
    // $J=G91 X9999 F1000  (or X-9999 for negative direction)
    double dist = (direction >= 0) ? 9999.0 : -9999.0;
    QString cmd = QString("$J=G91 %1%2 F%3")
        .arg(axis)
        .arg(dist, 0, 'f', 0)
        .arg(feedRate, 0, 'f', 0);
    sendCommand(cmd);
    m_jogging = true;
}

void SerialManager::jogCancel()
{
    if (!m_handle) return;
    sendRealtimeCommand(static_cast<char>(0x85));
    m_jogging = false;
}

void SerialManager::startStreaming(const QStringList &lines)
{
    if (!m_handle) return;

    m_streamQueue = lines;
    m_totalLines = lines.size();
    m_linesSent = 0;
    m_okCount = 0;
    m_errorCount = 0;
    m_streaming = true;
    m_paused = false;

    emit streamingStarted();

    for (int i = 0; i < qMin(16, m_totalLines); ++i)
        sendNextLine();
}

void SerialManager::pauseStreaming()
{
    if (m_streaming && !m_paused) {
        m_paused = true;
        sendRealtimeCommand('!');
        emit streamingPaused();
    }
}

void SerialManager::resumeStreaming()
{
    if (m_streaming && m_paused) {
        m_paused = false;
        sendRealtimeCommand('~');
        emit streamingResumed();
    }
}

void SerialManager::stopStreaming()
{
    m_streaming = false;
    m_streamQueue.clear();
    m_pendingCommands.clear();
    m_linesSent = 0;
    emit streamingFinished();
}

bool SerialManager::isStreaming() const
{
    return m_streaming;
}

void SerialManager::handleReadData()
{
#ifdef Q_OS_WIN
    if (!m_handle) return;

    OVERLAPPED overlapped = {};
    overlapped.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);

    char buf[4096];
    DWORD bytesRead = 0;

    while (true) {
        BOOL result = ReadFile(m_handle, buf, sizeof(buf) - 1, &bytesRead, &overlapped);
        if (!result && GetLastError() == ERROR_IO_PENDING) {
            WaitForSingleObject(overlapped.hEvent, 100);
            GetOverlappedResult(m_handle, &overlapped, &bytesRead, FALSE);
        }

        if (bytesRead == 0) break;

        buf[bytesRead] = '\0';
        m_buffer += QString::fromUtf8(buf, bytesRead);
        processBuffer();
    }

    CloseHandle(overlapped.hEvent);
#endif
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
            m_okCount++;
            emit responseReceived(line);
            if (m_streaming && !m_paused)
                sendNextLine();
        } else if (line.startsWith("error")) {
            m_errorCount++;
            emit responseReceived(line);
            if (m_streaming && !m_paused)
                sendNextLine();
        } else if (line.startsWith("Grbl")) {
            m_grblReady = true;
            emit responseReceived(line);
        } else {
            emit responseReceived(line);
        }
    }
}

void SerialManager::sendNextLine()
{
    if (m_linesSent >= m_totalLines) {
        if (m_okCount + m_errorCount >= m_totalLines) {
            m_streaming = false;
            emit streamingFinished();
        }
        return;
    }

    QString line = m_streamQueue[m_linesSent].trimmed();
    m_linesSent++;
    sendCommand(line);
    emit streamingProgress(m_linesSent, m_totalLines);
}