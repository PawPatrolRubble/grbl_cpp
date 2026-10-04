#pragma once
#include "GrblTransport.h"
#include <QQueue>
#include <optional>

// FIFO ownership and receive-buffer accounting, independent of serial hardware.
class GrblCommandQueue
{
public:
    struct Entry { GrblCommand command; QByteArray bytes; };
    static constexpr int receiveCapacity = 127;
    bool enqueue(const GrblCommand& command) {
        const QByteArray bytes = command.text.trimmed().toUtf8() + '\n';
        if (!command.id || bytes.size() < 2 || bytes.size() > receiveCapacity) return false;
        for (const auto& entry : m_waiting) if (entry.command.id == command.id) return false;
        for (const auto& entry : m_sent) if (entry.command.id == command.id) return false;
        m_waiting.enqueue({command, bytes});
        return true;
    }
    std::optional<Entry> takeWritable() {
        if (m_waiting.isEmpty() || m_used + m_waiting.head().bytes.size() > receiveCapacity) return {};
        auto entry = m_waiting.dequeue();
        m_used += entry.bytes.size();
        m_sent.enqueue(entry);
        return entry;
    }
    std::optional<Entry> acknowledge() {
        if (m_sent.isEmpty()) return {};
        auto entry = m_sent.dequeue();
        m_used -= entry.bytes.size();
        return entry;
    }
    void clearWaiting() { m_waiting.clear(); }
    void reset() { m_waiting.clear(); m_sent.clear(); m_used = 0; }
    int bytesUsed() const { return m_used; }
    int waitingCount() const { return m_waiting.size(); }
private:
    QQueue<Entry> m_waiting, m_sent;
    int m_used = 0;
};
