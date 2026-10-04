#include "ApplicationCore.h"
#include "grbl/MachineState.h"
#include "grbl/SerialManager.h"
#include "grbl/GcodeStreamer.h"
#include "grbl/GrblParser.h"
#include "grbl/GcodeParser.h"
#include "grbl/FileManager.h"
#include "grbl/GrblConstants.h"
#include "SettingsManager.h"
#include "plotting/RackJobValidator.h"
#include <QRegularExpression>

ApplicationCore::ApplicationCore(QObject *parent)
    : QObject(parent)
    , m_machineState(new MachineState(this))
    , m_serialManager(new SerialManager(m_machineState, this))
    , m_streamer(new GcodeStreamer(this))
    , m_grblParser(new GrblParser(m_machineState, this))
    , m_gcodeParser(new GcodeParser(this))
    , m_fileManager(new FileManager(this))
{
    wireServices();
}

void ApplicationCore::wireServices()
{
    // SerialManager → GrblParser (parse status reports)
    connect(m_serialManager, &SerialManager::responseReceived,
            this, &ApplicationCore::responseReceived);
    connect(m_serialManager, &SerialManager::responseReceived,
            m_grblParser, &GrblParser::parseResponse);

    connect(m_serialManager, &SerialManager::connectedChanged,
            this, &ApplicationCore::connectedChanged);

    // Startup queries use the same owner/correlation as manual commands.
    connect(m_serialManager, &GrblTransport::readyChanged, this, [this](bool ready) {
        if (ready) m_streamer->submitManualCommands({"$G", "$#", "$$"}, GrblCommand::Origin::System);
    });
    m_streamer->setTransport(m_serialManager);
    m_streamer->setRackValidator([this](const PlotJob& job) {
        return RackJobValidator::validate(job, *SettingsManager::instance().toolLibrary(),
            m_grblParser->workCoordinateOffset(job.workCoordinateSystem), m_grblParser->currentMachinePosition());
    });
    connect(m_serialManager, &GrblTransport::controllerReset, m_grblParser, &GrblParser::invalidateCoordinates);
    connect(m_serialManager, &GrblTransport::connectedChanged, this, [this](bool connected) {
        if (!connected) m_grblParser->invalidateCoordinates();
    });
    connect(m_serialManager, &GrblTransport::commandSent, this, [this](const QString& command) {
        static const QRegularExpression offsetChange("G0*(?:10|92(?:\\.[123])?|43\\.1|49)(?=[^0-9.]|$)");
        QString compact = command.toUpper(); compact.remove(' ');
        if (offsetChange.match(compact).hasMatch()) m_grblParser->invalidateOffsets();
    });
    auto refreshPosition = [this] {
        m_grblParser->invalidateMachinePosition();
        m_streamer->sendRealtime(GrblRealtime::StatusReport);
    };
    connect(m_streamer, &GcodeStreamer::manualMotionSynchronized, this, refreshPosition);
    connect(m_streamer, &GcodeStreamer::streamingEnded, this,
            [refreshPosition](GcodeStreamer::Outcome outcome, const QString&, int) {
                if (outcome == GcodeStreamer::Outcome::Completed) refreshPosition();
            });

    // GcodeStreamer → UI signals
    connect(m_streamer, &GcodeStreamer::streamingProgress,
            this, &ApplicationCore::streamingProgress);
    connect(m_streamer, &GcodeStreamer::streamingStarted,
            this, &ApplicationCore::streamingStarted);
    connect(m_streamer, &GcodeStreamer::streamingFinished,
            this, &ApplicationCore::streamingFinished);
    connect(m_streamer, &GcodeStreamer::streamingPaused,
            this, &ApplicationCore::streamingPaused);
    connect(m_streamer, &GcodeStreamer::streamingResumed,
            this, &ApplicationCore::streamingResumed);

    // Error forwarding
    connect(m_streamer, &GcodeStreamer::commandRejected,
            this, &ApplicationCore::errorOccurred);
    connect(m_streamer, &GcodeStreamer::streamingEnded,
            this, &ApplicationCore::streamingEnded);
    connect(m_streamer, &GcodeStreamer::availabilityChanged,
            this, &ApplicationCore::executionAvailabilityChanged);
    connect(m_streamer, &GcodeStreamer::executionPhaseChanged,
            this, &ApplicationCore::executionPhaseChanged);
}

// ---------------------------------------------------------------------------
// Facade operations
// ---------------------------------------------------------------------------

void ApplicationCore::startStreaming(const QStringList &lines, int startLine, int endLine)
{
    m_streamer->startStreaming(lines, startLine, endLine);
}

void ApplicationCore::startJob(const PlotJob& job, int startLine, int endLine)
{
    m_streamer->startJob(job, startLine, endLine);
}

bool ApplicationCore::confirmRackSetup(const QUuid& holderTool)
{
    return m_streamer->confirmHomed() && m_streamer->confirmHolder(holderTool.isNull()
        ? GcodeStreamer::HolderState::Empty : GcodeStreamer::HolderState::Loaded, holderTool);
}

void ApplicationCore::pauseStreaming()
{
    m_streamer->pauseStreaming();
}

void ApplicationCore::resumeStreaming()
{
    m_streamer->resumeStreaming();
}

void ApplicationCore::stopStreaming()
{
    m_streamer->stopStreaming();
}

void ApplicationCore::sendCommand(const QString &cmd)
{
    sendCommands({cmd});
}

void ApplicationCore::sendCommands(const QStringList& commands)
{
    m_streamer->submitManualCommands(commands);
}

void ApplicationCore::sendRealtime(char cmd)
{
    m_streamer->sendRealtime(cmd);
}

void ApplicationCore::jogStep(const QString& axis, double step, double feedRate)
{
    if (axis != "X" && axis != "Y" && axis != "Z") return;
    sendCommand(QString("$J=G91 %1%2 F%3").arg(axis).arg(step, 0, 'f', 3).arg(feedRate, 0, 'f', 0));
}
void ApplicationCore::jogContinuous(const QString& axis, int direction, double feedRate)
{
    jogStep(axis, direction >= 0 ? 9999.0 : -9999.0, feedRate);
}
void ApplicationCore::jogCancel() { sendRealtime(GrblRealtime::JogCancel); }
