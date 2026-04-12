#pragma once

#include <QObject>

class MachineState;
class SerialManager;
class GcodeStreamer;
class GrblParser;
class GcodeParser;
class FileManager;

/// Central service container for the application.
/// Owns and wires all backend services together, decoupling them from the UI.
class ApplicationCore : public QObject
{
    Q_OBJECT

public:
    explicit ApplicationCore(QObject *parent = nullptr);
    ~ApplicationCore() override = default;

    [[nodiscard]] MachineState    *machineState()    const { return m_machineState; }
    [[nodiscard]] SerialManager   *serialManager()   const { return m_serialManager; }
    [[nodiscard]] GcodeStreamer   *streamer()        const { return m_streamer; }
    [[nodiscard]] GrblParser      *grblParser()      const { return m_grblParser; }
    [[nodiscard]] GcodeParser     *gcodeParser()     const { return m_gcodeParser; }
    [[nodiscard]] FileManager     *fileManager()     const { return m_fileManager; }

    /// Wire internal service-to-service connections.
    /// UI code should connect to the signals exposed here, NOT to individual services directly.
    void wireServices();

signals:
    // Exposed signals for UI consumption
    void responseReceived(const QString &response);
    void connectedChanged(bool connected);
    void errorOccurred(const QString &error);
    void streamingProgress(int currentLine, int totalLines);
    void streamingStarted();
    void streamingFinished();
    void streamingPaused();
    void streamingResumed();

public slots:
    // Facade operations — UI calls these instead of individual services
    void startStreaming(const QStringList &lines);
    void pauseStreaming();
    void resumeStreaming();
    void stopStreaming();
    void sendCommand(const QString &cmd);
    void sendRealtime(char cmd);

private:
    MachineState  *m_machineState;
    SerialManager *m_serialManager;
    GcodeStreamer *m_streamer;
    GrblParser    *m_grblParser;
    GcodeParser   *m_gcodeParser;  // Stateless — safe to call from any thread
    FileManager   *m_fileManager;
};
