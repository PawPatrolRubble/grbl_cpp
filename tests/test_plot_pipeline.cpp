#include "plotting/PlotJobBuilder.h"
#include "plotting/PlotJobTransform.h"
#include "grbl/ToolLibrary.h"
#include "grbl/GraphicsDesign/Document.h"
#include "grbl/GraphicsDesign/LineEntity.h"
#include "grbl/GraphicsDesign/GroupEntity.h"
#include <QCoreApplication>
#include <limits>
#include <iostream>

namespace {
int checks = 0, failures = 0;
void expect(bool condition, const char* message) {
    ++checks;
    if (!condition) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
}
std::unique_ptr<LineEntity> line(double x = 0) {
    return std::make_unique<LineEntity>(QPointF(x, 0), QPointF(x + 10, 10));
}
void buildJobs() {
    Document doc;
    ToolLibrary pens;
    const auto pen = pens.addTool("Blue", Qt::blue);
    doc.setDefaultToolId(pen);
    auto path = line();
    const auto pathId = path->id();
    doc.addEntity(std::move(path));
    const PlotOptions options{800, 222, 5, 0, 55};
    auto result = PlotJobBuilder::build(doc, options, &pens);
    expect(result.valid() && result.job.lines().front().contains("G55"), "builder emits explicit configured drawing modes");
    int plunge = 0, drawing = 0;
    for (const auto& block : result.job.blocks) {
        if (block.kind != PlotJobBlock::Kind::Drawing) continue;
        ++drawing;
        expect(block.pathId == pathId && block.toolId == pen && block.frame == PlotJobBlock::Frame::Work,
               "drawing block retains path, effective pen and coordinate frame");
        if (block.text.startsWith("G1") && block.text.contains("Z0.0000") && block.text.contains("F222")) ++plunge;
    }
    expect(drawing > 0 && plunge == 1, "builder distinguishes plunge and drawing feeds");
    expect(!PlotJobBuilder::build(doc, options).valid(), "missing library cannot bypass pen validation");
    auto disabled = *pens.tool(pen); disabled.enabled = false; pens.updateTool(disabled);
    expect(!PlotJobBuilder::build(doc, options, &pens).valid(), "disabled pen blocks plotting");
    disabled.enabled = true; pens.updateTool(disabled);
    pens.removeTool(pen);
    expect(!PlotJobBuilder::build(doc, options, &pens).valid(), "removed pen reference blocks plotting");
    doc.setDefaultToolId({});
    PlotOptions bad = options; bad.plungeFeed = 0;
    expect(!PlotJobBuilder::build(doc, bad).valid(), "zero Z feed rejected centrally");
    bad = options; bad.safeZ = std::numeric_limits<double>::infinity();
    expect(!PlotJobBuilder::build(doc, bad).valid(), "nonfinite options rejected centrally");
    bad = options; bad.safeZ = bad.drawingZ;
    expect(!PlotJobBuilder::build(doc, bad).valid(), "coincident pen-up/drawing heights rejected");
    bad = options; bad.workCoordinateSystem = 60;
    expect(!PlotJobBuilder::build(doc, bad).valid(), "unsupported work frame rejected");
    doc.entityAt(0)->setEnabled(false);
    expect(!PlotJobBuilder::build(doc, options).valid(), "empty effective design rejected");
}
void programPreservation() {
    GcodeParser parser;
    PlotJob job = PlotJob::fromLines({"; start", "G21 G90 G17 G54", "M3 S200", "G0", "X1 Y2",
                                     "G4 P0.3", "G1 X10 Y20 F321 M8 (draw)", "M5", "; finish"});
    const auto original = parser.parse(job.lines());
    expect(original.size() == 2 && original[0].lineNumber == 4, "modal-only blocks are not geometric motion");
    const auto changed = PlotJobTransform::apply(job, GcodeParser::transformMoves(original, 1, 1, 5, 7, 0));
    expect(changed.valid() && changed.job.blocks.size() == job.blocks.size(), "transform preserves all original program blocks");
    for (int index : {0, 1, 2, 3, 5, 7, 8})
        expect(changed.job.blocks[index].text == job.blocks[index].text, "setup, dwell, modal, auxiliary and comment blocks preserved exactly");
    expect(changed.job.blocks[6].text.contains("F321") && changed.job.blocks[6].text.contains("M8")
               && changed.job.blocks[6].text.contains("(draw)"), "inline feed/auxiliary/comment retained");
    const auto reparsed = parser.parse(changed.job.lines());
    expect(reparsed.size() == 2 && reparsed.last().end == QVector3D(15, 27, 0), "transformed output matches drawing geometry");
    for (const QString& unsupported : {QString("G20"), QString("G91"), QString("G53 G0 X-20"),
                                       QString("G90.1"), QString("G18"), QString("G10 L2 P1 X0")}) {
        auto special = PlotJob::fromLines({"G21 G90 G54", unsupported, "G1 X10 F100"});
        expect(!PlotJobTransform::apply(special, parser.parse(special.lines())).valid(), "unsupported mode/frame rejected before transformation");
    }
    auto arc = PlotJob::fromLines({"G21 G90", "G0 X0 Y0", "G2 X10 Y0 R5 F100"});
    auto scaled = PlotJobTransform::apply(arc, GcodeParser::transformMoves(parser.parse(arc.lines()), 2, 2, 0, 0, 0));
    expect(scaled.valid() && scaled.job.lines().last().contains("R10.0000"), "uniform scaling preserves radius-form arcs");
    auto ellipse = PlotJobTransform::apply(arc, GcodeParser::transformMoves(parser.parse(arc.lines()), 2, 1, 0, 0, 0));
    expect(!ellipse.valid() && ellipse.job.lines() == arc.lines(), "nonuniform arc scaling is rejected without changing program");
}
void undoTransactions() {
    Document doc;
    auto existing = line(); const auto existingId = existing->id(); doc.addEntity(std::move(existing));
    std::vector<std::unique_ptr<Entity>> imported;
    imported.push_back(line(20)); imported.push_back(line(40));
    const auto importedId = imported[0]->id();
    int changes = 0;
    QObject::connect(&doc, &Document::changed, [&] { ++changes; });
    doc.importEntities(std::move(imported));
    expect(doc.entityCount() == 3 && changes == 1, "import is one document transaction");
    doc.undo();
    expect(doc.entityCount() == 1 && doc.entityAt(0)->id() == existingId, "one undo removes the entire import");
    doc.redo();
    expect(doc.entityCount() == 3 && doc.indexOf(importedId) == 1, "import redo restores stable path identities");
    auto before = doc.entityAt(1)->clone(); auto after = before->clone(); after->translate(QPointF(10, 0));
    doc.commandStack()->execute(std::make_unique<UpdateEntityCommand>(1, std::move(before), std::move(after)));
    doc.insertEntity(0, line(100));
    doc.undo();
    auto* restored = static_cast<LineEntity*>(doc.entityAt(doc.indexOf(importedId)));
    expect(restored->startPoint() == QPointF(20, 0), "undo of edit follows UUID after index shifts");
    doc.redo();
    expect(static_cast<LineEntity*>(doc.entityAt(doc.indexOf(importedId)))->startPoint() == QPointF(30, 0),
           "redo of edit follows UUID after index shifts");
    doc.addLayer("Rack color"); const auto layerId = doc.layerAt(1).id;
    doc.setLayerName(1, "Blue"); doc.undo();
    expect(doc.layerAt(1).name == "Rack color", "layer rename is undoable");
    doc.setLayerVisible(1, false); doc.undo(); expect(doc.layerAt(1).visible, "layer visibility is undoable");
    doc.setLayerLocked(1, true); doc.undo(); expect(!doc.layerAt(1).locked, "layer lock is undoable");
    doc.setLayerColor(1, Qt::red); doc.undo(); expect(doc.layerAt(1).color != Qt::red, "layer color is undoable");
    std::vector<std::unique_ptr<Entity>> children;
    auto child = line(80); child->setLayerId(layerId); children.push_back(std::move(child));
    auto group = std::make_unique<GroupEntity>(std::move(children)); group->setLayerId(layerId);
    const auto groupId = group->id(); doc.addEntity(std::move(group)); doc.setActiveLayerId(layerId);
    doc.removeLayerAt(1);
    auto* moved = static_cast<GroupEntity*>(doc.entityAt(doc.indexOf(groupId)));
    expect(moved->layerId() == doc.layerAt(0).id && moved->children()[0]->layerId() == doc.layerAt(0).id,
           "removing layer reassigns nested children");
    doc.undo();
    expect(doc.layerCount() == 2 && doc.activeLayerId() == layerId && moved->layerId() == layerId
               && moved->children()[0]->layerId() == layerId, "layer removal undo restores nested assignments and active layer");
}
}
int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    buildJobs(); programPreservation(); undoTransactions();
    std::cout << checks << " plot/document checks, " << failures << " failures.\n";
    return failures ? 1 : 0;
}
