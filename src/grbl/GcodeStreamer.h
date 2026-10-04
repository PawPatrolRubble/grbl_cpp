#pragma once
#include "GrblTransport.h"
#include "plotting/PlotJob.h"
#include <QHash>
#include <QPointer>
#include <QQueue>
#include <QTimer>
#include <functional>

// Owns the complete job lifecycle and all ordinary manual/system submissions.
class GcodeStreamer : public QObject
{
    Q_OBJECT
public:
    enum class State { Idle, Preparing, Running, Paused, Synchronizing, Completed, Failed, Cancelled, Disconnected };
    enum class Outcome { Completed, Failed, Cancelled, Disconnected };
    enum class HolderState { Unknown, Empty, Loaded };
    Q_ENUM(State)
    Q_ENUM(Outcome)
    Q_ENUM(HolderState)
    explicit GcodeStreamer(QObject* parent = nullptr);
    void setTransport(GrblTransport* transport);
    void startStreaming(const QStringList& lines, int startLine = 0, int endLine = -1);
    void startJob(const PlotJob& job, int startLine = 0, int endLine = -1);
    void pauseStreaming();
    void resumeStreaming();
    void stopStreaming();
    bool submitManualCommands(const QStringList& lines, GrblCommand::Origin origin = GrblCommand::Origin::Manual);
    void sendRealtime(char command);
    bool isStreaming() const;
    bool isPaused() const { return m_state == State::Paused; }
    bool readyToStart() const;
    State state() const { return m_state; }
    HolderState holderState() const { return m_holderState; }
    QUuid loadedToolId() const { return m_loadedToolId; }
    bool confirmHolder(HolderState state, const QUuid& tool = {});
    bool confirmHomed() { if (!readyToStart()) return false; m_homingConfirmed = true; return true; }
    bool homingConfirmed() const { return m_homingConfirmed; }
    void setRackValidator(std::function<QString(const PlotJob&)> validator) { m_rackValidator = std::move(validator); }
    void setExchangeTimeout(int ms) { m_exchangeTimeoutOverride = ms; }
    void setPreparationTimeout(int ms) { m_prepareTimer.setInterval(ms); }
signals:
    void sendCommandRequested(const QString& command); // observation, not a transport path
    void streamingStarted();
    void streamingFinished(); // successful, physically synchronized completion only
    void streamingEnded(GcodeStreamer::Outcome outcome, const QString& message, int sourceLine);
    void streamingPaused();
    void streamingResumed();
    void streamingProgress(int currentLine, int totalLines); // acknowledged source index
    void stateChanged(GcodeStreamer::State state);
    void availabilityChanged(bool ready);
    void holderStateChanged(GcodeStreamer::HolderState state, const QUuid& tool);
    void commandRejected(const QString& message);
    void executionPhaseChanged(const QString& phase, const QUuid& tool, const QUuid& path);
    void manualMotionSynchronized();
private:
    struct JobPending { int sourceLine = -1; bool finalCheckpoint = false; };
    void setState(State state);
    void pump();
    void pumpManual();
    void onCommandCompleted(quint64 commandId, const QString& error);
    void onResponse(const QString& response);
    void onReset();
    void finish(Outcome outcome, const QString& message, int sourceLine = -1, bool reset = false);
    static QString validateCommand(const QString& command);
    QPointer<GrblTransport> m_transport;
    State m_state = State::Idle;
    State m_beforePause = State::Running;
    HolderState m_holderState = HolderState::Unknown;
    QUuid m_loadedToolId;
    PlotJob m_job;
    QList<GrblCommand> m_commands;
    QHash<quint64, JobPending> m_jobPending;
    QHash<quint64, QString> m_manualPending;
    QQueue<GrblCommand> m_manualQueue;
    quint64 m_nextCommandId = 1;
    int m_nextBlock = 0;
    int m_totalLines = 0;
    bool m_machineIdle = false;
    bool m_waitJogComplete = false;
    bool m_pumping = false;
    bool m_pumpingManual = false;
    bool m_finishing = false;
    bool m_finalCheckpointSent = false;
    bool m_finalCheckpointAcknowledged = false;
    QTimer m_prepareTimer;
    QTimer m_exchangeTimer;
    int m_pausedExchangeTime = -1;
    int m_exchangeTimeoutOverride = 0;
    bool m_homingConfirmed = false;
    std::function<QString(const PlotJob&)> m_rackValidator;
    QString m_activePhase;
    static constexpr int maxPendingJobCommands = 48;
};
