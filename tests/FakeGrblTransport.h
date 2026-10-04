#pragma once
#include "grbl/GrblTransport.h"
#include "grbl/GrblConstants.h"
#include <QQueue>

class FakeGrblTransport : public GrblTransport
{
public:
    bool connected = true;
    bool ready = true;
    bool rejectNext = false;
    bool acknowledgeImmediately = false;
    QList<GrblCommand> submitted;
    QQueue<GrblCommand> outstanding;
    QByteArray realtime;
    int clearCount = 0;
    bool isConnected() const override { return connected; }
    bool isReady() const override { return ready; }
    bool submit(const GrblCommand& command) override {
        if (!connected || !ready || rejectNext) { rejectNext = false; return false; }
        submitted << command;
        outstanding.enqueue(command);
        if (acknowledgeImmediately) acknowledgeNext();
        return true;
    }
    void sendRealtimeCommand(char command) override {
        realtime.append(command);
        if (command == GrblRealtime::Reset) {
            outstanding.clear();
            ready = false;
            emit readyChanged(false);
            emit controllerReset();
        }
    }
    void clearPendingCommands() override { ++clearCount; outstanding.clear(); }
    void acknowledgeNext(const QString& error = {}) {
        if (outstanding.isEmpty()) return;
        const auto command = outstanding.dequeue();
        emit commandCompleted(command.id, error);
    }
    void reportIdle() { emit responseReceived("<Idle|MPos:0,0,0>"); }
    void reportRun() { emit responseReceived("<Run|MPos:0,0,0>"); }
    void disconnectDevice() {
        connected = false;
        ready = false;
        outstanding.clear();
        emit readyChanged(false);
        emit connectedChanged(false);
    }
    void restart() {
        ready = true;
        emit readyChanged(true);
        reportIdle();
    }
};
