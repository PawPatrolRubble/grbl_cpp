#include "GcodeStreamer.h"
#include "GrblConstants.h"

GcodeStreamer::GcodeStreamer(QObject *parent)
    : QObject(parent)
{
}

void GcodeStreamer::startStreaming(const QStringList &lines)
{
    m_streamQueue = lines;
    m_totalLines = lines.size();
    m_linesSent = 0;
    m_okCount = 0;
    m_errorCount = 0;
    m_bufferBytes = 0;
    m_sentLineLengths.clear();
    m_streaming = true;
    m_paused = false;

    emit streamingStarted();
    sendNextLine();
}

void GcodeStreamer::pauseStreaming()
{
    if (m_streaming && !m_paused) {
        m_paused = true;
        emit sendRealtimeRequested(GrblRealtime::FeedHold);
        emit streamingPaused();
    }
}

void GcodeStreamer::resumeStreaming()
{
    if (m_streaming && m_paused) {
        m_paused = false;
        emit sendRealtimeRequested(GrblRealtime::CycleStart);
        emit streamingResumed();
    }
}

void GcodeStreamer::stopStreaming()
{
    m_streaming = false;
    m_streamQueue.clear();
    m_sentLineLengths.clear();
    m_bufferBytes = 0;
    m_linesSent = 0;
    m_okCount = 0;
    m_errorCount = 0;
    emit streamingFinished();
}

bool GcodeStreamer::isStreaming() const
{
    return m_streaming;
}

bool GcodeStreamer::isPaused() const
{
    return m_paused;
}

void GcodeStreamer::onAckReceived()
{
    m_okCount++;
    if (m_streaming && !m_sentLineLengths.isEmpty()) {
        m_bufferBytes -= m_sentLineLengths.dequeue();
    }
    if (m_streaming && !m_paused) {
        sendNextLine();
    }
}

void GcodeStreamer::onErrorReceived(const QString &/*error*/)
{
    m_errorCount++;
    if (m_streaming && !m_sentLineLengths.isEmpty()) {
        m_bufferBytes -= m_sentLineLengths.dequeue();
    }
    if (m_streaming && !m_paused) {
        sendNextLine();
    }
}

void GcodeStreamer::onDisconnected()
{
    if (m_streaming) {
        m_streaming = false;
        m_streamQueue.clear();
        m_sentLineLengths.clear();
        m_bufferBytes = 0;
        m_linesSent = 0;
        m_okCount = 0;
        m_errorCount = 0;
        emit streamingFinished();
    }
}

void GcodeStreamer::sendNextLine()
{
    if (m_linesSent >= m_totalLines) {
        if (m_okCount + m_errorCount >= m_totalLines) {
            m_streaming = false;
            m_sentLineLengths.clear();
            m_bufferBytes = 0;
            emit streamingFinished();
        }
        return;
    }

    while (m_linesSent < m_totalLines && !m_paused) {
        QString line = m_streamQueue[m_linesSent].trimmed();
        int lineLength = line.toUtf8().size() + 2; // \r\n

        if (m_bufferBytes + lineLength > 127) {
            break; // GRBL RX buffer limit
        }

        m_bufferBytes += lineLength;
        m_sentLineLengths.enqueue(lineLength);

        m_linesSent++;
        emit sendCommandRequested(line);
        emit streamingProgress(m_linesSent, m_totalLines);
    }
}