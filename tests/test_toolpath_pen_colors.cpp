#include "AutomaticRackFixture.h"
#include "SettingsManager.h"
#include "grbl/GraphicsDesign/CircleEntity.h"
#include "ui/ToolpathCanvas.h"
#include "ui/ToolpathPanel.h"
#include "ui/GcodeListWidget.h"
#include <QApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFontDatabase>
#include <QLabel>
#include <QMouseEvent>
#include <QStandardPaths>
#include <QStyleFactory>
#include <QTimer>
#include <cmath>
#include <iostream>

namespace {
int checks = 0, failures = 0;
void expect(bool condition, const char* message) {
    ++checks;
    if (!condition) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
}
QPoint screenPoint(const ToolpathCanvas& canvas, const QPointF& point) {
    // Fixtures fit inside the configured 100 mm square worktable.
    const double scale = std::min((canvas.width() - 100) / 100.0, (canvas.height() - 100) / 100.0);
    return QPoint(qRound(canvas.width() / 2.0 + (point.x() - 50) * scale),
                  qRound(canvas.height() / 2.0 - (point.y() - 50) * scale));
}
bool hasColorNear(ToolpathCanvas& canvas, const QPointF& point, const QColor& color, int tolerance = 25) {
    const auto image = canvas.grab().toImage();
    const auto center = screenPoint(canvas, point);
    for (int y = center.y() - 3; y <= center.y() + 3; ++y)
        for (int x = center.x() - 3; x <= center.x() + 3; ++x) {
            if (!image.rect().contains(x, y)) continue;
            const auto pixel = image.pixelColor(x, y);
            if (std::abs(pixel.red() - color.red()) <= tolerance
                && std::abs(pixel.green() - color.green()) <= tolerance
                && std::abs(pixel.blue() - color.blue()) <= tolerance) return true;
        }
    return false;
}
int darkestNear(ToolpathCanvas& canvas, const QPointF& point) {
    const auto image = canvas.grab().toImage(); const auto center = screenPoint(canvas, point);
    int darkest = 255;
    for (int y = center.y() - 3; y <= center.y() + 3; ++y)
        for (int x = center.x() - 3; x <= center.x() + 3; ++x)
            if (image.rect().contains(x, y)) darkest = std::min(darkest, qGray(image.pixel(x, y)));
    return darkest;
}
void loadPreview(ToolpathPanel& panel, const PlotJob& job) {
    QEventLoop loop; bool ready = false;
    QObject::connect(&panel, &ToolpathPanel::previewReady, &loop, [&](const QUuid& id) {
        if (id == job.id) { ready = true; loop.quit(); }
    });
    panel.setPlotJob(job);
    QTimer::singleShot(2000, &loop, &QEventLoop::quit);
    if (!ready) loop.exec();
    expect(ready, "asynchronous preview finishes for the current job");
    panel.resize(1100, 680); panel.show(); QApplication::processEvents();
    panel.setWorktableSize(QSizeF(100, 100));
    panel.findChild<ToolpathCanvas*>()->setTransformEnabled(false);
}
void penStrokes(const QString& screenshots) {
    AutomaticRackFixture fixture;
    const QList<QLineF> lines{{15, 18, 38, 18}, {15, 28, 38, 28},
                              {58, 18, 83, 18}, {58, 28, 83, 28}, {15, 48, 38, 48}};
    int index = 0;
    for (const auto& entity : fixture.document.entities()) {
        const auto line = lines[index++];
        static_cast<LineEntity*>(entity.get())->setStartEnd(line.p1(), line.p2());
    }
    auto circle = std::make_unique<CircleEntity>(QPointF(70, 65), 12);
    circle->setToolId(fixture.red); fixture.document.addEntity(std::move(circle));
    auto overlap = std::make_unique<LineEntity>(QPointF(66, 77), QPointF(74, 77));
    overlap->setToolId(fixture.black); fixture.document.addEntity(std::move(overlap));
    auto* library = SettingsManager::instance().toolLibrary();
    library->loadFromJson(fixture.library.toJson());
    const auto job = fixture.job();
    expect(!job.blocks.isEmpty() && job.automaticRack, "fixture generates a typed multi-pen rack job");
    ToolpathPanel panel; loadPreview(panel, job);
    auto* canvas = panel.findChild<ToolpathCanvas*>();
    expect(hasColorNear(*canvas, {27, 18}, Qt::black), "black assigned straight stroke renders black");
    expect(hasColorNear(*canvas, {71, 28}, Qt::red), "red assigned straight stroke renders red");
    expect(hasColorNear(*canvas, {82, 65}, Qt::red), "arc renders with its assigned pen color");
    expect(hasColorNear(*canvas, {70, 77}, Qt::black), "later black stroke remains above an earlier red arc");
    bool sourceIndices = !panel.previewMoves().isEmpty();
    for (const auto& move : panel.previewMoves())
        sourceIndices = sourceIndices && job.blocks[move.lineNumber].kind == PlotJobBlock::Kind::Drawing;
    expect(sourceIndices, "rack commands stay outside the preview and source indices remain intact");
    expect(panel.findChild<QLabel*>("toolpathColorLegend")->text().contains("assigned tool colors"), "preview legend explains assigned stroke colors");
    expect(hasColorNear(*canvas, {7.5, 9}, QColor("#ff8800")), "rapid travel stays dashed orange despite its pen assignment");
    if (!screenshots.isEmpty())
        expect(panel.grab().save(screenshots + "/toolpath-pen-colors.png"), "multi-pen preview screenshot saves");

    int redLine = -1;
    for (const auto& move : panel.previewMoves())
        if (move.type == GcodeMove::Type::Linear && move.end == QVector3D(83, 28, 0)) redLine = move.lineNumber;
    expect(redLine >= 0, "red drawing stroke has an original command index");
    int selectedLine = -1;
    QObject::connect(&panel, &ToolpathPanel::lineSelected, &panel, [&](int row) { selectedLine = row; });
    QMouseEvent click(QEvent::MouseButtonPress, QPointF(screenPoint(*canvas, {71, 28})),
                      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(canvas, &click);
    expect(selectedLine == redLine && panel.findChild<GcodeListWidget*>()->selectedLine() == redLine,
           "clicking a colored stroke selects its original G-code row");
    expect(hasColorNear(*canvas, {71, 28}, QColor("#ffdd44")), "selected stroke keeps the yellow highlight");
    canvas->highlightSegment(-1);
    expect(hasColorNear(*canvas, {71, 28}, Qt::red), "clearing selection restores the assigned pen color");
    panel.setExecutingLine(redLine);
    expect(hasColorNear(*canvas, {71, 28}, QColor("#ffdd44")), "executing stroke remains highlighted yellow");
    expect(hasColorNear(*canvas, {27, 18}, QColor("#666666")), "completed strokes retain the existing gray execution state");
    expect(hasColorNear(*canvas, {82, 65}, Qt::red), "future strokes keep their pen color during execution");
    panel.setExecutingLine(-1);
    expect(hasColorNear(*canvas, {27, 18}, Qt::black), "ending execution restores the full pen-colored preview");
    panel.setStreamRange(redLine, redLine);
    if (!screenshots.isEmpty()) panel.grab().save(screenshots + "/toolpath-range.png");
    expect(hasColorNear(*canvas, {71, 28}, Qt::red), "in-range stroke keeps its pen color");
    const int dimmed = darkestNear(*canvas, {27, 18});
    expect(dimmed >= 60 && dimmed < 160, "out-of-range black stroke is gray and remains visible");
    panel.setStreamRange(-1, -1);

    const auto seal = panel.currentPlotJob().executionSeal;
    auto red = *library->tool(fixture.red);
    red.name = "Blue ink"; red.previewColor = QColor("#2563eb");
    expect(library->updateTool(red), "saved pen display properties can change");
    expect(hasColorNear(*canvas, {71, 28}, red.previewColor) && hasColorNear(*canvas, {82, 65}, red.previewColor),
           "live pen color edit updates both straight and arc caches");
    expect(hasColorNear(*canvas, {27, 18}, Qt::black), "editing one pen preserves other pen colors");
    expect(hasColorNear(*canvas, {7.5, 9}, QColor("#ff8800")), "editing pen colors preserves rapid travel styling");
    const auto* list = panel.findChild<GcodeListModel*>();
    expect(list->data(list->index(redLine), GcodeListModel::PreviewColorRole).value<QColor>() == red.previewColor,
           "preview colors stay synchronized with the G-code list");
    expect(panel.currentGcodeLines() == job.lines() && panel.currentPlotJob().executionSeal == seal,
           "display changes preserve the executable program and seal");
    if (!screenshots.isEmpty())
        expect(panel.grab().save(screenshots + "/toolpath-live-color.png"), "live color preview screenshot saves");
    red.name = "White ink"; red.previewColor = Qt::white; library->updateTool(red);
    if (!screenshots.isEmpty()) panel.grab().save(screenshots + "/toolpath-white.png");
    expect(hasColorNear(*canvas, {71, 28}, Qt::white, 2), "white pens render an actual white stroke");
    const int outline = darkestNear(*canvas, {71, 28});
    expect(outline >= 100 && outline < 200, "white strokes have a visible outline");
    library->removeTool(fixture.red);
    expect(hasColorNear(*canvas, {71, 28}, QColor("#2266aa")) && hasColorNear(*canvas, {82, 65}, QColor("#22aa66")),
           "missing pen definitions clear stale colors and use motion-color fallbacks");
    library->loadFromJson(fixture.library.toJson());
    expect(hasColorNear(*canvas, {71, 28}, Qt::red), "restoring a pen definition restores its stroke color");

    const auto raw = PlotJob::fromLines({"G21 G90", "G0 X15 Y18 Z5", "G1 Z0", "G1 X38 Y18",
                                        "G0 Z5", "G0 X82 Y65 Z5", "G1 Z0", "G2 X82 Y65 I-12 J0"});
    panel.setPlotJob(job); // Leave an older parser result in flight before switching jobs.
    loadPreview(panel, raw);
    expect(hasColorNear(*canvas, {27, 18}, QColor("#2266aa")), "raw import and stale parse completion cannot reuse previous pen colors");
    expect(hasColorNear(*canvas, {82, 65}, QColor("#22aa66")), "raw arcs retain their existing motion color");
    expect(panel.findChild<QLabel*>("toolpathColorLegend")->text().contains("Blue straight strokes / green arcs"),
           "unknown pen preview explains the fallback colors");
    panel.clearAll();
    expect(panel.previewMoves().isEmpty() && !hasColorNear(*canvas, {27, 18}, QColor("#2266aa")), "clearing a job clears preview strokes and colors");
}
void defaultPenAndTransforms() {
    AutomaticRackFixture fixture;
    auto* library = SettingsManager::instance().toolLibrary();
    library->loadFromJson(fixture.library.toJson());
    for (const auto& path : fixture.document.entities()) path->setToolId({});
    fixture.document.setDefaultToolId(fixture.black); fixture.options.automaticRack = false;
    const auto built = PlotJobBuilder::build(fixture.document, fixture.options, library);
    expect(built.valid(), "default pen generates a manual drawing job");
    ToolpathPanel panel; loadPreview(panel, built.job);
    auto* canvas = panel.findChild<ToolpathCanvas*>();
    expect(hasColorNear(*canvas, {15, 15}, Qt::black), "document default pen colors unassigned paths");
    canvas->movesTransformed(GcodeParser::transformMoves(panel.previewMoves(), 1, 1, 5, 7, 0));
    expect(panel.currentGcodeLines() != built.job.lines() && hasColorNear(*canvas, {20, 22}, Qt::black),
           "drawing transform retains pen identity at the new geometry without refitting");
    auto pen = *library->tool(fixture.black); pen.previewColor = QColor("#7e22ce"); library->updateTool(pen);
    expect(hasColorNear(*canvas, {20, 22}, pen.previewColor), "live edits recolor transformed strokes without changing zoom or geometry");
    fixture.document.setDefaultToolId({});
    loadPreview(panel, PlotJobBuilder::build(fixture.document, fixture.options, library).job);
    expect(hasColorNear(*canvas, {15, 15}, QColor("#2266aa")), "manual current pen uses the motion-color fallback");
}
}
int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("GrblSenderTests");
    QCoreApplication::setApplicationName("ToolpathPenColors");
    QStandardPaths::setTestModeEnabled(true);
    if (app.platformName() == "offscreen") {
        for (const auto* font : {"segoeui.ttf", "segoeuib.ttf", "seguisym.ttf", "consola.ttf"})
            QFontDatabase::addApplicationFont(QString("C:/Windows/Fonts/") + font);
    }
    app.setFont(QFont("Segoe UI", 10)); app.setStyle(QStyleFactory::create("Fusion"));
    QFile style(":/resources/style.qss");
    if (style.open(QIODevice::ReadOnly)) app.setStyleSheet(QString::fromUtf8(style.readAll()));
    QString screenshots;
    const auto args = app.arguments(); const auto flag = args.indexOf("--screenshots");
    if (flag >= 0 && flag + 1 < args.size()) { screenshots = args[flag + 1]; QDir().mkpath(screenshots); }
    penStrokes(screenshots);
    defaultPenAndTransforms();
    std::cout << checks << " toolpath pen color checks, " << failures << " failures.\n";
    return failures ? 1 : 0;
}
