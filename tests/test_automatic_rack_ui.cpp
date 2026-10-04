#include "AutomaticRackFixture.h"
#include "FakeGrblTransport.h"
#include "ApplicationCore.h"
#include "SettingsManager.h"
#include "grbl/GrblParser.h"
#include "grbl/MachineState.h"
#include "ui/RackJobStartDialog.h"
#include "ui/ToolsDialog.h"
#include "ui/DesignCanvas.h"
#include "ui/panels/GraphicsDesignPanel.h"
#include "ui/ToolpathPanel.h"
#include "ui/ToolpathCanvas.h"
#include "ui/GcodeListWidget.h"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QDialog>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QScrollArea>
#include <QLabel>
#include <QListWidget>
#include <QDir>
#include <QEventLoop>
#include <QTimer>
#include <QStandardPaths>
#include <QFontDatabase>
#include <QFile>
#include <QStyleFactory>
#include <iostream>

namespace {
int checks = 0, failures = 0;
void expect(bool condition, const char* text) {
    ++checks; if (!condition) { ++failures; std::cerr << "FAIL: " << text << '\n'; }
}
void readCoordinates(GrblParser& parser) {
    parser.parseResponse("[G54:-100,-100,-10]");
    parser.parseResponse("[G92:0,0,0]"); parser.parseResponse("[TLO:0]");
    parser.parseResponse("<Idle|MPos:-80,-80,-5|WCO:-100,-100,-10>");
}
void coordinateConfidence() {
    MachineState state; GrblParser parser(&state);
    expect(!parser.workCoordinateOffset(54) && !parser.currentMachinePosition(), "coordinates start unknown");
    readCoordinates(parser);
    expect(parser.workCoordinateOffset(54) == QVector3D(-100, -100, -10), "selected work offset includes queried G92 and TLO");
    parser.parseResponse("[G55:-50,-60,-20]");
    expect(parser.workCoordinateOffset(55) == QVector3D(-50, -60, -20), "preflight resolves selected WCS independently of active frame");
    parser.parseResponse("[G92:1,2,3]"); parser.parseResponse("[TLO:4]");
    expect(parser.workCoordinateOffset(54) == QVector3D(-99, -98, -3), "work translation includes temporary and tool-length offsets");
    parser.parseResponse("[G54:nan,0,0]"); expect(!parser.workCoordinateOffset(54), "malformed offset removes coordinate confidence");
    parser.parseResponse("<Idle|MPos:bad,0,0>"); expect(!parser.currentMachinePosition(), "malformed live position cannot become machine zero");
    readCoordinates(parser); parser.parseResponse("Grbl 1.1h ['$' for help]");
    expect(!parser.workCoordinateOffset(54) && !parser.currentMachinePosition(), "reset invalidates cached coordinates");
    parser.parseResponse("<Idle|WPos:0,0,0>");
    expect(!parser.currentMachinePosition(), "work-only position needs known WCO before deriving machine coordinates");
}
void operatorSetup(const QString& screenshotDirectory) {
    AutomaticRackFixture f; RackJobStartDialog dialog(f.job(), f.library);
    auto* start = dialog.findChild<QPushButton*>("rackStartButton");
    auto* holder = dialog.findChild<QComboBox*>("confirmedHolderCombo");
    expect(!start->isEnabled(), "Start dialog requires physical confirmations");
    for (const char* name : {"rackStartHomedCheck", "rackStartOccupancyCheck", "rackStartHeightsCheck"})
        dialog.findChild<QCheckBox*>(name)->setChecked(true);
    holder->setCurrentIndex(holder->findData(QVariant::fromValue(f.red)));
    expect(!start->isEnabled(), "operator cannot start with a different pen than compiled expectation");
    holder->setCurrentIndex(1);
    expect(start->isEnabled() && dialog.holderToolId().isNull(), "confirmed empty start enables complete job");
    if (!screenshotDirectory.isEmpty()) {
        dialog.show(); QApplication::processEvents(); dialog.grab().save(screenshotDirectory + "/rack-start.png");
    }
}
void designAndCalibration(const QString& screenshotDirectory) {
    AutomaticRackFixture f;
    auto* library = SettingsManager::instance().toolLibrary();
    library->loadFromJson(f.library.toJson()); library->loadRackFromJson(f.library.rackToJson());
    GraphicsDesignPanel panel;
    for (const auto& path : f.document.entities()) panel.canvas()->document()->addEntity(path->clone());
    auto* automatic = panel.findChild<QCheckBox*>("automaticRackCheck");
    auto* initial = panel.findChild<QComboBox*>("initialHolderCombo");
    auto* generate = panel.findChild<QPushButton*>("startBtn");
    automatic->setChecked(true);
    expect(generate->isEnabled() && !generate->toolTip().isEmpty(), "automatic generation offers setup for an unknown initial holder");
    initial->setCurrentIndex(1);
    expect(generate->isEnabled(), "configured automatic design generates without machine connection");
    const auto status = panel.findChild<QLabel*>("toolAssignmentStatus")->text();
    expect(status.contains("Black → Red → Black") && status.contains("3 pickups, 3 returns"), "design shows ordered pen sequence and exchange counts");
    expect(initial->findData(QVariant::fromValue(f.black)) >= 0, "initial holder selector includes defined pens");
    PlotJob emitted;
    QObject::connect(&panel, &GraphicsDesignPanel::plotJobGenerated, [&](const PlotJob& job) { emitted = job; });
    generate->click(); expect(emitted.automaticRack && !emitted.executionSeal.isEmpty(), "Generate emits managed metadata");
    ToolpathPanel toolpath;
    bool ready = false; QObject::connect(&toolpath, &ToolpathPanel::previewReady, [&](auto) { ready = true; });
    toolpath.setPlotJob(emitted);
    QEventLoop loop;
    QObject::connect(&toolpath, &ToolpathPanel::previewReady, &loop, &QEventLoop::quit);
    QTimer::singleShot(2000, &loop, &QEventLoop::quit); if (!ready) loop.exec();
    const auto moves = toolpath.previewMoves();
    bool correctFrame = !moves.isEmpty();
    for (const auto& move : moves)
        correctFrame = correctFrame && emitted.blocks[move.lineNumber].kind == PlotJobBlock::Kind::Drawing
                       && move.end.x() >= 0 && move.end.y() >= 0;
    expect(ready && correctFrame, "managed preview renders drawing frame with original source indices");
    expect(!toolpath.returnToOriginEnabled(), "managed job excludes legacy origin finish motion");
    ToolsDialog rackDialog(*library);
    auto* selected = rackDialog.findChild<QListWidget*>("toolDefinitionList");
    selected->setCurrentRow(0);
    auto* calibrated = rackDialog.findChild<QCheckBox*>("penStationCalibratedCheck");
    auto* x = rackDialog.findChild<QDoubleSpinBox*>("penSlotX");
    expect(calibrated->isChecked(), "station editor loads saved calibration");
    x->setValue(x->value() - 1); QMetaObject::invokeMethod(x, "editingFinished");
    expect(!calibrated->isChecked() && !library->penRackStation(f.black).motionCalibrated, "station edit clears UI and stored calibration");
    calibrated->setChecked(true);
    expect(library->penRackStation(f.black).motionCalibrated, "operator can confirm the edited station after calibration");
    if (!screenshotDirectory.isEmpty()) {
        panel.resize(1250, 1000); panel.show(); QApplication::processEvents();
        panel.grab().save(screenshotDirectory + "/automatic-design.png");
        rackDialog.show(); QApplication::processEvents(); rackDialog.grab().save(screenshotDirectory + "/rack-calibration.png");
    }
}
void mixedToolsKeepGenerateAvailable(const QString& screenshotDirectory) {
    AutomaticRackFixture f;
    auto* library = SettingsManager::instance().toolLibrary();
    library->loadFromJson(f.library.toJson()); library->loadRackFromJson(f.library.rackToJson());
    GraphicsDesignPanel panel;
    auto* generate = panel.findChild<QPushButton*>("startBtn");
    expect(!generate->isEnabled(), "empty designs keep Generate disabled");
    for (int index : {0, 2}) panel.canvas()->document()->addEntity(f.document.entityAt(index)->clone());
    auto* automatic = panel.findChild<QCheckBox*>("automaticRackCheck");
    auto* initial = panel.findChild<QComboBox*>("initialHolderCombo");
    expect(panel.canvas()->document()->plottingToolIds().size() == 2, "reproduction uses two assigned tools");
    expect(generate->isEnabled(), "mixed tools keep Generate available for tool-change setup");
    int emittedCount = 0;
    PlotJob emitted;
    QObject::connect(&panel, &GraphicsDesignPanel::plotJobGenerated, [&](const PlotJob& job) {
        ++emittedCount; emitted = job;
    });
    auto chooseHolder = [&](const QUuid& tool, bool cancel) {
        QTimer::singleShot(0, &panel, [&, tool, cancel]() {
            auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
            expect(dialog && dialog->objectName() == "rackGenerationSetupDialog", "Generate opens tool-change setup");
            if (!dialog) return;
            auto* holder = dialog->findChild<QComboBox*>("generationInitialHolderCombo");
            auto* buttons = dialog->findChild<QDialogButtonBox*>("rackGenerationButtons");
            if (!holder || !buttons) { dialog->reject(); return; }
            expect(holder->currentData().isValid() || !buttons->button(QDialogButtonBox::Ok)->isEnabled(),
                   "setup cannot generate without an explicit holder selection");
            if (cancel) { buttons->button(QDialogButtonBox::Cancel)->click(); return; }
            holder->setCurrentIndex(holder->findData(QVariant::fromValue(tool)));
            expect(buttons->button(QDialogButtonBox::Ok)->isEnabled(), "selecting empty or loaded holder enables setup Generate");
            if (!screenshotDirectory.isEmpty()) {
                QApplication::processEvents();
                expect(dialog->grab().save(screenshotDirectory + "/mixed-tool-generation-setup.png"), "tool-change setup screenshot saves");
            }
            buttons->button(QDialogButtonBox::Ok)->click();
        });
        generate->click();
    };
    chooseHolder({}, true);
    expect(emittedCount == 0 && !automatic->isChecked() && !initial->currentData().isValid(),
           "cancelling setup emits no job and preserves generation options");
    chooseHolder({}, false);
    expect(emittedCount == 1 && emitted.automaticRack && emitted.initialHolderKnown && emitted.initialToolId.isNull(),
           "mixed tools generate a managed automatic job after choosing an empty holder");
    expect(emitted.penSequence == QList<QUuid>{f.black, f.red} && !emitted.executionSeal.isEmpty(),
           "generated mixed-tool job retains both tools in path order and execution metadata");
    expect(automatic->isChecked() && initial->currentIndex() == 1,
           "accepted setup updates visible rack and initial-holder controls");
    int pickups = 0;
    for (const auto& block : emitted.blocks) pickups += block.holderEffect == PlotJobBlock::HolderEffect::Loaded;
    expect(pickups == 2, "empty-holder job contains the initial pickup and tool exchange");
    automatic->setChecked(false);
    chooseHolder(f.black, false);
    expect(emittedCount == 2 && emitted.initialToolId == f.black, "setup supports an already loaded initial tool");
    pickups = 0;
    for (const auto& block : emitted.blocks) pickups += block.holderEffect == PlotJobBlock::HolderEffect::Loaded;
    expect(pickups == 1, "loaded initial tool is not picked up again");
    initial->setCurrentIndex(0);
    expect(generate->isEnabled(), "automatic mode with an unknown holder keeps setup accessible");
    chooseHolder(f.black, false);
    expect(emittedCount == 3 && emitted.initialHolderKnown && emitted.initialToolId == f.black,
           "Generate resolves an unknown holder when automatic mode is already enabled");
    if (!screenshotDirectory.isEmpty()) {
        panel.resize(1250, 1000); panel.show(); QApplication::processEvents();
        panel.findChild<QScrollArea*>("designControlsScroll")->ensureWidgetVisible(generate);
        QApplication::processEvents();
        expect(panel.grab().save(screenshotDirectory + "/mixed-tool-generation.png"), "enabled mixed-tool design screenshot saves");
    }
    auto station = library->penRackStation(f.red); station.configured = false;
    library->updatePenRackStation(f.red, station);
    QTimer::singleShot(0, &panel, [&]() {
        auto* warning = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        expect(warning && warning->text().contains("rack", Qt::CaseInsensitive), "invalid rack setup reports generation errors");
        if (warning) warning->accept();
    });
    generate->click();
    expect(emittedCount == 3, "invalid station cannot emit a stale or incomplete mixed-tool job");
    auto unavailable = *library->tool(f.red); unavailable.enabled = false;
    library->updateTool(unavailable);
    QTimer::singleShot(0, &panel, [&]() {
        auto* warning = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        expect(warning && warning->text().contains("unavailable"), "unavailable mixed-tool assignment reports an error");
        if (warning) warning->accept();
    });
    generate->click();
    expect(emittedCount == 3, "unavailable mixed tools cannot emit a job");

    // Single-tool manual generation remains a one-click operation.
    GraphicsDesignPanel single;
    single.canvas()->document()->addEntity(f.document.entityAt(0)->clone());
    PlotJob manual;
    QObject::connect(&single, &GraphicsDesignPanel::plotJobGenerated, [&](const PlotJob& job) { manual = job; });
    single.findChild<QPushButton*>("startBtn")->click();
    expect(!manual.automaticRack && !manual.blocks.isEmpty(), "single-tool manual plotting generates without rack setup");
}
void coreExecution() {
    AutomaticRackFixture f;
    auto* tools = SettingsManager::instance().toolLibrary();
    tools->loadFromJson(f.library.toJson()); tools->loadRackFromJson(f.library.rackToJson());
    FakeGrblTransport transport; ApplicationCore core;
    core.streamer()->setTransport(&transport);
    QObject::connect(&transport, &GrblTransport::responseReceived, core.grblParser(), &GrblParser::parseResponse);
    readCoordinates(*core.grblParser());
    transport.responseReceived("<Idle|MPos:-80,-80,-5|WCO:-100,-100,-10>");
    expect(core.confirmRackSetup({}), "application facade confirms operator homing and empty holder");
    core.startJob(f.job());
    expect(core.streamer()->isStreaming(), "ApplicationCore performs real rack preflight before execution");
    for (int guard = 0; core.streamer()->isStreaming() && guard < 1000; ++guard) transport.acknowledgeNext();
    expect(core.streamer()->state() == GcodeStreamer::State::Completed, "ApplicationCore managed flow completes against simulated transport");
    expect(!core.grblParser()->currentMachinePosition(), "completion requires a new position report before the next rack preflight");
}
}
int main(int argc, char** argv) {
    QApplication app(argc, argv);
    const int font = QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeui.ttf");
    if (font >= 0) app.setFont(QFont(QFontDatabase::applicationFontFamilies(font).first(), 10));
    QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeuib.ttf");
    QFontDatabase::addApplicationFont("C:/Windows/Fonts/seguisym.ttf");
    QFontDatabase::addApplicationFont("C:/Windows/Fonts/consola.ttf");
    QApplication::setStyle(QStyleFactory::create("Fusion"));
    QFile style(":/resources/style.qss");
    if (style.open(QIODevice::ReadOnly)) app.setStyleSheet(style.readAll());
    QCoreApplication::setOrganizationName("GrblSenderTests");
    QCoreApplication::setApplicationName("AutomaticRackUi"); QStandardPaths::setTestModeEnabled(true);
    QString screenshots;
    const auto args = app.arguments(); const int flag = int(args.indexOf("--screenshots"));
    if (flag >= 0 && flag + 1 < args.size()) { screenshots = args[flag + 1]; QDir().mkpath(screenshots); }
    coordinateConfidence(); operatorSetup(screenshots); designAndCalibration(screenshots);
    mixedToolsKeepGenerateAvailable(screenshots); coreExecution();
    std::cout << checks << " automatic UI/core checks, " << failures << " failures.\n";
    return failures ? 1 : 0;
}
