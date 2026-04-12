#pragma once

#include <QObject>
#include <QStringList>
#include <QQueue>

class GcodeStreamer : public QObject
{
    Q_OBJECT

public:
    explicit GcodeStreamer(QObject *parent = nullptr);

    void startStreaming(const QStringList &lines);
    void pauseStreaming();
    void resumeStreaming();
    void stopStreaming();
    bool isStreaming() const;

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
    QQueue<int> m_sentLineLengths;
    int m_linesSent = 0;
    int m_totalLines = 0;
    int m_okCount = 0;
    int m_errorCount = 0;
    int m_bufferBytes = 0;
    bool m_streaming = false;
    bool m_paused = false;
};