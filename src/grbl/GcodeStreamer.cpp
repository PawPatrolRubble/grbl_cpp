#include "GcodeStreamer.h"
#include "GcodeParser.h"
#include "GrblConstants.h"
#include <QScopedValueRollback>
#include <QRegularExpression>

GcodeStreamer::GcodeStreamer(QObject* parent) : QObject(parent)
{
    m_prepareTimer.setSingleShot(true);
    m_prepareTimer.setInterval(30000);
    connect(&m_prepareTimer, &QTimer::timeout, this, [this] {
        if (m_state == State::Preparing)
            finish(Outcome::Failed, "Timed out waiting for pending commands and an idle controller.", -1, true);
    });
    m_exchangeTimer.setSingleShot(true);
    connect(&m_exchangeTimer, &QTimer::timeout, this, [this] {
        if (isStreaming() && !isPaused()) {
            int line = -1;
            if (!m_jobPending.isEmpty()) line = m_jobPending.constBegin()->sourceLine;
            finish(Outcome::Failed, "Timed out during " + m_activePhase + ". Inspect the holder and rack before restarting.", line, true);
        }
    });
}
bool GcodeStreamer::isStreaming() const
{
    return m_state == State::Preparing || m_state == State::Running
        || m_state == State::Paused || m_state == State::Synchronizing;
}
bool GcodeStreamer::readyToStart() const
{
    return m_transport && m_transport->isConnected() && m_transport->isReady()
        && m_machineIdle && !m_waitJogComplete && !isStreaming()
        && m_manualPending.isEmpty() && m_manualQueue.isEmpty();
}
void GcodeStreamer::setState(State state)
{
    if (m_state == state) return;
    m_state = state;
    emit stateChanged(state);
    emit availabilityChanged(readyToStart());
}
void GcodeStreamer::setTransport(GrblTransport* transport)
{
    if (m_transport == transport) return;
    if (isStreaming()) finish(Outcome::Disconnected, "The active transport was replaced.", -1, true);
    if (m_transport) disconnect(m_transport, nullptr, this, nullptr);
    m_transport = transport;
    m_homingConfirmed = false;
    confirmHolder(HolderState::Unknown);
    m_machineIdle = false;
    m_waitJogComplete = false;
    m_manualPending.clear();
    m_manualQueue.clear();
    if (!transport) return;
    connect(transport, &GrblTransport::commandCompleted, this, &GcodeStreamer::onCommandCompleted);
    connect(transport, &GrblTransport::responseReceived, this, &GcodeStreamer::onResponse);
    connect(transport, &GrblTransport::controllerReset, this, &GcodeStreamer::onReset);
    connect(transport, &GrblTransport::connectedChanged, this, [this](bool connected) {
        if (!connected) {
            m_homingConfirmed = false;
            m_machineIdle = false;
            m_waitJogComplete = false;
            if (isStreaming()) finish(Outcome::Disconnected, "Device disconnected.", -1, false);
            m_manualQueue.clear();
            m_manualPending.clear();
            confirmHolder(HolderState::Unknown);
        }
        emit availabilityChanged(readyToStart());
    });
    connect(transport, &GrblTransport::readyChanged, this, [this](bool) {
        pumpManual();
        pump();
        emit availabilityChanged(readyToStart());
    });
    connect(transport, &GrblTransport::errorOccurred, this, [this](const QString& error) {
        if (isStreaming()) finish(Outcome::Failed, error, -1, true);
        else {
            m_homingConfirmed = false;
            m_machineIdle = false;
            m_manualQueue.clear();
            m_manualPending.clear();
            confirmHolder(HolderState::Unknown);
            if (m_transport) {
                m_transport->clearPendingCommands();
                if (m_transport->isReady()) m_transport->sendRealtimeCommand(GrblRealtime::Reset);
            }
            emit commandRejected(error);
            emit availabilityChanged(false);
        }
    });
    connect(transport, &QObject::destroyed, this, [this] {
        m_homingConfirmed = false;
        confirmHolder(HolderState::Unknown);
        if (isStreaming()) finish(Outcome::Disconnected, "Transport closed.");
        emit availabilityChanged(false);
    });
    emit availabilityChanged(readyToStart());
}
QString GcodeStreamer::validateCommand(const QString& command)
{
    if (command.toUtf8().size() > 79) return "Command exceeds the GRBL 79-byte block limit.";
    for (const QChar character : command)
        if (character.unicode() < 32 || character.unicode() > 126)
            return "Commands must contain printable ASCII only.";
        else if (character == '!' || character == '~' || character == '?')
            return "Send realtime commands through the realtime controls, not normal blocks.";
    return {};
}
void GcodeStreamer::startStreaming(const QStringList& lines, int startLine, int endLine)
{
    startJob(PlotJob::fromLines(lines), startLine, endLine);
}
void GcodeStreamer::startJob(const PlotJob& job, int startLine, int endLine)
{
    if (isStreaming() || m_finishing || m_pumping) { emit commandRejected("A job already owns the controller."); return; }
    if (!m_transport || !m_transport->isConnected()) {
        emit commandRejected("Connect to the machine before starting a job.");
        return;
    }
    if (!job.automaticRack && job.containsRackOperations()) {
        emit commandRejected("Rack commands require a complete managed automatic job."); return;
    }
    if (!job.automaticRack) for (const auto& block : job.blocks) {
        if (block.text.contains("GRBL_CPP_MANAGED_RACK")) {
            emit commandRejected("A flattened rack program has lost its execution metadata. Regenerate the design job."); return;
        }
    }
    if (job.automaticRack) {
        if (startLine != 0 || (endLine >= 0 && endLine != job.blocks.size() - 1)) {
            emit commandRejected("Automatic rack jobs must run from beginning to end; clear the line range."); return;
        }
        if (!readyToStart() || !m_homingConfirmed) {
            emit commandRejected("Automatic rack jobs require an idle ready controller and confirmed machine homing."); return;
        }
        const bool expectedEmpty = job.initialToolId.isNull();
        if (!job.initialHolderKnown || m_holderState == HolderState::Unknown
            || (expectedEmpty ? m_holderState != HolderState::Empty
                              : m_holderState != HolderState::Loaded || m_loadedToolId != job.initialToolId)) {
            emit commandRejected("Confirm the holder contents matching this job's initial pen before Start."); return;
        }
        if (job.executionSeal.isEmpty() || job.executionSeal != job.executionDigest()) {
            emit commandRejected("The automatic program was modified after compilation. Regenerate it."); return;
        }
        if (!m_rackValidator) { emit commandRejected("Rack execution preflight is unavailable."); return; }
        const QString error = m_rackValidator(job);
        if (!error.isEmpty()) { emit commandRejected(error); return; }
    }
    const int total = int(job.blocks.size());
    const int first = qBound(0, startLine, total);
    const int last = endLine < 0 ? total : qBound(first, endLine + 1, total);
    QList<GrblCommand> commands;
    for (int i = first; i < last; ++i) {
        const QString text = GcodeParser::stripComments(job.blocks[i].text).trimmed();
        if (text.isEmpty()) continue;
        static const QRegularExpression eepromWrite("G0*(?:10(?:\\.0+)?|28\\.1|30\\.1)(?=[^0-9.]|$)");
        QString compact = text.toUpper();
        compact.remove(' ');
        const QString error = text.contains('$') || eepromWrite.match(compact).hasMatch()
            ? "System and EEPROM-writing commands must be submitted separately in manual mode."
            : validateCommand(text);
        if (!error.isEmpty()) {
            emit commandRejected(QString("Line %1: %2").arg(i + 1).arg(error));
            return;
        }
        commands.append({m_nextCommandId++, text, job.id, i, GrblCommand::Origin::Job});
    }
    if (commands.isEmpty()) { emit commandRejected("The selected range has no commands."); return; }
    m_job = job;
    m_commands = std::move(commands);
    m_totalLines = total;
    m_nextBlock = 0;
    m_jobPending.clear();
    m_finalCheckpointSent = false;
    m_finalCheckpointAcknowledged = false;
    m_exchangeTimer.stop();
    m_pausedExchangeTime = -1;
    m_activePhase.clear();
    setState(State::Preparing);
    emit streamingStarted();
    m_prepareTimer.start();
    m_transport->sendRealtimeCommand(GrblRealtime::StatusReport);
    pumpManual();
    pump();
}
bool GcodeStreamer::submitManualCommands(const QStringList& lines, GrblCommand::Origin origin)
{
    if (isStreaming() && !(origin == GrblCommand::Origin::System && m_state == State::Preparing)) {
        emit commandRejected("A job owns the controller. Pause/stop the job before submitting manual commands.");
        return false;
    }
    if (!m_transport || !m_transport->isConnected() || !m_transport->isReady()) {
        emit commandRejected("The controller is not ready for commands.");
        return false;
    }
    QStringList commands;
    for (const QString& line : lines) {
        const QString text = GcodeParser::stripComments(line).trimmed();
        if (text.isEmpty()) continue;
        const QString error = validateCommand(text);
        if (!error.isEmpty()) { emit commandRejected(error); return false; }
        commands << text;
    }
    if (commands.isEmpty()) return false;
    for (const QString& text : commands)
        m_manualQueue.enqueue({m_nextCommandId++, text, {}, -1, origin});
    // Queries/EEPROM writes use simple send-response, which also works in Alarm.
    // Only motion batches need a dwell: sending G4 after startup queries while
    // locked would be rejected and cause repeated resets before the user unlocks.
    bool synchronize = false;
    for (const auto& text : commands)
        synchronize = synchronize || !text.startsWith('$') || text.startsWith("$J", Qt::CaseInsensitive)
            || text.compare("$H", Qt::CaseInsensitive) == 0;
    if (synchronize)
        m_manualQueue.enqueue({m_nextCommandId++, "G4 P0.01", {}, -1, GrblCommand::Origin::System});
    emit availabilityChanged(false);
    pumpManual();
    return true;
}
void GcodeStreamer::pumpManual()
{
    if (m_pumpingManual || m_waitJogComplete || isPaused() || !m_transport || !m_transport->isReady()) return;
    QScopedValueRollback<bool> guard(m_pumpingManual, true);
    while (!m_waitJogComplete && !isPaused() && m_transport->isReady()
           && m_manualPending.isEmpty() && !m_manualQueue.isEmpty()) {
        const auto command = m_manualQueue.dequeue();
        m_manualPending.insert(command.id, command.text);
        if (!m_transport->submit(command)) {
            m_manualPending.remove(command.id);
            m_manualQueue.clear();
            if (isStreaming()) finish(Outcome::Failed, "A preceding command could not be submitted.", -1, true);
            else emit commandRejected("The manual command could not be submitted.");
            break;
        }
    }
}
void GcodeStreamer::pump()
{
    if (m_pumping || !isStreaming() || isPaused() || !m_transport || !m_transport->isReady()) return;
    if (m_state == State::Preparing) {
        if (!m_manualQueue.isEmpty() || !m_manualPending.isEmpty() || m_waitJogComplete || !m_machineIdle) return;
        m_prepareTimer.stop();
        setState(State::Running);
    }
    QScopedValueRollback<bool> guard(m_pumping, true);
    while (isStreaming() && !isPaused() && m_nextBlock < m_commands.size()
           && m_jobPending.size() < maxPendingJobCommands) {
        const auto& next = m_commands[m_nextBlock];
        const auto& block = m_job.blocks[next.sourceLine];
        const bool exclusive = m_job.automaticRack && block.kind != PlotJobBlock::Kind::Drawing;
        if (exclusive && !m_jobPending.isEmpty()) return;
        if (m_job.automaticRack && !m_jobPending.isEmpty()) {
            const auto& pending = m_job.blocks[m_jobPending.constBegin()->sourceLine];
            if (pending.kind != PlotJobBlock::Kind::Drawing) return;
        }
        if (m_job.automaticRack && block.phase != m_activePhase) {
            m_activePhase = block.phase;
            emit executionPhaseChanged(block.phase, block.toolId, block.pathId);
            if (!isStreaming() || isPaused()) return;
        }
        const auto command = m_commands[m_nextBlock++];
        m_jobPending.insert(command.id, {command.sourceLine, false});
        if (m_job.automaticRack && !m_exchangeTimer.isActive())
            m_exchangeTimer.start(m_exchangeTimeoutOverride > 0 ? m_exchangeTimeoutOverride
                : exclusive ? block.timeoutMs : m_job.phaseTimeoutMs);
        if (!m_transport->submit(command)) {
            finish(Outcome::Failed, "The command could not be submitted.", command.sourceLine, true);
            return;
        }
        emit sendCommandRequested(command.text);
        if (exclusive && !m_jobPending.isEmpty()) return;
    }
    if (!isStreaming() || isPaused()) return;
    if (m_nextBlock == m_commands.size() && m_jobPending.isEmpty() && !m_finalCheckpointSent) {
        m_finalCheckpointSent = true;
        setState(State::Synchronizing);
        const GrblCommand checkpoint{m_nextCommandId++, "G4 P0.01", m_job.id, -1, GrblCommand::Origin::Job};
        m_jobPending.insert(checkpoint.id, {-1, true});
        if (m_job.automaticRack)
            m_exchangeTimer.start(m_exchangeTimeoutOverride > 0 ? m_exchangeTimeoutOverride : m_job.phaseTimeoutMs);
        if (!m_transport->submit(checkpoint)) {
            finish(Outcome::Failed, "The completion checkpoint could not be submitted.", -1, true);
            return;
        }
        emit sendCommandRequested(checkpoint.text);
    }
}
void GcodeStreamer::onCommandCompleted(quint64 commandId, const QString& error)
{
    if (m_manualPending.contains(commandId)) {
        const QString text = m_manualPending.take(commandId);
        if (!error.isEmpty()) {
            m_manualQueue.clear();
            if (isStreaming()) finish(Outcome::Failed, "Preceding command " + text + ": " + error, -1, true);
            else {
                if (m_transport) {
                    m_transport->clearPendingCommands();
                    m_transport->sendRealtimeCommand(GrblRealtime::Reset);
                }
                emit commandRejected(text + ": " + error);
            }
            return;
        }
        if (text.startsWith("$J", Qt::CaseInsensitive)) {
            m_waitJogComplete = true;
            m_machineIdle = false;
            m_transport->sendRealtimeCommand(GrblRealtime::StatusReport);
        }
        if (text == "G4 P0.01" && m_manualQueue.isEmpty()) emit manualMotionSynchronized();
        pumpManual();
        pump();
        emit availabilityChanged(readyToStart());
        return;
    }
    if (!isStreaming() || !m_jobPending.contains(commandId)) return;
    const auto pending = m_jobPending.take(commandId);
    if (m_jobPending.isEmpty()) m_exchangeTimer.stop();
    else if (m_job.automaticRack && !isPaused())
        m_exchangeTimer.start(m_exchangeTimeoutOverride > 0 ? m_exchangeTimeoutOverride : m_job.phaseTimeoutMs);
    if (!error.isEmpty()) {
        finish(Outcome::Failed, error, pending.sourceLine, true);
        return;
    }
    if (pending.finalCheckpoint) {
        m_finalCheckpointAcknowledged = true;
        if (!isPaused()) finish(Outcome::Completed, "Plotting complete.");
    } else {
        if (m_job.automaticRack) {
            const auto& block = m_job.blocks[pending.sourceLine];
            if (block.holderEffect != PlotJobBlock::HolderEffect::None) {
                m_holderState = block.holderEffect == PlotJobBlock::HolderEffect::Empty ? HolderState::Empty : HolderState::Loaded;
                m_loadedToolId = m_holderState == HolderState::Loaded ? block.toolId : QUuid{};
                emit holderStateChanged(m_holderState, m_loadedToolId);
            }
        }
        emit streamingProgress(pending.sourceLine, m_totalLines);
        pump();
    }
}
void GcodeStreamer::pauseStreaming()
{
    if (!isStreaming() || isPaused()) return;
    m_beforePause = m_state;
    m_prepareTimer.stop();
    m_pausedExchangeTime = m_exchangeTimer.remainingTime();
    m_exchangeTimer.stop();
    setState(State::Paused);
    if (m_transport) m_transport->sendRealtimeCommand(GrblRealtime::FeedHold);
    emit streamingPaused();
}
void GcodeStreamer::resumeStreaming()
{
    if (!isPaused()) return;
    setState(m_beforePause);
    if (m_state == State::Preparing) m_prepareTimer.start();
    if (m_pausedExchangeTime >= 0 && !m_jobPending.isEmpty()) m_exchangeTimer.start(std::max(1, m_pausedExchangeTime));
    m_pausedExchangeTime = -1;
    if (m_transport) m_transport->sendRealtimeCommand(GrblRealtime::CycleStart);
    emit streamingResumed();
    if (m_finalCheckpointAcknowledged) finish(Outcome::Completed, "Plotting complete.");
    else { pumpManual(); pump(); }
}
void GcodeStreamer::stopStreaming()
{
    if (isStreaming()) finish(Outcome::Cancelled, "Plotting cancelled.", -1, true);
}
void GcodeStreamer::sendRealtime(char command)
{
    if (!m_transport) return;
    if (command == GrblRealtime::FeedHold && isStreaming()) { pauseStreaming(); return; }
    if (command == GrblRealtime::CycleStart && isPaused()) { resumeStreaming(); return; }
    if (command == GrblRealtime::JogCancel) {
        m_manualQueue.clear();
        m_machineIdle = false;
        m_waitJogComplete = true;
        emit availabilityChanged(false);
    }
    if (command == GrblRealtime::Reset && isStreaming()) {
        finish(Outcome::Cancelled, "Controller reset cancelled plotting.", -1, true);
        return;
    }
    m_transport->sendRealtimeCommand(command);
}
bool GcodeStreamer::confirmHolder(HolderState state, const QUuid& tool)
{
    if (state != HolderState::Unknown && !readyToStart()) return false;
    if (state == HolderState::Loaded && tool.isNull()) return false;
    m_holderState = state;
    m_loadedToolId = state == HolderState::Loaded ? tool : QUuid{};
    emit holderStateChanged(m_holderState, m_loadedToolId);
    return true;
}
void GcodeStreamer::onReset()
{
    m_homingConfirmed = false;
    m_machineIdle = false;
    m_waitJogComplete = false;
    m_manualQueue.clear();
    m_manualPending.clear();
    confirmHolder(HolderState::Unknown);
    if (isStreaming()) finish(Outcome::Failed, "Controller reset interrupted plotting.", -1, false);
    emit availabilityChanged(false);
}
void GcodeStreamer::onResponse(const QString& response)
{
    if (response.startsWith('<') && response.endsWith('>')) {
        m_machineIdle = response.startsWith("<Idle|") || response == "<Idle>";
        if (m_machineIdle) m_waitJogComplete = false;
        if (response.startsWith("<Alarm")) {
            m_homingConfirmed = false;
            confirmHolder(HolderState::Unknown);
        }
        if (response.startsWith("<Alarm") && isStreaming())
            finish(Outcome::Failed, "Machine entered an alarm state.", -1, true);
        else { pumpManual(); pump(); emit availabilityChanged(readyToStart()); }
    } else if (response.startsWith("ALARM:")) {
        m_homingConfirmed = false;
        m_machineIdle = false;
        if (isStreaming()) finish(Outcome::Failed, response, -1, true);
        else {
            m_manualQueue.clear();
            m_manualPending.clear();
            if (m_transport) m_transport->clearPendingCommands();
            confirmHolder(HolderState::Unknown);
            emit commandRejected(response);
        }
    }
}
void GcodeStreamer::finish(Outcome outcome, const QString& message, int sourceLine, bool reset)
{
    if (m_finishing) return;
    QScopedValueRollback<bool> guard(m_finishing, true);
    m_prepareTimer.stop();
    m_exchangeTimer.stop();
    m_waitJogComplete = false;
    m_jobPending.clear();
    m_commands.clear();
    m_manualQueue.clear();
    m_manualPending.clear();
    switch (outcome) {
    case Outcome::Completed: setState(State::Completed); break;
    case Outcome::Failed: setState(State::Failed); break;
    case Outcome::Cancelled: setState(State::Cancelled); break;
    case Outcome::Disconnected: setState(State::Disconnected); break;
    }
    if (outcome != Outcome::Completed) {
        m_homingConfirmed = false;
        confirmHolder(HolderState::Unknown);
    }
    if (reset && m_transport && m_transport->isConnected()) {
        m_transport->clearPendingCommands();
        m_transport->sendRealtimeCommand(GrblRealtime::FeedHold);
        m_transport->sendRealtimeCommand(GrblRealtime::Reset);
    }
    QString detail = message;
    if (m_job.automaticRack) {
        if (outcome == Outcome::Completed)
            detail += m_holderState == HolderState::Empty ? " Holder empty at rack clearance."
                                                         : " Final pen remains loaded and raised.";
        else detail += " Interrupted phase: " + m_activePhase
            + ". Holder contents are unknown; inspect the holder/rack, home, and regenerate with the actual initial pen before restarting.";
    }
    emit streamingEnded(outcome, detail, sourceLine);
    if (outcome == Outcome::Completed) emit streamingFinished();
    emit availabilityChanged(readyToStart());
}
