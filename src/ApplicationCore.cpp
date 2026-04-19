#include "ApplicationCore.h"
#include "grbl/MachineState.h"
#include "grbl/SerialManager.h"
#include "grbl/GcodeStreamer.h"
#include "grbl/GrblParser.h"
#include "grbl/GcodeParser.h"
#include "grbl/FileManager.h"

ApplicationCore::ApplicationCore(QObject *parent)
    : QObject(parent)
    , m_machineState(new MachineState(this))
    , m_serialManager(new SerialManager(m_machineState, this))
    , m_streamer(new GcodeStreamer(this))
    , m_grblParser(new GrblParser(m_machineState, this))
    , m_gcodeParser(new GcodeParser(this))
    , m_fileManager(new FileManager(this))
{
    m_streamer->setSerialManager(m_serialManager);
    wireServices();
}

void ApplicationCore::wireServices()
{
    // SerialManager → GrblParser (parse status reports)
    connect(m_serialManager, &SerialManager::responseReceived,
            this, &ApplicationCore::responseReceived);
    connect(m_serialManager, &SerialManager::responseReceived,
            m_grblParser, &GrblParser::parseResponse);

    // SerialManager → GcodeStreamer (ack/error routing)
    connect(m_serialManager, &SerialManager::ackReceived,
            m_streamer, &GcodeStreamer::onAckReceived);
    connect(m_serialManager, &SerialManager::commandErrorReceived,
            m_streamer, &GcodeStreamer::onErrorReceived);
    connect(m_serialManager, &SerialManager::connectedChanged,
            this, &ApplicationCore::connectedChanged);

    // Disconnect → stop streamer
    connect(m_serialManager, &SerialManager::connectedChanged,
            this, [this](bool connected) {
                if (!connected)
                    m_streamer->onDisconnected();
                else {
                    // Request current G-code state and coordinate offsets
                    // so WCO is initialized immediately after connecting.
                    m_serialManager->sendCommand("$G");
                    m_serialManager->sendCommand("$#");
                }
            });

    // GcodeStreamer → SerialManager (command output)
    connect(m_streamer, &GcodeStreamer::sendCommandRequested,
            this, [this](const QString &cmd) { m_serialManager->sendCommand(cmd); });
    connect(m_streamer, &GcodeStreamer::sendRealtimeRequested,
            this, [this](char cmd) { m_serialManager->sendRealtimeCommand(cmd); });

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
    connect(m_serialManager, &SerialManager::errorOccurred,
            this, &ApplicationCore::errorOccurred);
}

// ---------------------------------------------------------------------------
// Facade operations
// ---------------------------------------------------------------------------

void ApplicationCore::startStreaming(const QStringList &lines)
{
    m_streamer->startStreaming(lines);
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
    m_serialManager->sendCommand(cmd);
}

void ApplicationCore::sendRealtime(char cmd)
{
    m_serialManager->sendRealtimeCommand(cmd);
}
