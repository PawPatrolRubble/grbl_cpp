#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QQueue>
#include <QThreadPool>
#include <QReadWriteLock>

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

    void startStreaming(const QStringList &lines);
    void pauseStreaming();
    void resumeStreaming();
    void stopStreaming();
    bool isStreaming() const;

signals:
    void connectedChanged(bool connected);
    void responseReceived(const QString &response);
    void errorOccurred(const QString &error);
    void streamingStarted();
    void streamingFinished();
    void streamingPaused();
    void streamingResumed();
    void streamingProgress(int currentLine, int totalLines);
    void commandSent(const QString &command);
    void commandError(int line, const QString &error);

private slots:
    void handleReadData();

private:
    void processBuffer();
    void sendNextLine();

    MachineState *m_machineState;
    QString m_buffer;

    void *m_handle;
    class QWinEventNotifier *m_readNotifier;
    int m_baudRate = 115200;

    QStringList m_streamQueue;
    QQueue<QString> m_pendingCommands;
    int m_linesSent = 0;
    int m_totalLines = 0;
    int m_okCount = 0;
    int m_errorCount = 0;
    bool m_streaming = false;
    bool m_paused = false;
    bool m_jogging = false;
    bool m_grblReady = false;

    bool writeData(const QByteArray &data);
};