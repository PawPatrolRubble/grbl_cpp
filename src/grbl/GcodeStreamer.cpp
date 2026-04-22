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

void GcodeStreamer::startStreaming(const QStringList &lines, int startLine, int endLine)
{
    m_streamQueue = lines;
    m_totalLines = (endLine >= 0 && endLine < lines.size()) ? endLine + 1 : lines.size();
    m_linesSent = qBound(0, startLine, m_totalLines);
    m_commandsSent = 0;
    m_okCount = 0;
    m_errorCount = 0;
    m_lastProgressLine = -1;
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
    if (!m_streaming) return;

    m_streaming = false;
    m_paused = false;
    m_streamQueue.clear();
    m_linesSent = 0;
    m_commandsSent = 0;
    m_okCount = 0;
    m_errorCount = 0;

    // Flush queued commands that haven't been sent yet.
    if (m_serialManager)
        m_serialManager->clearQueue();

    // Feed hold → stop GRBL motion immediately, then soft reset
    // to discard anything still in GRBL's planner buffer.
    emit sendRealtimeRequested(GrblRealtime::Reset);

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
// sendNextLine — Push lines into SerialManager's TX queue in batches.
//
// Instead of enqueuing all lines at once (which would block the UI thread
// for thousands of lines), we limit to MAX_PENDING_COMMANDS per call.
// When acks arrive, onAckReceived() calls this again to feed more lines.
// Progress signals are also throttled to avoid per-line UI updates.
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

    // Limit how many commands we enqueue per call so we don't block
    // the UI thread when thousands of lines are queued at once.
    int pending = m_serialManager ? m_serialManager->pendingQueueSize() : 0;

    while (m_linesSent < m_totalLines && !m_paused
           && pending < MAX_PENDING_COMMANDS) {
        QString line = m_streamQueue[m_linesSent].trimmed();

        // Skip blank lines and comments — GRBL never sees these, so
        // they don't produce acks and don't count as commands.
        if (line.isEmpty() || line.startsWith(';') || line.startsWith('(')) {
            m_linesSent++;
            continue;
        }

        // Strip inline comments to reduce bytes sent over serial.
        int semicolonPos = line.indexOf(';');
        if (semicolonPos >= 0)
            line = line.left(semicolonPos).trimmed();
        // Strip parenthesized comments: (...)
        int parenPos = line.indexOf('(');
        if (parenPos >= 0) {
            int closePos = line.indexOf(')', parenPos);
            if (closePos >= 0)
                line = (line.left(parenPos) + line.mid(closePos + 1)).trimmed();
        }

        if (line.isEmpty()) {
            m_linesSent++;
            continue;
        }

        m_linesSent++;
        m_commandsSent++;
        ++pending;

        emit sendCommandRequested(line);
    }

    // Throttle progress updates — emit at most every 1% or 50 lines,
    // whichever is smaller, plus always on the final line.
    int step = qMax(1, qMin(m_totalLines / 100, 50));
    if (m_linesSent >= m_totalLines
        || m_linesSent - m_lastProgressLine >= step) {
        m_lastProgressLine = m_linesSent;
        emit streamingProgress(m_linesSent, m_totalLines);
    }
}