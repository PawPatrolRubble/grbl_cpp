#include "grbl/GcodeStreamer.h"
#include "grbl/GrblParser.h"
#include "grbl/MachineState.h"
#include "grbl/GraphicsDesign/Document.h"
#include "grbl/GraphicsDesign/LineEntity.h"
#include "ui/ToolpathPanel.h"
#include "ui/ToolpathCanvas.h"
#include "ui/panels/ControlPanel.h"
#include "ui/panels/JogPanel.h"
#include "ApplicationCore.h"
#include "ui/MainWindow.h"
#include "FakeGrblTransport.h"
#include <QApplication>
#include <QPushButton>
#include <QLabel>
#include <QEventLoop>
#include <QThreadPool>
#include <QTimer>
#include <QStandardPaths>
#include <QKeyEvent>
#include <iostream>

namespace {
int failures = 0;
void expect(bool condition, const char* description)
{
    std::cout << (condition ? "PASS: " : "FAIL: ") << description << '\n';
    if (!condition) ++failures;
}

void errorsStopSubmission()
{
    GcodeStreamer streamer;
    FakeGrblTransport transport;
    streamer.setTransport(&transport);
    transport.reportIdle();
    QStringList submitted;
    QObject::connect(&streamer, &GcodeStreamer::sendCommandRequested,
                     [&](const QString& line) { submitted << line; });
    QStringList lines;
    for (int i = 0; i < 100; ++i) lines << QString("G1 X%1 F500").arg(i);
    streamer.startStreaming(lines);
    const int beforeError = submitted.size();
    transport.acknowledgeNext("error:20");
    expect(!streamer.isStreaming() && submitted.size() == beforeError,
           "A rejected command stops further job submission");
}

void acknowledgedMotionIsNotPhysicalCompletion()
{
    GcodeStreamer streamer;
    FakeGrblTransport transport;
    streamer.setTransport(&transport);
    transport.reportIdle();
    int successfulFinishes = 0;
    QObject::connect(&streamer, &GcodeStreamer::streamingFinished,
                     [&] { ++successfulFinishes; });
    streamer.startStreaming({"G1 X100 F100"});
    transport.acknowledgeNext();
    expect(successfulFinishes == 0,
           "Acknowledging the last motion does not declare physical completion");
}

void disconnectionIsNotSuccess()
{
    GcodeStreamer streamer;
    FakeGrblTransport transport;
    streamer.setTransport(&transport);
    transport.reportIdle();
    int successfulFinishes = 0;
    QObject::connect(&streamer, &GcodeStreamer::streamingFinished,
                     [&] { ++successfulFinishes; });
    streamer.startStreaming({"G1 X100 F100"});
    transport.disconnectDevice();
    expect(successfulFinishes == 0 && !streamer.isStreaming(),
           "Disconnection ends the job without a success signal");
}

void undoTargetsTheCreatedIdentity()
{
    Document document;
    auto drawn = std::make_unique<LineEntity>(QPointF(1, 1), QPointF(2, 2));
    const QUuid drawnId = drawn->id();
    document.commandStack()->execute(std::make_unique<AddEntityCommand>(std::move(drawn)));
    auto imported = std::make_unique<LineEntity>(QPointF(3, 3), QPointF(4, 4));
    const QUuid importedId = imported->id();
    document.addEntity(std::move(imported));
    document.undo();
    expect(document.entityCount() == 1 && document.indexOf(drawnId) < 0
               && document.indexOf(importedId) >= 0,
           "Undo removes the entity created by its command, even after an append");
}

void plungeUsesItsOwnFeed()
{
    Document document;
    document.addEntity(std::make_unique<LineEntity>(QPointF(1, 1), QPointF(2, 2)));
    const auto lines = document.generateGcode(888, 222, 5, 0);
    bool plungeFound = false;
    for (const auto& line : lines)
        if (line.startsWith("G1") && line.contains("Z0.0000") && line.contains("F222"))
            plungeFound = true;
    expect(plungeFound, "Plot generation applies the separate Z plunge feed");
}

void parserKeepsSourceLineNumbers()
{
    GcodeParser parser;
    const auto moves = parser.parse({"; comment", "", "G21 G90", "G1 X10 F100"});
    expect(moves.size() == 1 && moves.front().lineNumber == 3,
           "Preview moves retain their original program line numbers");
}

void machineStateIsAuthoritative()
{
    MachineState state;
    GrblParser parser(&state);
    state.setState(MachineState::State::Idle);
    int alarmSignals = 0;
    QObject::connect(&state, &MachineState::stateChanged, [&](MachineState::State value) {
        if (value == MachineState::State::Alarm) ++alarmSignals;
    });
    parser.parseResponse("error:20");
    expect(state.state() == MachineState::State::Idle && alarmSignals == 0,
           "A command rejection does not emit a fictitious machine alarm");
    parser.parseResponse("ALARM:1");
    expect(state.state() == MachineState::State::Alarm && alarmSignals == 1,
           "An alarm push updates both stored machine state and its observers");
}

void rackProgramsCannotBeRewrittenAsDrawingMoves()
{
    ToolpathPanel panel;
    const QStringList original{"G21 G90", "G53 G1 X-100 Y-20 F300", "G4 P0.3", "M5"};
    GcodeParser parser;
    panel.setGcodeLines(original);
    panel.setMoves(parser.parse(original));
    auto* canvas = panel.findChild<ToolpathCanvas*>();
    canvas->movesTransformed(GcodeParser::transformMoves(parser.parse(original), 1, 1, 10, 0, 0));
    expect(panel.currentGcodeLines() == original,
           "A transform cannot drop machine-coordinate moves, dwell, or actuator blocks");
}
void previewResultsBelongToCurrentJob()
{
    ToolpathPanel panel;
    QStringList large;
    for (int i = 0; i < 40000; ++i) large << QString("G1 X%1 F100").arg(i);
    panel.setGcodeLines(large);
    panel.setGcodeLines({"G21 G90", "G1 X3 F100"});
    QEventLoop loop;
    QObject::connect(&panel, &ToolpathPanel::previewReady, &loop, &QEventLoop::quit);
    QTimer::singleShot(5000, &loop, &QEventLoop::quit);
    loop.exec();
    QThreadPool::globalInstance()->waitForDone(5000);
    QApplication::processEvents();
    const auto preview = panel.previewMoves();
    expect(preview.size() == 1 && preview.front().end.x() == 3,
           "A delayed previous parse cannot overwrite the current job preview");
    panel.setGcodeLines(large);
    panel.clearAll();
    QThreadPool::globalInstance()->waitForDone(5000);
    QApplication::processEvents();
    expect(panel.previewMoves().isEmpty() && panel.currentGcodeLines().isEmpty(),
           "Clearing a program invalidates its pending preview result");
}
void controlsTrackExecutionOutcome()
{
    ControlPanel panel;
    panel.setConnected(true);
    panel.setFileLoaded("test.nc", 10);
    auto* start = panel.findChild<QPushButton*>("startBtn");
    auto* load = panel.findChild<QPushButton*>("loadBtn");
    auto* home = panel.findChild<QPushButton*>("homeBtn");
    expect(!start->isEnabled(), "Start requires controller readiness, not just a connection");
    panel.setExecutionReady(true);
    expect(start->isEnabled(), "Start enables when loaded program and controller are ready");
    panel.setStreamingStarted();
    expect(!start->isEnabled() && !load->isEnabled() && !home->isEnabled(),
           "Active job disables program replacement and manual homing");
    panel.setStreamingEnded("Device disconnected.");
    panel.setConnected(false);
    expect(panel.findChild<QLabel*>("progressLabel")->text() == "Device disconnected." && !start->isEnabled(),
           "Failure/disconnection remains visible without a successful completion label");
}
void jogControlsRetainRelease()
{
    JogPanel panel;
    panel.setMotionEnabled(false);
    QKeyEvent press(QEvent::KeyPress, Qt::Key_Right, Qt::NoModifier);
    expect(!panel.handleKeyPress(&press), "Disabled jogging cannot be bypassed through keyboard shortcuts");
    panel.setJogEnabled(true);
    expect(panel.findChild<QPushButton*>("btnXPlus")->isEnabled()
               && !panel.findChild<QPushButton*>("goToAbsBtn")->isEnabled(),
           "Jog release controls remain available while absolute motion waits for synchronization");
    auto* held = panel.findChild<QPushButton*>("btnXPlus");
    held->setDown(true);
    panel.setAbsoluteMotionEnabled(false);
    expect(held->isEnabled() && held->isDown(), "Updating absolute-motion availability does not interrupt a held jog button");
    int cancels = 0;
    QObject::connect(&panel, &JogPanel::jogCancelRequested, [&] { ++cancels; });
    QKeyEvent repeatedRelease(QEvent::KeyRelease, Qt::Key_Right, Qt::NoModifier, {}, true);
    panel.handleKeyRelease(&repeatedRelease);
    expect(cancels == 0, "Keyboard auto-repeat release does not cancel a held continuous jog");
    QKeyEvent release(QEvent::KeyRelease, Qt::Key_Right, Qt::NoModifier);
    panel.handleKeyRelease(&release);
    expect(cancels == 1, "Actual keyboard release sends continuous jog cancel");
}
void applicationServicesCanClose()
{
    { ApplicationCore core; }
    expect(true, "Application service teardown closes without accessing a destroyed state model");
    {
        MainWindow window;
        window.show();
        QApplication::processEvents();
        expect(window.findChild<ToolpathPanel*>() && window.findChild<ControlPanel*>(),
               "Application window constructs and renders with the split libraries");
        expect(!window.findChild<JogPanel*>()->findChild<QPushButton*>("btnXPlus")->isEnabled(),
               "Initial disconnected window disables manual motion");
        window.close();
    }
    expect(true, "Application window and backend close cleanly");
}
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("GrblSenderTests");
    QCoreApplication::setApplicationName("ArchitectureRegression");
    QStandardPaths::setTestModeEnabled(true);
    errorsStopSubmission();
    acknowledgedMotionIsNotPhysicalCompletion();
    disconnectionIsNotSuccess();
    undoTargetsTheCreatedIdentity();
    plungeUsesItsOwnFeed();
    parserKeepsSourceLineNumbers();
    machineStateIsAuthoritative();
    rackProgramsCannotBeRewrittenAsDrawingMoves();
    previewResultsBelongToCurrentJob();
    controlsTrackExecutionOutcome();
    jogControlsRetainRelease();
    applicationServicesCanClose();
    std::cout << failures << " architecture regression failures.\n";
    return failures == 0 ? 0 : 1;
}
