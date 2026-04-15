#include "GcodeStreamer.h"
#include "GrblConstants.h"
#include "SerialManager.h"

GcodeStreamer::GcodeStreamer(QObject *parent)
    : QObject(parent)
{
}

void GcodeStreamer::setSerialManager(SerialManager *serialManager)
{
    m_serialManager = serialManager;
}

void GcodeStreamer::startStreaming(const QStringList &lines)
{
    m_streamQueue = lines;
    m_totalLines = lines.size();
    m_linesSent = 0;
    m_commandsSent = 0;
    m_okCount = 0;
    m_errorCount = 0;
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
        sendNextLine();
    }
}

void GcodeStreamer::stopStreaming()
{
    m_streaming = false;
    m_streamQueue.clear();
    m_linesSent = 0;
    m_commandsSent = 0;
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
    if (!m_streaming) return;

    m_okCount++;

    if (!m_paused) {
        // All commands acknowledged?
        if (m_okCount + m_errorCount >= m_commandsSent && m_linesSent >= m_totalLines) {
            m_streaming = false;
            emit streamingFinished();
            return;
        }
        sendNextLine();
    }
}

void GcodeStreamer::onErrorReceived(const QString &/*error*/)
{
    if (!m_streaming) return;

    m_errorCount++;

    if (!m_paused) {
        if (m_okCount + m_errorCount >= m_commandsSent && m_linesSent >= m_totalLines) {
            m_streaming = false;
            emit streamingFinished();
            return;
        }
        sendNextLine();
    }
}

void GcodeStreamer::onDisconnected()
{
    if (m_streaming) {
        m_streaming = false;
        m_streamQueue.clear();
        m_linesSent = 0;
        m_commandsSent = 0;
        m_okCount = 0;
        m_errorCount = 0;
        emit streamingFinished();
    }
}

// ---------------------------------------------------------------------------
// sendNextLine — Push the next batch of G-code lines into SerialManager.
//
// SerialManager owns all buffer-level throttling via its internal TX queue
// and the character-counting protocol.  The streamer simply feeds lines
// into SerialManager::sendCommand(), which queues them if the GRBL RX
// buffer is currently full and drains them automatically when acks arrive.
//
// We cap the number of lines pushed per call to MAX_IN_FLIGHT_LINES to
// avoid loading the entire file into SerialManager's queue at once.
// ---------------------------------------------------------------------------
void GcodeStreamer::sendNextLine()
{
    if (m_linesSent >= m_totalLines) {
        // All lines have been submitted; wait for remaining acks.
        if (m_okCount + m_errorCount >= m_commandsSent) {
            m_streaming = false;
            emit streamingFinished();
        }
        return;
    }

    int pushed = 0;
    while (m_linesSent < m_totalLines && !m_paused && pushed < MAX_IN_FLIGHT_LINES) {
        QString line = m_streamQueue[m_linesSent].trimmed();

        // Skip blank lines and comments — GRBL never sees these, so
        // they don't produce acks and don't count as commands.
        if (line.isEmpty() || line.startsWith(';')) {
            m_linesSent++;
            emit streamingProgress(m_linesSent, m_totalLines);
            continue;
        }

        m_linesSent++;
        m_commandsSent++;
        pushed++;

        emit sendCommandRequested(line);
        emit streamingProgress(m_linesSent, m_totalLines);
    }
}