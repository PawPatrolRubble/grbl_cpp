#pragma once

#include <QObject>
#include <QStringList>

class SerialManager;

class GcodeStreamer : public QObject
{
    Q_OBJECT

public:
    explicit GcodeStreamer(QObject *parent = nullptr);
    ~GcodeStreamer() override = default;

    void setSerialManager(SerialManager *serialManager);

    void startStreaming(const QStringList &lines);
    void pauseStreaming();
    void resumeStreaming();
    void stopStreaming();
    [[nodiscard]] bool isStreaming() const;
    [[nodiscard]] bool isPaused() const;

signals:
    void sendCommandRequested(const QString &command);
    void sendRealtimeRequested(char command);
    void streamingStarted();
    void streamingFinished();
    void streamingPaused();
    void streamingResumed();
    void streamingProgress(int currentLine, int totalLines);

public slots:
    void onAckReceived();
    void onErrorReceived(const QString &error);
    void onDisconnected();

private:
    void sendNextLine();

    /// Max commands allowed in SerialManager's TX queue before we stop
    /// feeding more. Prevents enqueueing thousands of lines in one burst.
    static constexpr int MAX_PENDING_COMMANDS = 32;

    QStringList m_streamQueue;
    SerialManager *m_serialManager = nullptr;
    int m_linesSent = 0;       ///< Next index into m_streamQueue to process.
    int m_totalLines = 0;      ///< Total lines in the file (including blanks/comments).
    int m_commandsSent = 0;    ///< Number of actual G-code commands sent (excluding blanks/comments).
    int m_okCount = 0;         ///< Number of "ok" acks received from GRBL.
    int m_errorCount = 0;      ///< Number of "error:" responses received from GRBL.
    int m_lastProgressLine = -1; ///< Last line number we emitted progress for (throttle).
    bool m_streaming = false;
    bool m_paused = false;
};