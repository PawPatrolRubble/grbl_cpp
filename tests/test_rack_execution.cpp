#include "AutomaticRackFixture.h"
#include "FakeGrblTransport.h"
#include "grbl/GcodeStreamer.h"
#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>
#include <iostream>

namespace {
int checks = 0, failures = 0;
void expect(bool condition, const char* text) {
    ++checks; if (!condition) { ++failures; std::cerr << "FAIL: " << text << '\n'; }
}
void events(int ms) { QEventLoop loop; QTimer::singleShot(ms, &loop, &QEventLoop::quit); loop.exec(); }
void setup(GcodeStreamer& c, FakeGrblTransport& t) {
    c.setTransport(&t); t.reportIdle();
    c.setRackValidator([](const PlotJob&) { return QString{}; });
    c.confirmHomed(); c.confirmHolder(GcodeStreamer::HolderState::Empty);
}
void execution() {
    AutomaticRackFixture f; const auto job = f.job();
    FakeGrblTransport t; GcodeStreamer c; setup(c, t);
    int ended = 0, transitions = 0, maximumPending = 0;
    QObject::connect(&c, &GcodeStreamer::streamingEnded, [&](auto, auto, int) { ++ended; });
    QObject::connect(&c, &GcodeStreamer::holderStateChanged, [&](auto, auto) { ++transitions; });
    c.startJob(job);
    expect(c.isStreaming() && t.outstanding.size() == 1, "managed start submits one setup command");
    for (int guard = 0; c.isStreaming() && guard < 1000 && !t.outstanding.isEmpty(); ++guard) {
        maximumPending = std::max(maximumPending, int(t.outstanding.size()));
        const int line = t.outstanding.head().sourceLine;
        if (line >= 0) {
            const auto& block = job.blocks[line];
            if (block.kind != PlotJobBlock::Kind::Drawing)
                expect(t.outstanding.size() == 1, "rack and barriers drain earlier submissions");
            else expect(c.holderState() == GcodeStreamer::HolderState::Loaded && c.loadedToolId() == block.toolId,
                        "drawing waits for synchronized pickup");
            if (block.holderEffect == PlotJobBlock::HolderEffect::Loaded)
                expect(c.holderState() == GcodeStreamer::HolderState::Empty, "pickup state commits after checkpoint");
            if (block.holderEffect == PlotJobBlock::HolderEffect::Empty)
                expect(c.holderState() == GcodeStreamer::HolderState::Loaded, "return state commits after checkpoint");
        }
        t.acknowledgeNext();
    }
    expect(ended == 1 && c.state() == GcodeStreamer::State::Completed, "managed job completes exactly once");
    expect(c.holderState() == GcodeStreamer::HolderState::Empty && transitions == 6, "final return commits empty holder");
    expect(maximumPending > 1, "drawing retains buffered throughput");
}
void guardsAndFailures() {
    AutomaticRackFixture f; const auto job = f.job();
    {
        FakeGrblTransport t; GcodeStreamer c; c.setTransport(&t); t.reportIdle();
        c.startJob(job); expect(t.submitted.isEmpty(), "missing preflight refuses motion");
        setup(c, t); c.startJob(job, 1); expect(t.submitted.isEmpty(), "line-range execution is rejected");
        c.confirmHolder(GcodeStreamer::HolderState::Loaded, f.red);
        c.startJob(job); expect(t.submitted.isEmpty(), "mismatched holder refuses motion");
        c.confirmHolder(GcodeStreamer::HolderState::Empty);
        c.startStreaming(job.lines()); expect(t.submitted.isEmpty(), "flattened managed program refuses execution");
        auto altered = job; altered.blocks[2].text = "G53 G1 Z-90 F300";
        c.startJob(altered); expect(t.submitted.isEmpty(), "modified frozen program refuses execution");
    }
    for (int line = 1; line < job.blocks.size(); ++line) {
        if (job.blocks[line].kind == PlotJobBlock::Kind::Drawing) continue;
        FakeGrblTransport t; GcodeStreamer c; setup(c, t); c.startJob(job);
        for (int guard = 0; c.isStreaming() && guard < 1000 && !t.outstanding.isEmpty(); ++guard) {
            if (t.outstanding.head().sourceLine == line) {
                const int sent = int(t.submitted.size()); t.acknowledgeNext("error:20");
                expect(c.state() == GcodeStreamer::State::Failed && t.submitted.size() == sent,
                       "error in each rack phase stops submissions without recovery motion");
                expect(c.holderState() == GcodeStreamer::HolderState::Unknown && !c.homingConfirmed(),
                       "rack failure invalidates holder and homing confidence");
                break;
            }
            t.acknowledgeNext();
        }
    }
    {
        FakeGrblTransport t; GcodeStreamer c; setup(c, t); c.setExchangeTimeout(20); c.startJob(job);
        c.pauseStreaming(); const int sent = int(t.submitted.size()); events(45);
        expect(c.isPaused() && c.isStreaming(), "intentional pause suspends timeout");
        c.resumeStreaming(); expect(t.submitted.size() == sent, "resume does not replay motion"); events(40);
        expect(c.state() == GcodeStreamer::State::Failed && c.holderState() == GcodeStreamer::HolderState::Unknown,
               "timeout fails with unknown holder");
    }
    for (int interruption = 0; interruption < 3; ++interruption) {
        FakeGrblTransport t; GcodeStreamer c; setup(c, t); c.startJob(job);
        if (interruption == 0) c.stopStreaming();
        if (interruption == 1) t.disconnectDevice();
        if (interruption == 2) c.sendRealtime(GrblRealtime::Reset);
        expect(!c.isStreaming() && c.holderState() == GcodeStreamer::HolderState::Unknown && !c.homingConfirmed(),
               "cancel, disconnect and reset require inspection");
    }
    {
        FakeGrblTransport t; GcodeStreamer c; setup(c, t);
        t.acknowledgeImmediately = true;
        f.options.initialToolId = f.black; f.options.returnFinalPen = false;
        c.confirmHolder(GcodeStreamer::HolderState::Loaded, f.black);
        c.startJob(f.job());
        expect(c.state() == GcodeStreamer::State::Completed && c.loadedToolId() == f.black,
               "synchronous acknowledgements complete loaded-start keep job without reentrant submission");
    }
    {
        FakeGrblTransport t; GcodeStreamer c; setup(c, t);
        QObject::connect(&c, &GcodeStreamer::executionPhaseChanged, [&](auto, auto, auto) { c.stopStreaming(); });
        c.startJob(job);
        expect(c.state() == GcodeStreamer::State::Cancelled && t.submitted.isEmpty(),
               "cancellation from phase notification cannot submit stale motion");
    }
}
}
int main(int argc, char** argv) {
    QCoreApplication app(argc, argv); execution(); guardsAndFailures();
    std::cout << checks << " rack execution checks, " << failures << " failures.\n";
    return failures ? 1 : 0;
}
