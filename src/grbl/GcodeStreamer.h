#pragma once

#include <QObject>
#include <QStringList>

class SerialManager;

/// Maximum lines pushed into SerialManager's queue per sendNextLine() call.
/// SerialManager handles actual byte-level throttling via its TX queue.
constexpr int MAX_IN_FLIGHT_LINES = 16;

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

    QStringList m_streamQueue;
    SerialManager *m_serialManager = nullptr;
    int m_linesSent = 0;       ///< Next index into m_streamQueue to process.
    int m_totalLines = 0;      ///< Total lines in the file (including blanks/comments).
    int m_commandsSent = 0;    ///< Number of actual G-code commands sent (excluding blanks/comments).
    int m_okCount = 0;         ///< Number of "ok" acks received from GRBL.
    int m_errorCount = 0;      ///< Number of "error:" responses received from GRBL.
    bool m_streaming = false;
    bool m_paused = false;
};