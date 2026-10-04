#pragma once
#include <QObject>
#include <QString>
#include <QUuid>

struct GrblCommand
{
    enum class Origin { Job, Manual, System };
    quint64 id = 0;
    QString text;
    QUuid jobId;
    int sourceLine = -1;
    Origin origin = Origin::Manual;
};

// Serial and simulated adapters correlate acknowledgements with FIFO commands.
class GrblTransport : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;
    virtual bool isConnected() const = 0;
    virtual bool isReady() const = 0;
    virtual bool submit(const GrblCommand& command) = 0;
    virtual void sendRealtimeCommand(char command) = 0;
    // In-flight accounting survives cancellation until reset/disconnect.
    virtual void clearPendingCommands() = 0;
signals:
    void connectedChanged(bool connected);
    void readyChanged(bool ready);
    void responseReceived(const QString& response);
    void commandCompleted(quint64 commandId, const QString& error);
    void errorOccurred(const QString& error);
    void commandSent(const QString& command);
    void controllerReset();
};
