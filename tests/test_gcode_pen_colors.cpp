#include "AutomaticRackFixture.h"
#include "SettingsManager.h"
#include "ui/GcodeListWidget.h"
#include "ui/ToolpathPanel.h"
#include "ui/ToolpathCanvas.h"
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QIcon>
#include <QStandardPaths>
#include <QStyleFactory>
#include <cmath>
#include <iostream>

namespace {
int checks = 0, failures = 0;
void expect(bool condition, const char* message) {
    ++checks;
    if (!condition) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
}
int drawingRow(const PlotJob& job, const QUuid& tool) {
    for (int row = 0; row < job.blocks.size(); ++row)
        if (job.blocks[row].kind == PlotJobBlock::Kind::Drawing && job.blocks[row].toolId == tool)
            return row;
    return -1;
}
double luminance(const QColor& color) {
    const auto linear = [](double channel) {
        return channel <= 0.04045 ? channel / 12.92 : std::pow((channel + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * linear(color.redF()) + 0.7152 * linear(color.greenF()) + 0.0722 * linear(color.blueF());
}
void readableLineLabels() {
    GcodeListModel model;
    model.setLines({"G0 X1", "G1 X1", "G2 X1 I1", "G3 X1 I1", "G4 P0.01", "; Comment"});
    for (int row = 0; row < model.rowCount(); ++row) {
        const auto text = model.data(model.index(row), Qt::ForegroundRole).value<QColor>();
        expect((1.05 / (luminance(text) + 0.05)) >= 4.5, "pen labels and commands are readable on the white list background");
    }
    GcodeListWidget widget;
    widget.ensurePolished();
    auto* view = widget.findChild<QListView*>();
    const auto palette = view->palette();
    const double text = luminance(palette.color(QPalette::Inactive, QPalette::HighlightedText));
    const double background = luminance(palette.color(QPalette::Inactive, QPalette::Highlight));
    expect((std::max(text, background) + 0.05) / (std::min(text, background) + 0.05) >= 4.5,
           "selected pen labels remain readable when the list loses focus");
    widget.setLines({"G21 G90", "G1 X1"});
    widget.resize(450, 200); widget.show(); QApplication::processEvents();
    widget.selectLine(1); QApplication::processEvents();
    const auto selected = view->visualRect(view->model()->index(1, 0));
    const auto renderedBackground = view->viewport()->grab().toImage().pixelColor(view->viewport()->width() - 5, selected.center().y());
    expect(1.05 / (luminance(renderedBackground) + 0.05) >= 4.5, "rendered selected row has a readable background for white labels");
    widget.setExecutingLine(1); QApplication::processEvents();
    view->clearFocus(); widget.setFocus(); QApplication::processEvents();
    const auto executingBackground = view->viewport()->grab().toImage().pixelColor(view->viewport()->width() - 5, selected.center().y());
    expect(1.05 / (luminance(executingBackground) + 0.05) >= 4.5, "selected executing row keeps pen labels readable");
}
void assignedPenColors(const QString& screenshots) {
    AutomaticRackFixture fixture;
    auto* library = SettingsManager::instance().toolLibrary();
    library->loadFromJson(fixture.library.toJson());
    const auto job = fixture.job();
    ToolpathPanel panel;
    panel.setPlotJob(job);
    auto* model = panel.findChild<GcodeListModel*>();
    expect(model && model->lines() == job.lines(), "display annotations preserve executable lines");
    expect(panel.findChild<QLabel*>("gcodeColorLegend"), "list explains pen swatches and motion text colors");
    for (int row = 0; row < job.blocks.size(); ++row) {
        const auto& block = job.blocks[row];
        const auto index = model->index(row);
        if (block.toolId.isNull()) {
            expect(!model->data(index, Qt::DecorationRole).isValid(), "unassigned setup and rack lines have no invented pen color");
            continue;
        }
        const auto* pen = library->tool(block.toolId);
        expect(model->data(index).toString().contains(pen->name), "each drawing line names its assigned pen");
        expect(model->data(index, Qt::DecorationRole).isValid(), "each drawing line has a color swatch");
        expect(model->data(index, Qt::ToolTipRole).toString().contains(pen->previewColor.name()), "hover shows exact stroke color");
        expect(model->data(index, GcodeListModel::PreviewColorRole).value<QColor>() == pen->previewColor, "row color matches its pen ID");
        if (block.kind == PlotJobBlock::Kind::Rack)
            expect(model->data(index).toString().contains("Rack:"), "rack colors identify the handled pen and label rack motion");
        if (block.kind == PlotJobBlock::Kind::Synchronization)
            expect(model->data(index).toString().contains("Wait:"), "synchronization rows distinguish waiting from drawing");
    }
    const int blackRow = drawingRow(job, fixture.black), redRow = drawingRow(job, fixture.red);
    const auto blackIndex = model->index(blackRow), redIndex = model->index(redRow);
    const auto swatch = model->data(blackIndex, Qt::DecorationRole).value<QIcon>();
    expect(swatch.pixmap(QSize(16, 16)).toImage().pixelColor(8, 8) == QColor(Qt::black), "black pen is rendered as black");
    expect(swatch.pixmap(QSize(16, 16), QIcon::Selected).toImage().pixelColor(8, 8) == QColor(Qt::black), "selection preserves true swatch color");
    expect(swatch.pixmap(QSize(16, 16)).toImage().pixelColor(1, 1) != QColor(Qt::black), "black swatch has a visible border");
    panel.setExecutingLine(redRow);
    expect(model->data(redIndex, Qt::ForegroundRole).value<QColor>() == QColor("#ffcc00"), "execution highlighting remains visible");
    expect(model->data(redIndex, GcodeListModel::PreviewColorRole).value<QColor>() == QColor(Qt::red), "execution highlighting preserves pen color");
    const auto seal = panel.currentPlotJob().executionSeal;
    auto red = *library->tool(fixture.red);
    red.name = "Ruby"; red.previewColor = QColor("#b91c1c");
    expect(library->updateTool(red), "pen display properties can be edited");
    expect(model->data(redIndex).toString().contains("Ruby") && model->data(redIndex, GcodeListModel::PreviewColorRole).value<QColor>() == red.previewColor,
           "list refreshes pen name and color without reloading the job");
    expect(model->executingLine() == redRow && panel.currentGcodeLines() == job.lines() && panel.currentPlotJob().executionSeal == seal,
           "pen display refresh preserves execution position and the sealed program");
    auto* widget = panel.findChild<GcodeListWidget*>();
    widget->selectLine(redRow);
    expect(widget->selectedLine() == redRow, "annotated rows retain source line selection");
    panel.resize(1200, 760); panel.show(); QApplication::processEvents();
    widget->selectLine(redRow); QApplication::processEvents();
    auto* view = widget->findChild<QListView*>();
    const auto selectedRect = view->visualRect(view->currentIndex());
    const auto selectedBackground = view->viewport()->grab().toImage().pixelColor(view->viewport()->width() - 5, selectedRect.center().y());
    expect(1.05 / (luminance(selectedBackground) + 0.05) >= 4.5, "selected pen labels remain readable in the Toolpath panel");
    if (!screenshots.isEmpty()) {
        expect(widget->grab().save(screenshots + "/gcode-pen-colors.png"), "rendered color list screenshot saves");
    }
    expect(library->removeTool(fixture.red), "referenced pen can be removed from definitions");
    expect(model->data(redIndex).toString().contains("Missing tool") && !model->data(redIndex, Qt::DecorationRole).isValid(),
           "missing pen shows a clear label and loses stale color");
    panel.setGcodeLines({"G21 G90", "G1 X1 Y2"});
    const auto rawIndex = model->index(1);
    expect(model->data(rawIndex).toString().contains("Unknown tool") && !model->data(rawIndex, Qt::DecorationRole).isValid(),
           "imported G-code clears previous pen annotations");
    expect(model->data(rawIndex, Qt::ToolTipRole).toString().contains("no tool assignment metadata"), "raw code explains unknown stroke color");
    panel.clearAll();
    expect(model->rowCount() == 0 && widget->lines().isEmpty(), "clearing toolpaths clears pen annotations and lines");
}
void transformedAndDefaultPens() {
    AutomaticRackFixture fixture;
    auto* library = SettingsManager::instance().toolLibrary();
    library->loadFromJson(fixture.library.toJson());
    for (const auto& path : fixture.document.entities()) path->setToolId({});
    fixture.document.setDefaultToolId(fixture.black);
    fixture.options.automaticRack = false;
    const auto built = PlotJobBuilder::build(fixture.document, fixture.options, library);
    expect(built.valid(), "single default pen job is transformable");
    ToolpathPanel panel;
    panel.setPlotJob(built.job);
    auto* model = panel.findChild<GcodeListModel*>();
    auto* canvas = panel.findChild<ToolpathCanvas*>();
    const int row = drawingRow(built.job, fixture.black);
    expect(row >= 0 && model->data(model->index(row), GcodeListModel::PreviewColorRole).value<QColor>() == QColor(Qt::black),
           "document default pen determines unassigned path stroke color");
    GcodeParser parser;
    auto moves = parser.parse(built.job.lines());
    for (auto& move : moves) { move.start += QVector3D(2, 3, 0); move.end += QVector3D(2, 3, 0); }
    canvas->movesTransformed(moves);
    expect(panel.currentGcodeLines() != built.job.lines(), "interactive transform updates drawing coordinates");
    expect(model->data(model->index(row), GcodeListModel::ToolIdRole).value<QUuid>() == fixture.black
           && model->data(model->index(row)).toString().contains("Black"), "transform preserves row pen identity and color label");
    model->setRangeStart(row); model->setRangeEnd(row + 1);
    expect(model->data(model->index(row)).toString().contains(QChar(0x25B6))
           && model->data(model->index(row), GcodeListModel::PreviewColorRole).value<QColor>() == QColor(Qt::black),
           "stream range markers preserve pen metadata");
    auto white = *library->tool(fixture.black); white.previewColor = Qt::white;
    library->updateTool(white);
    const auto whiteSwatch = model->data(model->index(row), Qt::DecorationRole).value<QIcon>().pixmap(QSize(16, 16)).toImage();
    expect(whiteSwatch.pixelColor(8, 8) == QColor(Qt::white) && whiteSwatch.pixelColor(1, 1) != QColor(Qt::white),
           "white pens have a visible border");
    fixture.document.setDefaultToolId({});
    panel.setPlotJob(PlotJobBuilder::build(fixture.document, fixture.options, library).job);
    int currentRow = drawingRow(panel.currentPlotJob(), {});
    expect(currentRow >= 0 && model->data(model->index(currentRow)).toString().contains("Current tool")
           && !model->data(model->index(currentRow), Qt::DecorationRole).isValid(), "manual current pen has an explicit unknown color label");
}
}
int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("GrblSenderTests");
    QCoreApplication::setApplicationName("GcodePenColors");
    QStandardPaths::setTestModeEnabled(true);
    if (app.platformName() == "offscreen") {
        for (const auto* font : {"segoeui.ttf", "segoeuib.ttf", "seguisym.ttf", "consola.ttf"})
            QFontDatabase::addApplicationFont(QString("C:/Windows/Fonts/") + font);
    }
    app.setFont(QFont("Segoe UI", 10)); app.setStyle(QStyleFactory::create("Fusion"));
    QFile style(":/resources/style.qss");
    if (style.open(QIODevice::ReadOnly)) app.setStyleSheet(QString::fromUtf8(style.readAll()));
    QString screenshots;
    const auto arguments = app.arguments(); const auto flag = arguments.indexOf("--screenshots");
    if (flag >= 0 && flag + 1 < arguments.size()) { screenshots = arguments[flag + 1]; QDir().mkpath(screenshots); }
    assignedPenColors(screenshots);
    transformedAndDefaultPens();
    readableLineLabels();
    std::cout << checks << " G-code pen color checks, " << failures << " failures.\n";
    return failures ? 1 : 0;
}
