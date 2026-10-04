#include "plotting/PlotJobBuilder.h"
#include "grbl/ToolLibrary.h"
#include "grbl/GraphicsDesign/Document.h"
#include "grbl/GraphicsDesign/LineEntity.h"
#include <QCoreApplication>
#include <iostream>
#include "AutomaticRackFixture.h"
#include "plotting/RackJobValidator.h"
#include "plotting/PlotJobTransform.h"

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    ToolLibrary library;
    const auto black = library.addTool("Black", Qt::black);
    const auto red = library.addTool("Red", Qt::red);
    Document document;
    for (const auto& pen : {black, black, red, red, black}) {
        auto path = std::make_unique<LineEntity>(QPointF(10, 10), QPointF(20, 20));
        path->setToolId(pen);
        document.addEntity(std::move(path));
    }
    PenRackSettings rack;
    rack.configured = true;
    library.updateRackSettings(rack);
    int slot = 0;
    for (const auto& id : {black, red}) {
        auto profile = library.penRackStation(id);
        profile.configured = true;
        profile.slot = QString::number(++slot);
        profile.slotX = -20 * slot;
        profile.slotY = -20;
        profile.returnMacro = "G53 G1 X{slotX} F{dockFeed}";
        profile.pickupMacro = profile.returnMacro;
        library.updatePenRackStation(id, profile);
    }
    PlotOptions options;
    options.automaticRack = true;
    options.initialHolderKnown = true;
    const auto result = PlotJobBuilder::build(document, options, &library);
    if (!result.valid() || !result.job.containsRackOperations()) {
        std::cerr << "FAIL: ordered assigned paths produce an automatic rack job: "
                  << result.errors.join(';').toStdString() << '\n';
        return 1;
    }
    AutomaticRackFixture fixture;
    const auto job = fixture.job();
    int failures = 0, checks = 0;
    auto expect = [&](bool condition, const char* text) {
        ++checks; if (!condition) { ++failures; std::cerr << "FAIL: " << text << '\n'; }
    };
    const QVector3D offset(-100, -100, -10), position(-80, -80, -5);
    expect(RackJobValidator::validate(job, fixture.library, offset, position).isEmpty(), "calibrated bounded job passes preflight");
    expect(job.penSequence == QList<QUuid>{fixture.black, fixture.red, fixture.black}, "adjacent pens coalesce in path order");
    QList<QUuid> paths, pickups, returns;
    for (const auto& block : job.blocks) {
        if (block.kind == PlotJobBlock::Kind::Drawing && (paths.isEmpty() || paths.last() != block.pathId)) paths << block.pathId;
        if (block.holderEffect == PlotJobBlock::HolderEffect::Loaded) pickups << block.toolId;
        if (block.holderEffect == PlotJobBlock::HolderEffect::Empty) returns << block.toolId;
    }
    expect(paths == fixture.paths, "all path identities retain list order");
    expect(pickups == job.penSequence && returns == job.penSequence, "empty-start pickup and final-return counts match");
    expect(!PlotJobTransform::apply(job, {}).valid(), "managed job transforms preserve safety by refusing edits");
    expect(!RackJobValidator::validate(job, fixture.library, {}, position).isEmpty(), "unknown work offsets fail preflight");
    expect(!RackJobValidator::validate(job, fixture.library, offset, {}).isEmpty(), "unknown machine position fails preflight");
    expect(!RackJobValidator::validate(job, fixture.library, QVector3D{}, position).isEmpty(), "work-to-machine out-of-bounds fails preflight");
    expect(!RackJobValidator::validate(job, fixture.library, QVector3D(-100, -100, -2), position).isEmpty(), "work pen-up above machine clearance fails preflight");
    expect(!RackJobValidator::validate(job, fixture.library, offset, QVector3D(-500, 0, 0)).isEmpty(), "starting machine position outside bounds fails preflight");
    auto pen = fixture.library.penRackStation(fixture.black); pen.slotX -= 1; fixture.library.updatePenRackStation(fixture.black, pen);
    expect(!fixture.library.penRackStation(fixture.black).motionCalibrated, "station motion edit invalidates calibration");
    expect(!RackJobValidator::validate(job, fixture.library, offset, position).isEmpty(), "stale station snapshot fails preflight");
    auto changed = fixture.job();
    expect(!RackJobValidator::validate(changed, fixture.library, offset, position).isEmpty(), "uncalibrated station fails even with new program");
    pen = fixture.library.penRackStation(fixture.black); pen.motionCalibrated = true; fixture.library.updatePenRackStation(fixture.black, pen);
    auto rackSettings = fixture.library.rackSettings(); rackSettings.travelFeed += 1; fixture.library.updateRackSettings(rackSettings);
    expect(!fixture.library.rackSettings().motionCalibrated, "rack motion edit invalidates calibration");
    expect(!RackJobValidator::validate(fixture.job(), fixture.library, offset, position).isEmpty(), "uncalibrated rack fails preflight");
    fixture.options.initialToolId = fixture.black; fixture.options.returnFinalPen = false;
    const auto keepJob = fixture.job();
    int pickupCount = 0; for (const auto& block : keepJob.blocks) pickupCount += block.holderEffect == PlotJobBlock::HolderEffect::Loaded;
    expect(pickupCount == 2 && keepJob.blocks.last().phase == "Keep final pen raised", "initial loaded pen skips pickup and final keep stays raised");
    fixture.options.initialHolderKnown = false;
    expect(!PlotJobBuilder::build(fixture.document, fixture.options, &fixture.library).valid(), "unknown initial holder cannot compile");
    fixture.options.initialHolderKnown = true; fixture.options.safeZ = 0;
    expect(!PlotJobBuilder::build(fixture.document, fixture.options, &fixture.library).valid(), "automatic jobs require distinct higher pen-up Z");
    fixture.options.safeZ = 1.00005; fixture.options.drawingZ = 1.00004;
    expect(!PlotJobBuilder::build(fixture.document, fixture.options, &fixture.library).valid(), "serialized firmware heights cannot collapse pen-up and drawing Z");
    fixture.options.drawingZ = 0;
    fixture.options.safeZ = 5;
    fixture.document.entityAt(2)->setEnabled(false); fixture.document.entityAt(3)->setEnabled(false);
    expect(fixture.job().penSequence == QList<QUuid>{fixture.black}, "disabled paths do not cause spurious pen exchanges");
    for (const auto& path : fixture.document.entities()) path->setToolId({});
    expect(!PlotJobBuilder::build(fixture.document, fixture.options, &fixture.library).valid(), "automatic jobs refuse undefined manual pen");
    fixture.document.setDefaultToolId(fixture.red);
    expect(fixture.job().penSequence == QList<QUuid>{fixture.red}, "default pen resolves before ordered planning");
    ToolLibrary restored;
    restored.loadFromJson(fixture.library.toJson()); restored.loadRackFromJson(fixture.library.rackToJson());
    expect(restored.penRackStation(fixture.black).motionCalibrated && !restored.rackSettings().motionCalibrated,
           "restart preserves exact station and rack calibration flags");
    std::cout << checks << " planning/preflight checks, " << failures << " failures.\n";
    return failures ? 1 : 0;
}
