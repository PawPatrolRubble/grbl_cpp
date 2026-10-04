#include "AutomaticRackFixture.h"
#include "SettingsManager.h"
#include "ui/GcodeListWidget.h"
#include "ui/ToolpathCanvas.h"
#include "ui/ToolpathPanel.h"
#include <QAbstractProxyModel>
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFontDatabase>
#include <QKeyEvent>
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
    const double scale = std::min((canvas.width() - 100) / 100.0, (canvas.height() - 100) / 100.0);
    return QPoint(qRound(canvas.width() / 2.0 + (point.x() - 50) * scale),
                  qRound(canvas.height() / 2.0 - (point.y() - 50) * scale));
}
bool hasColorNear(ToolpathCanvas& canvas, const QPointF& point, const QColor& color) {
    const auto image = canvas.grab().toImage(); const auto center = screenPoint(canvas, point);
    for (int y = center.y() - 3; y <= center.y() + 3; ++y)
        for (int x = center.x() - 3; x <= center.x() + 3; ++x) {
            if (!image.rect().contains(x, y)) continue;
            const auto pixel = image.pixelColor(x, y);
            if (std::abs(pixel.red() - color.red()) <= 25 && std::abs(pixel.green() - color.green()) <= 25
                && std::abs(pixel.blue() - color.blue()) <= 25) return true;
        }
    return false;
}
void click(QWidget* widget, const QPoint& point) {
    QMouseEvent press(QEvent::MouseButtonPress, QPointF(point), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QMouseEvent release(QEvent::MouseButtonRelease, QPointF(point), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(widget, &press); QApplication::sendEvent(widget, &release);
}
void loadPreview(ToolpathPanel& panel, const PlotJob& job) {
    QEventLoop loop; bool ready = false;
    QObject::connect(&panel, &ToolpathPanel::previewReady, &loop, [&](const QUuid& id) {
        if (id == job.id) { ready = true; loop.quit(); }
    });
    panel.setPlotJob(job); QTimer::singleShot(2000, &loop, &QEventLoop::quit);
    if (!ready) loop.exec();
    expect(ready, "current preview completes");
    panel.resize(1100, 680); panel.show(); QApplication::processEvents();
    panel.setWorktableSize(QSizeF(100, 100));
}
void sharedFilter(const QString& screenshots) {
    AutomaticRackFixture fixture;
    const QList<QLineF> lines{{15, 18, 38, 18}, {15, 28, 38, 28},
                              {58, 18, 83, 18}, {58, 28, 83, 28}, {15, 48, 38, 48}};
    int i = 0;
    for (const auto& entity : fixture.document.entities()) {
        const auto line = lines[i++];
        static_cast<LineEntity*>(entity.get())->setStartEnd(line.p1(), line.p2());
    }
    SettingsManager::instance().toolLibrary()->loadFromJson(fixture.library.toJson());
    const auto job = fixture.job();
    ToolpathPanel panel; loadPreview(panel, job);
    auto* filter = panel.findChild<QComboBox*>("toolpathToolFilter");
    expect(filter, "Toolpath page exposes a shared pen filter");
    if (!filter) return;
    auto* list = panel.findChild<GcodeListWidget*>();
    auto* view = list->findChild<QListView*>();
    auto* canvas = panel.findChild<ToolpathCanvas*>();
    const int red = filter->findData(QVariant::fromValue(fixture.red));
    expect(red >= 0 && filter->currentIndex() == 0, "used pens are available and All pens is the default");
    filter->setCurrentIndex(red); QApplication::processEvents();
    QList<int> expectedRows;
    for (int row = 0; row < job.blocks.size(); ++row)
        if (job.blocks[row].toolId == fixture.red) expectedRows << row;
    expect(view->model()->rowCount() == expectedRows.size(), "list includes only commands associated with the chosen pen");
    auto* proxy = qobject_cast<QAbstractProxyModel*>(view->model());
    expect(proxy, "view filtering keeps a source model for original command indices");
    if (proxy) for (int row = 0; row < proxy->rowCount(); ++row) {
        const auto index = proxy->index(row, 0);
        expect(proxy->mapToSource(index).row() == expectedRows[row], "filtered row maps to its original G-code line");
    }
    expect(hasColorNear(*canvas, {71, 28}, Qt::red), "chosen pen remains visible in the preview");
    expect(!hasColorNear(*canvas, {27, 18}, Qt::black), "other pen strokes are hidden in the preview");
    expect(!hasColorNear(*canvas, {7.5, 9}, QColor("#ff8800")), "travel associated with hidden pens is hidden too");
    expect(panel.currentGcodeLines() == job.lines() && list->lines() == job.lines()
           && panel.currentPlotJob().executionSeal == job.executionSeal, "filtering preserves the complete executable job");
    if (!screenshots.isEmpty())
        expect(panel.grab().save(screenshots + "/pen-filter-red.png"), "filtered preview screenshot saves");

    auto* count = list->findChild<QLabel*>("gcodeCountLabel");
    expect(count->text() == QString("%1 / %2 lines visible").arg(expectedRows.size()).arg(job.blocks.size()),
           "filtered count reports visible and total original commands");
    int redLine = -1, blackLine = -1, laterBlack = -1;
    for (const auto& move : panel.previewMoves()) {
        if (move.type != GcodeMove::Type::Linear) continue;
        if (move.end == QVector3D(83, 28, 0)) redLine = move.lineNumber;
        if (move.end == QVector3D(38, 18, 0)) blackLine = move.lineNumber;
        if (move.end == QVector3D(38, 48, 0)) laterBlack = move.lineNumber;
    }
    expect(redLine >= 0 && blackLine >= 0 && laterBlack > redLine, "drawing fixtures have distinct original line indices");
    int selected = -1;
    QObject::connect(&panel, &ToolpathPanel::lineSelected, &panel, [&](int row) { selected = row; });
    click(canvas, screenPoint(*canvas, {71, 28}));
    expect(selected == redLine && panel.startFromLine() == redLine, "filtered canvas selection uses the original G-code line");
    selected = -1;
    click(canvas, screenPoint(*canvas, {27, 18}));
    expect(selected == -1, "hidden pen strokes cannot be selected by clicking the canvas");
    canvas->highlightSegment(-1);
    list->selectLine(redLine);
    click(view->viewport(), view->visualRect(view->currentIndex()).center());
    expect(selected == redLine && hasColorNear(*canvas, {71, 28}, QColor("#ffdd44")),
           "filtered list click highlights the original drawing segment");
    QKeyEvent down(QEvent::KeyPress, Qt::Key_Down, Qt::NoModifier);
    QApplication::sendEvent(view, &down);
    expect(list->selectedLine() == expectedRows[expectedRows.indexOf(redLine) + 1],
           "keyboard navigation returns original indices through the filtered list");
    list->selectLine(redLine);
    filter->setCurrentIndex(0);
    expect(list->selectedLine() == redLine, "changing to All preserves a still-visible original selection");
    filter->setCurrentIndex(filter->findData(QVariant::fromValue(fixture.black)));
    expect(list->selectedLine() == -1, "a filter clears selection when its original line becomes hidden");
    filter->setCurrentIndex(0);
    expect(!hasColorNear(*canvas, {71, 28}, QColor("#ffdd44")),
           "restoring All does not resurrect a cleared hidden selection highlight");
    filter->setCurrentIndex(filter->findData(QVariant::fromValue(fixture.red)));
    canvas->highlightSegment(-1);

    panel.setExecutingLine(laterBlack);
    expect(count->text().contains(QString("Line %1 / %2").arg(laterBlack + 1).arg(job.blocks.size()))
           && count->text().contains("Hidden by tool filter"), "hidden execution keeps original progress and explains why its row is absent");
    expect(!hasColorNear(*canvas, {27, 18}, QColor("#666666"))
           && !hasColorNear(*canvas, {27, 48}, QColor("#ffdd44")),
           "execution highlights and completed-stroke caches cannot reveal hidden pens");
    expect(hasColorNear(*canvas, {71, 28}, QColor("#666666")), "visible completed strokes retain execution status");
    filter->setCurrentIndex(0);
    expect(hasColorNear(*canvas, {27, 18}, QColor("#666666")), "changing a filter rebuilds the completed-stroke cache");
    filter->setCurrentIndex(filter->findData(QVariant::fromValue(fixture.red)));
    panel.setExecutingLine(redLine);
    expect(!count->text().contains("Hidden by tool filter") && hasColorNear(*canvas, {71, 28}, QColor("#ffdd44")),
           "visible execution highlights the original row and stroke");
    panel.setExecutingLine(-1);
    panel.setStreamRange(redLine, redLine);
    expect(!hasColorNear(*canvas, {27, 18}, QColor("#444444")), "range dimming also excludes hidden pens");
    panel.setStreamRange(-1, -1);

    auto* library = SettingsManager::instance().toolLibrary();
    auto pen = *library->tool(fixture.red); pen.name = "Blue ink"; pen.previewColor = QColor("#2563eb");
    library->updateTool(pen);
    expect(filter->currentData().toUuid() == fixture.red && filter->currentText().contains("Blue ink")
           && view->model()->rowCount() == expectedRows.size() && hasColorNear(*canvas, {71, 28}, pen.previewColor),
           "pen edits refresh the selector and preview while preserving the selected pen ID");
    library->removeTool(fixture.red);
    expect(filter->currentData().toUuid() == fixture.red && filter->currentText().contains("Missing tool")
           && view->model()->rowCount() == expectedRows.size(), "deleted referenced pens remain filterable by their original IDs");
    library->loadFromJson(fixture.library.toJson());
    const auto unused = library->addTool("Unused", Qt::green);
    expect(filter->findData(QVariant::fromValue(unused)) < 0, "unused library pens do not appear in a job's filter");
    auto black = *library->tool(fixture.black); black.name = "Same name"; black.previewColor = Qt::red; library->updateTool(black);
    pen = *library->tool(fixture.red); pen.name = "Same name"; library->updateTool(pen);
    const int blackOption = filter->findData(QVariant::fromValue(fixture.black));
    const int redOption = filter->findData(QVariant::fromValue(fixture.red));
    expect(blackOption != redOption && filter->itemText(blackOption) != filter->itemText(redOption),
           "pens with identical names and colors have distinguishable filter choices");
    expect(filter->currentData().toUuid() == fixture.red && view->model()->rowCount() == expectedRows.size(),
           "identical display names never merge pen identities");

    filter->setCurrentIndex(filter->findData(QVariant::fromValue(QUuid{})));
    int unassigned = 0;
    for (const auto& block : job.blocks) if (block.toolId.isNull()) ++unassigned;
    expect(view->model()->rowCount() == unassigned && !hasColorNear(*canvas, {71, 28}, Qt::red),
           "Unassigned / setup filters null pen IDs separately from All");
    if (!screenshots.isEmpty()) panel.grab().save(screenshots + "/pen-filter-setup.png");
    expect(panel.currentGcodeLines() == job.lines() && panel.currentPlotJob().executionSeal == job.executionSeal,
           "selection, progress and library edits preserve all executable commands");

    library->loadFromJson(fixture.library.toJson());
    QEventLoop loop; bool ready = false;
    QObject::connect(&panel, &ToolpathPanel::previewReady, &loop, [&](const QUuid&) { ready = true; loop.quit(); });
    panel.setPlotJob(job);
    expect(filter->currentIndex() == 0, "loading a job resets the view to All pens");
    filter->setCurrentIndex(filter->findData(QVariant::fromValue(fixture.red)));
    QTimer::singleShot(2000, &loop, &QEventLoop::quit); if (!ready) loop.exec();
    expect(ready && filter->currentData().toUuid() == fixture.red && !hasColorNear(*canvas, {27, 18}, Qt::black),
           "an asynchronous preview completion preserves a filter chosen while parsing");
    const auto raw = PlotJob::fromLines({"G21 G90", "G0 X15 Y18 Z5", "G1 Z0", "G1 X38 Y18"});
    loadPreview(panel, raw);
    expect(filter->currentIndex() == 0 && filter->count() == 2 && view->model()->rowCount() == raw.blocks.size(),
           "raw imports reset stale pen choices and show the complete file");
    filter->setCurrentIndex(filter->findData(QVariant::fromValue(QUuid{})));
    expect(view->model()->rowCount() == raw.blocks.size() && hasColorNear(*canvas, {27, 18}, QColor("#2266aa")),
           "raw G-code remains visible under Unassigned / setup without inventing a pen identity");
    panel.clearAll();
    expect(filter->currentIndex() == 0 && filter->count() == 1 && !filter->isEnabled()
           && view->model()->rowCount() == 0 && panel.previewMoves().isEmpty(), "clear removes stale filters and all preview geometry");
}
void manualRangesAndTransforms() {
    AutomaticRackFixture fixture; fixture.options.automaticRack = false;
    for (const auto& path : fixture.document.entities()) path->setToolId(fixture.red);
    auto* library = SettingsManager::instance().toolLibrary(); library->loadFromJson(fixture.library.toJson());
    const auto built = PlotJobBuilder::build(fixture.document, fixture.options, library);
    expect(built.valid(), "manual range fixture is a valid single-pen job");
    const auto job = built.job;
    ToolpathPanel panel; loadPreview(panel, job);
    auto* filter = panel.findChild<QComboBox*>("toolpathToolFilter");
    auto* list = panel.findChild<GcodeListWidget*>(); auto* view = list->findChild<QListView*>();
    auto* canvas = panel.findChild<ToolpathCanvas*>();
    filter->setCurrentIndex(filter->findData(QVariant::fromValue(fixture.red)));
    auto* proxy = qobject_cast<QAbstractProxyModel*>(view->model());
    const int first = proxy->mapToSource(proxy->index(0, 0)).row();
    const int last = proxy->mapToSource(proxy->index(proxy->rowCount() - 1, 0)).row();
    QPushButton *start = nullptr, *end = nullptr;
    for (auto* button : list->findChildren<QPushButton*>()) {
        if (button->text() == "Set Start") start = button;
        if (button->text() == "Set End") end = button;
    }
    expect(start && end && start->isEnabled() && end->isEnabled(), "manual jobs retain streaming range controls");
    list->selectLine(first); start->click(); list->selectLine(last); end->click();
    expect(panel.rangeStart() == first && panel.rangeEnd() == last && panel.startFromLine() == last,
           "filtered range and start-from controls refer to original program lines");
    expect(canvas->toolTip().contains("Choose All tools"), "filtered drawing explains that transforms require the complete view");
    const auto before = panel.currentGcodeLines();
    // The midpoint lies inside the full drawing box and away from any stroke or handle.
    const QPoint from = screenPoint(*canvas, {15, 12});
    QMouseEvent press(QEvent::MouseButtonPress, QPointF(from), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QMouseEvent move(QEvent::MouseMove, QPointF(from + QPoint(15, 10)), Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
    QMouseEvent release(QEvent::MouseButtonRelease, QPointF(from + QPoint(15, 10)), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(canvas, &press); QApplication::sendEvent(canvas, &move); QApplication::sendEvent(canvas, &release);
    expect(panel.currentGcodeLines() == before, "filtered view cannot drag and transform hidden paths");
    QMouseEvent hover(QEvent::MouseMove, QPointF(from), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(canvas, &hover);
    expect(canvas->cursor().shape() == Qt::ArrowCursor, "filtered view does not advertise disabled drawing transform handles");
    filter->setCurrentIndex(0);
    expect(canvas->toolTip().isEmpty() && view->model()->rowCount() == job.blocks.size(), "All restores the complete view and drawing transforms");
    canvas->movesTransformed(GcodeParser::transformMoves(panel.previewMoves(), 1, 1, 5, 7, 0));
    filter->setCurrentIndex(filter->findData(QVariant::fromValue(fixture.red)));
    expect(view->model()->rowCount() == last - first + 1 && hasColorNear(*canvas, {20, 22}, Qt::red),
           "filtering after a drawing transform retains pen identity and original command indices");
}
}
int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("GrblSenderTests");
    QCoreApplication::setApplicationName("PenFilter"); QStandardPaths::setTestModeEnabled(true);
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
    sharedFilter(screenshots);
    manualRangesAndTransforms();
    std::cout << checks << " tool filter checks, " << failures << " failures.\n";
    return failures ? 1 : 0;
}
