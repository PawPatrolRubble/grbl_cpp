#include "FakeGrblTransport.h"
#include "grbl/GcodeStreamer.h"
#include "grbl/GrblCommandQueue.h"
#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>
#include <iostream>

namespace {
int checks = 0, failures = 0;
void expect(bool condition, const char* message) {
    ++checks;
    if (!condition) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
}
struct Fixture {
    FakeGrblTransport transport;
    GcodeStreamer controller;
    int successes = 0, endings = 0, rejected = 0, failedLine = -1;
    GcodeStreamer::Outcome outcome = GcodeStreamer::Outcome::Completed;
    QList<int> progress;
    Fixture() {
        controller.setTransport(&transport);
        transport.reportIdle();
        QObject::connect(&controller, &GcodeStreamer::streamingFinished, [&] { ++successes; });
        QObject::connect(&controller, &GcodeStreamer::commandRejected, [&](const QString&) { ++rejected; });
        QObject::connect(&controller, &GcodeStreamer::streamingProgress,
                         [&](int line, int) { progress << line; });
        QObject::connect(&controller, &GcodeStreamer::streamingEnded,
                         [&](GcodeStreamer::Outcome value, const QString&, int line) {
            ++endings; outcome = value; failedLine = line;
        });
    }
    void drain() { for (int i = 0; i < 300 && !transport.outstanding.isEmpty(); ++i) transport.acknowledgeNext(); }
};
void manualOwnership() {
    Fixture f;
    expect(f.controller.submitManualCommands({"$G", "$#"}, GrblCommand::Origin::System), "system batch accepted");
    f.controller.startStreaming({"; comment", "", "G1 X10 F100"});
    expect(f.controller.state() == GcodeStreamer::State::Preparing && f.transport.submitted.size() == 1,
           "job waits for preceding system commands");
    f.transport.acknowledgeNext();
    expect(f.progress.isEmpty() && f.transport.submitted.last().text == "$#", "system ack is not job progress");
    f.transport.acknowledgeNext();
    expect(f.transport.submitted.last().origin == GrblCommand::Origin::Job && f.progress.isEmpty(),
           "job acquires idle controller after preceding query acknowledgements");
    const auto jobId = f.transport.outstanding.head().id;
    f.transport.commandCompleted(jobId + 999, {});
    expect(f.progress.isEmpty(), "unknown acknowledgements ignored");
    expect(!f.controller.submitManualCommands({"G0 X20"}), "manual commands rejected while job owns controller");
    f.transport.acknowledgeNext();
    expect(f.progress == QList<int>{2} && f.successes == 0, "job progress uses source index and waits for motion completion");
    f.transport.commandCompleted(jobId, {});
    expect(f.progress.size() == 1, "duplicate acknowledgement ignored");
    f.transport.acknowledgeNext();
    expect(f.successes == 1 && f.endings == 1 && f.controller.readyToStart(), "checkpoint produces exactly one success");
}
void failuresAndCancellation() {
    Fixture f;
    QStringList lines;
    for (int i = 0; i < 100; ++i) lines << QString("G1 X%1 F100").arg(i);
    f.controller.startStreaming(lines);
    expect(f.transport.submitted.size() == 48, "bounded number of pending job commands");
    f.transport.acknowledgeNext("error:20");
    expect(f.outcome == GcodeStreamer::Outcome::Failed && f.failedLine == 0 && f.successes == 0,
           "first rejected block fails at its source line");
    expect(f.transport.submitted.size() == 48 && f.transport.outstanding.isEmpty() && f.transport.clearCount == 1,
           "failure stops submission and clears pending work");
    expect(f.transport.realtime.contains(GrblRealtime::Reset), "failure resets buffered motion");
    f.transport.restart();
    f.controller.startStreaming({"G1 X2 F100"});
    f.controller.stopStreaming();
    expect(f.outcome == GcodeStreamer::Outcome::Cancelled && f.successes == 0 && !f.controller.isStreaming(),
           "stop has distinct cancelled outcome");
    f.transport.restart();
    f.controller.startStreaming({"G1 X2 F100"});
    f.transport.disconnectDevice();
    expect(f.outcome == GcodeStreamer::Outcome::Disconnected && f.successes == 0, "disconnect cannot report success");
    Fixture rejected;
    rejected.transport.rejectNext = true;
    rejected.controller.startStreaming({"G0 X1"});
    expect(rejected.outcome == GcodeStreamer::Outcome::Failed && rejected.endings == 1, "submission failure terminates job");
    Fixture manual;
    manual.controller.submitManualCommands({"$100=80", "$101=80"});
    manual.transport.acknowledgeNext("error:3");
    expect(manual.transport.submitted.size() == 1 && manual.rejected == 1 && !manual.transport.ready,
           "manual rejection stops a configuration batch");
    Fixture startup;
    startup.transport.responseReceived("<Alarm|MPos:0,0,0>");
    startup.controller.submitManualCommands({"$G", "$#", "$$"}, GrblCommand::Origin::System);
    startup.drain();
    expect(startup.transport.submitted.size() == 3 && !startup.transport.realtime.contains(GrblRealtime::Reset),
           "startup queries in alarm do not send a rejected motion checkpoint and reset loop");
}
void pauseAndReset() {
    Fixture f;
    f.controller.startStreaming({"G1 X10 F100"});
    f.controller.pauseStreaming();
    f.transport.acknowledgeNext();
    expect(f.transport.outstanding.isEmpty() && f.successes == 0, "pause does not submit completion checkpoint");
    f.controller.resumeStreaming();
    expect(f.transport.outstanding.size() == 1, "resume submits checkpoint");
    f.controller.pauseStreaming();
    f.transport.acknowledgeNext();
    expect(f.successes == 0, "checkpoint acknowledgement while paused waits for resume");
    f.controller.resumeStreaming();
    expect(f.successes == 1 && f.endings == 1, "resume completes acknowledged checkpoint once");
    const QUuid pen = QUuid::createUuid();
    expect(f.controller.confirmHolder(GcodeStreamer::HolderState::Loaded, pen), "idle holder may be confirmed");
    f.controller.sendRealtime(GrblRealtime::Reset);
    expect(f.controller.holderState() == GcodeStreamer::HolderState::Unknown && !f.controller.readyToStart(),
           "reset invalidates known holder and readiness");
    f.transport.restart();
    f.controller.startStreaming({"G0 X1"});
    f.transport.controllerReset();
    expect(f.outcome == GcodeStreamer::Outcome::Failed && !f.controller.isStreaming(), "unexpected reset fails active job");
    Fixture synchronous;
    synchronous.transport.acknowledgeImmediately = true;
    synchronous.controller.startStreaming({"G0 X1", "G0 X2"});
    expect(synchronous.successes == 1 && synchronous.progress.size() == 2, "synchronous transport acknowledgement is reentrant safe");
}
void validationAndSnapshot() {
    Fixture eeprom;
    for (const auto& command : {QString("G10 L2 P1 X0"), QString("G28.1"), QString("G30.1")})
        eeprom.controller.startStreaming({command});
    expect(eeprom.rejected == 3 && eeprom.transport.submitted.isEmpty(), "EEPROM writes cannot enter character-counted jobs");
    Fixture f;
    f.controller.startStreaming({"G0 X1 !"});
    expect(f.rejected == 1 && f.transport.submitted.isEmpty(), "realtime bytes cannot enter normal job blocks");
    f.controller.startStreaming({"$100=80"});
    expect(f.rejected == 2 && f.transport.submitted.isEmpty(), "system configuration cannot enter buffered jobs");
    PlotJob rack = PlotJob::fromLines({"G53 G0 X-100"});
    rack.blocks[0].kind = PlotJobBlock::Kind::Rack;
    f.controller.startJob(rack);
    expect(f.rejected == 3 && f.transport.submitted.isEmpty(), "rack preview cannot execute");
    PlotJob job = PlotJob::fromLines({"G0 X1", "G0 X2"});
    f.transport.reportRun();
    f.controller.startJob(job);
    job.blocks[0].text = "G0 X999";
    f.transport.reportIdle();
    expect(f.transport.submitted.front().text == "G0 X1", "running job owns immutable snapshot");
    f.controller.startStreaming({"G0 X999"});
    expect(f.rejected == 4 && f.transport.submitted.size() == 2, "second start cannot replace active job");
    f.drain();
    expect(f.successes == 1, "snapshot job completes");
    Fixture fault;
    fault.controller.submitManualCommands({"G0 X1", "G0 X2"});
    fault.transport.errorOccurred("Simulated write failure");
    expect(fault.transport.outstanding.isEmpty() && !fault.controller.readyToStart(), "manual transport failure drops work and invalidates readiness");
    Fixture reentrant;
    QObject::connect(&reentrant.controller, &GcodeStreamer::stateChanged, [&](GcodeStreamer::State state) {
        if (state == GcodeStreamer::State::Completed) reentrant.controller.startStreaming({"G0 X999"});
    });
    reentrant.controller.startStreaming({"G0 X1"}); reentrant.drain();
    expect(reentrant.rejected == 1 && reentrant.successes == 1 && reentrant.transport.submitted.size() == 2,
           "reentrant start cannot replace job during outcome delivery");
}
void jogAndPreparation() {
    Fixture f;
    f.controller.submitManualCommands({"$J=G91 X100 F500"});
    f.transport.responseReceived("<Jog|MPos:0,0,0>");
    f.transport.acknowledgeNext();
    expect(f.transport.submitted.size() == 1, "ordinary checkpoint waits for jogging to finish");
    f.controller.startStreaming({"G0 X2"});
    expect(f.controller.state() == GcodeStreamer::State::Preparing, "job waits for active jog");
    f.transport.reportIdle();
    expect(f.transport.submitted.size() == 2 && f.transport.submitted.last().text == "G4 P0.01",
           "fresh idle report releases jog checkpoint");
    f.drain();
    expect(f.successes == 1, "job after jog completes");
    Fixture timeout;
    timeout.transport.reportRun();
    timeout.controller.setPreparationTimeout(5);
    timeout.controller.startStreaming({"G0 X1"});
    QEventLoop loop;
    QTimer::singleShot(30, &loop, &QEventLoop::quit);
    loop.exec();
    expect(timeout.outcome == GcodeStreamer::Outcome::Failed && timeout.endings == 1, "preparation timeout terminates waiting job");
}
void serialAccounting() {
    GrblCommandQueue queue;
    const GrblCommand first{1, QString(63, 'X'), {}, 8, GrblCommand::Origin::Manual};
    const GrblCommand second{2, QString(62, 'Y'), QUuid::createUuid(), 12, GrblCommand::Origin::Job};
    const GrblCommand third{3, "G0 X1", {}, 13, GrblCommand::Origin::Job};
    expect(queue.enqueue(first) && queue.enqueue(second) && queue.enqueue(third), "serial queue accepts identified commands");
    expect(!queue.enqueue(first), "serial queue rejects duplicate pending identity");
    expect(queue.takeWritable()->command.id == 1 && queue.takeWritable()->command.id == 2,
           "serial transmission preserves FIFO order");
    expect(queue.bytesUsed() == 127 && !queue.takeWritable(), "serial buffer counts UTF8 bytes including newlines");
    const auto ack = queue.acknowledge();
    expect(ack && ack->command.id == 1 && ack->command.origin == GrblCommand::Origin::Manual,
           "response resolves the oldest in-flight owner");
    expect(queue.bytesUsed() == 63 && queue.takeWritable()->command.id == 3, "acknowledgement releases exact byte capacity");
    queue.enqueue({4, "G0 X9"}); queue.clearWaiting();
    expect(queue.waitingCount() == 0 && queue.bytesUsed() == 69, "clear waiting preserves in-flight accounting");
    expect(queue.acknowledge()->command.sourceLine == 12 && queue.acknowledge()->command.id == 3,
           "clear waiting preserves in-flight acknowledgement identities");
    expect(queue.bytesUsed() == 0 && !queue.acknowledge(), "unsolicited acknowledgement cannot underflow buffer accounting");
    expect(!queue.enqueue({0, "G0 X1"}) && !queue.enqueue({5, QString(127, 'X')}), "serial queue rejects unowned and oversized commands");
    queue.enqueue(first); queue.takeWritable(); queue.reset();
    expect(queue.bytesUsed() == 0 && !queue.acknowledge() && queue.waitingCount() == 0, "reset removes all old command ownership");
}
}
int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    manualOwnership(); failuresAndCancellation(); pauseAndReset(); validationAndSnapshot(); jogAndPreparation(); serialAccounting();
    std::cout << checks << " controller checks, " << failures << " failures.\n";
    return failures ? 1 : 0;
}
