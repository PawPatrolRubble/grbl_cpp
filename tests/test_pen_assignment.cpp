#include "grbl/GraphicsDesign/Document.h"
#include "grbl/GraphicsDesign/GroupEntity.h"
#include "grbl/GraphicsDesign/LineEntity.h"
#include "ui/DesignCanvas.h"
#include "ui/panels/GraphicsDesignPanel.h"
#include "grbl/ToolLibrary.h"
#include "SettingsManager.h"
#include "ui/canvas/EntityClipboard.h"
#include "ui/panels/EntityTransform.h"
#include "ui/panels/TransformPanel.h"
#include "ui/ToolsDialog.h"
#include "grbl/GraphicsDesign/Tools/TrimExtendTool.h"
#include "grbl/GraphicsDesign/TrimModification.h"
#include "grbl/GraphicsDesign/RectangleEntity.h"
#include "grbl/GraphicsDesign/CircleEntity.h"

#include <QApplication>
#include <QJsonArray>
#include <QStandardPaths>
#include <QComboBox>
#include <QListWidget>
#include <QPushButton>
#include <QProcess>
#include <QFile>
#include <QDir>
#include <QLineEdit>
#include <QCheckBox>
#include <QLabel>
#include <QEventLoop>
#include <QTimer>
#include <QFontDatabase>
#include <QScrollArea>
#include <QScrollBar>
#include <QDoubleSpinBox>
#include <QPlainTextEdit>
#include <QTabWidget>
#include <iostream>

namespace {

bool expect(bool condition, const char* message)
{
    std::cout << (condition ? "PASS: " : "FAIL: ") << message << '\n';
    return condition;
}

bool assignmentSurvivesProjectRoundTrip()
{
    const QUuid penId = QUuid::createUuid();
    LineEntity line(QPointF(0, 0), QPointF(10, 10));
    QJsonObject lineJson = line.toJson();
    lineJson["toolId"] = penId.toString();
    QJsonObject root;
    root["entities"] = QJsonArray{lineJson};
    Document document;
    document.loadFromJson(QJsonDocument(root));
    const QJsonObject savedLine = document.toJson().object()["entities"].toArray()[0].toObject();
    return expect(savedLine["toolId"].toString() == penId.toString(),
                  "A path's pen assignment survives project save/load");
}

bool nestedAssignmentSurvivesProjectRoundTrip()
{
    const QUuid penId = QUuid::createUuid();
    LineEntity line(QPointF(0, 0), QPointF(10, 10));
    QJsonObject lineJson = line.toJson();
    lineJson["toolId"] = penId.toString();
    QJsonObject groupJson{{"type", "group"}, {"children", QJsonArray{lineJson}}};
    Document document;
    document.loadFromJson(QJsonDocument(QJsonObject{{"entities", QJsonArray{groupJson}}}));
    const auto savedGroup = document.toJson().object()["entities"].toArray()[0].toObject();
    const auto savedLine = savedGroup["children"].toArray()[0].toObject();
    return expect(savedLine["toolId"].toString() == penId.toString(),
                  "A grouped path's pen assignment survives project save/load");
}

bool canvasUndoUsesDocumentCommands()
{
    DesignCanvas canvas;
    Document* document = canvas.document();
    document->addEntity(std::make_unique<LineEntity>(QPointF(0, 0), QPointF(10, 10)));
    document->commandStack()->execute(std::make_unique<UpdateEntityCommand>(
        0, document->entityAt(0)->clone(),
        std::make_unique<LineEntity>(QPointF(0, 0), QPointF(20, 20))));
    canvas.undo();
    const auto* line = dynamic_cast<const LineEntity*>(document->entityAt(0));
    return expect(line && line->endPoint() == QPointF(10, 10),
                  "Canvas Undo reverses document edits from the assignment/property panel");
}

bool bulkAssignmentUndoesTogether()
{
    Document document;
    for (int index = 0; index < 3; ++index) {
        document.addEntity(std::make_unique<LineEntity>(QPointF(index, 0), QPointF(index, 10)));
        document.select(document.entityAt(index)->id());
    }
    const QUuid penId = QUuid::createUuid();
    document.assignToolToSelection(penId);
    bool assigned = true;
    for (int index = 0; index < 3; ++index) assigned &= document.entityAt(index)->toolId() == penId;
    document.undo();
    bool restored = true;
    for (int index = 0; index < 3; ++index) restored &= document.entityAt(index)->toolId().isNull();
    document.redo();
    bool redone = true;
    for (int index = 0; index < 3; ++index) redone &= document.entityAt(index)->toolId() == penId;
    document.assignToolToSelection({});
    const bool cleared = document.entityAt(0)->toolId().isNull();
    document.undo();
    return expect(assigned && restored && redone && cleared && document.entityAt(0)->toolId() == penId,
                  "Bulk assignment, clearing to Default, Undo and Redo preserve all paths");
}

bool editsAndCopiesKeepThePen()
{
    Document document;
    auto line = std::make_unique<LineEntity>(QPointF(0, 0), QPointF(10, 10));
    const QUuid penId = QUuid::createUuid();
    line->setToolId(penId);
    document.addEntity(std::move(line));
    const QUuid entityId = document.entityAt(0)->id();
    document.select(entityId);
    document.commandStack()->execute(std::make_unique<UpdateEntityCommand>(
        0, document.entityAt(0)->clone(),
        EntityTransform::makeScaledEntity(document.entityAt(0), QPointF(0, 0), 2.0)));
    EntityClipboard clipboard(document, *document.commandStack());
    clipboard.duplicate();
    return expect(document.entityAt(0)->toolId() == penId && document.entityAt(1)->toolId() == penId
                      && document.entityAt(0)->id() == entityId && document.entityAt(1)->id() != entityId,
                  "Geometry edits and duplication keep the pen and give copies new identities");
}

bool defaultAndLegacyProjectsWork()
{
    Document document;
    document.addEntity(std::make_unique<LineEntity>(QPointF(0, 0), QPointF(10, 10)));
    const QUuid penId = QUuid::createUuid();
    document.commandStack()->execute(std::make_unique<SetDefaultToolCommand>(QUuid{}, penId));
    document.rememberToolName(penId, "Red");
    Document restored;
    restored.loadFromJson(document.toJson());
    const bool saved = restored.defaultToolId() == penId && restored.toolNameHint(penId) == "Red"
        && restored.plottingToolIds() == QSet<QUuid>{penId};
    document.undo();
    const bool undone = document.defaultToolId().isNull();
    restored.loadFromJson(QJsonDocument(QJsonObject{{"entities", QJsonArray{LineEntity(
        QPointF(0, 0), QPointF(1, 1)).toJson()}}}));
    return expect(saved && undone && restored.defaultToolId().isNull()
                      && restored.entityAt(0)->toolId().isNull(),
                  "Default pens persist and undo; old projects retain a manual default");
}

bool multiPenGenerationIsBlocked()
{
    Document document;
    ToolLibrary library;
    for (int index = 0; index < 2; ++index) {
        auto line = std::make_unique<LineEntity>(QPointF(index, 0), QPointF(index, 10));
        line->setToolId(library.addTool(QString("Pen %1").arg(index), Qt::black));
        document.addEntity(std::move(line));
    }
    const bool blocked = document.generateGcode(1000, 500, 5, 0, &library).isEmpty();
    document.entityAt(1)->setEnabled(false);
    return expect(blocked && !document.generateGcode(1000, 500, 5, 0, &library).isEmpty(),
                  "Multiple pens cannot silently generate single-pen G-code; disabled paths are excluded");
}

bool libraryPreservesStableReferences()
{
    ToolLibrary library;
    const QUuid id = library.addTool("Red", QColor("#cc2233"));
    ToolDefinition renamed = *library.tool(id);
    renamed.name = "Red fine";
    renamed.enabled = false;
    library.updateTool(renamed);
    ToolLibrary restored;
    restored.loadFromJson(library.toJson());
    const auto* profile = restored.tool(id);
    const bool roundTrip = profile && profile->name == "Red fine" && !profile->enabled
        && profile->previewColor == QColor("#cc2233");
    QJsonArray duplicated = library.toJson();
    duplicated.append(duplicated[0]);
    const bool rejected = !restored.loadFromJson(duplicated) && restored.tools().size() == 1;
    const bool invalidName = library.addTool("   ", Qt::red).isNull();
    return expect(roundTrip && rejected && invalidName && library.removeTool(id) && !library.tool(id),
                  "Pen definitions retain IDs through edits/persistence and reject invalid definitions");
}

bool assignmentControlsExist()
{
    GraphicsDesignPanel panel;
    return expect(panel.findChild<QListWidget*>("pathList")
                      && panel.findChild<QComboBox*>("assignToolCombo")
                      && panel.findChild<QComboBox*>("defaultToolCombo")
                      && panel.findChild<QPushButton*>("manageToolsButton"),
                  "The design panel exposes multi-path selection, pen assignment, default and pen editing");
}

bool pathSelectionAndAssignmentWorkTogether()
{
    auto* library = SettingsManager::instance().toolLibrary();
    const QUuid red = library->addTool("Red", Qt::red);
    GraphicsDesignPanel panel;
    Document* document = panel.canvas()->document();
    for (int index = 0; index < 3; ++index)
        document->addEntity(std::make_unique<LineEntity>(QPointF(index, 0), QPointF(index, 10)));
    auto* list = panel.findChild<QListWidget*>("pathList");
    auto* combo = panel.findChild<QComboBox*>("assignToolCombo");
    list->item(0)->setSelected(true);
    list->item(2)->setSelected(true);
    const bool selected = document->selectedIds().size() == 2;
    const int redIndex = combo->findData(QVariant::fromValue(red));
    QMetaObject::invokeMethod(combo, "activated", Q_ARG(int, redIndex));
    const bool assigned = document->entityAt(0)->toolId() == red
        && document->entityAt(2)->toolId() == red && document->entityAt(1)->toolId().isNull();
    const bool keptSelection = list->selectedItems().size() == 2 && document->selectedIds().size() == 2;
    panel.canvas()->undo();
    const bool undone = document->entityAt(0)->toolId().isNull() && document->entityAt(2)->toolId().isNull();
    panel.canvas()->redo();
    const bool redone = document->entityAt(0)->toolId() == red && document->entityAt(2)->toolId() == red;
    list->item(0)->setCheckState(Qt::Unchecked);
    const bool checkbox = document->selectedIds().size() == 2 && document->entityAt(0)->toolId() == red;
    document->clearSelection();
    const bool cleared = list->selectedItems().isEmpty() && !combo->isEnabled();
    return expect(selected && assigned && keptSelection && undone && redone && checkbox && cleared,
                  "Multi-selection, assignment, Undo/Redo and checkboxes stay synchronized in the widgets");
}

bool missingPenRemainsVisible()
{
    auto* library = SettingsManager::instance().toolLibrary();
    const QUuid pen = library->addTool("Removable blue", Qt::blue);
    GraphicsDesignPanel panel;
    auto* document = panel.canvas()->document();
    auto line = std::make_unique<LineEntity>(QPointF(0, 0), QPointF(10, 10));
    line->setToolId(pen);
    document->addEntity(std::move(line));
    library->removeTool(pen);
    auto* list = panel.findChild<QListWidget*>("pathList");
    return expect(document->entityAt(0)->toolId() == pen && list->item(0)->text().contains("Missing: Removable blue")
                      && !panel.findChild<QPushButton*>("startBtn")->isEnabled(),
                  "Removing a pen preserves its path reference, shows its saved name and blocks generation");
}

bool penEditorSupportsCreationAndEditing()
{
    ToolLibrary library;
    ToolsDialog dialog(library);
    auto* add = dialog.findChild<QPushButton*>("addToolButton");
    auto* name = dialog.findChild<QLineEdit*>("toolNameEdit");
    auto* enabled = dialog.findChild<QCheckBox*>("toolEnabledCheck");
    add->click();
    const QUuid id = library.tools().front().id;
    name->setText("Black fine");
    QMetaObject::invokeMethod(name, "editingFinished");
    enabled->setChecked(false);
    const bool edited = library.tool(id) && library.tool(id)->name == "Black fine" && !library.tool(id)->enabled;
    dialog.findChild<QPushButton*>("removeToolButton")->click();
    return expect(edited && library.tools().isEmpty(), "The pen editor creates, renames, disables and removes pens");
}

bool settingsSurviveRestart()
{
    auto* library = SettingsManager::instance().toolLibrary();
    const QUuid pen = library->addTool("Persistent green", QColor("#228844"));
    const QStringList arguments{"--settings-reader", pen.toString(), QCoreApplication::applicationName()};
    const int exitCode = QProcess::execute(QCoreApplication::applicationFilePath(), arguments);
    return expect(exitCode == 0, "Pen definitions persist through an actual process restart");
}

bool groupAssignmentsUndoTogether()
{
    Document document;
    std::vector<std::unique_ptr<Entity>> children;
    for (int index = 0; index < 2; ++index)
        children.push_back(std::make_unique<LineEntity>(QPointF(index, 0), QPointF(index, 10)));
    document.addEntity(std::make_unique<GroupEntity>(std::move(children)));
    document.select(document.entityAt(0)->id());
    const QUuid pen = QUuid::createUuid();
    document.assignToolToSelection(pen);
    const bool assigned = document.toolIdsForEntity(document.entityAt(0)) == QSet<QUuid>{pen};
    document.undo();
    return expect(assigned && document.toolIdsForEntity(document.entityAt(0)) == QSet<QUuid>{QUuid{}},
                  "Assigning a group updates its drawable children in one undo step");
}

bool trimmedPathsKeepTheirPen()
{
    LineEntity line(QPointF(0, 0), QPointF(10, 0));
    const QUuid pen = QUuid::createUuid();
    line.setToolId(pen);
    Document document;
    TrimModification modification(document);
    auto result = modification.trimAtIntersection(&line, QPointF(5, 0), QPointF(1, 0));
    return expect(result.success && !result.replacements.empty() && result.replacements[0]->toolId() == pen,
                  "Trim replacements retain the source path's pen before they enter the document");
}

bool splitPathsKeepTheirPen()
{
    Document document;
    auto line = std::make_unique<LineEntity>(QPointF(0, 0), QPointF(20, 0));
    const QUuid pen = QUuid::createUuid();
    line->setToolId(pen);
    document.addEntity(std::move(line));
    document.addEntity(std::make_unique<LineEntity>(QPointF(5, -10), QPointF(5, 10)));
    document.addEntity(std::make_unique<LineEntity>(QPointF(15, -10), QPointF(15, 10)));
    TrimExtendTool tool(document, *document.commandStack(), TrimExtendTool::Mode::Trim);
    tool.mouseMove(QPointF(10, 0));
    tool.mousePress(QPointF(10, 0));
    return expect(document.entityCount() == 4 && document.entityAt(0)->toolId() == pen
                      && document.entityAt(3)->toolId() == pen,
                  "Both surviving segments of a split path retain its pen");
}

bool undoStateIsUpdatedAfterAssignment()
{
    DesignCanvas canvas;
    auto* document = canvas.document();
    document->addEntity(std::make_unique<LineEntity>(QPointF(0, 0), QPointF(10, 10)));
    document->select(document->entityAt(0)->id());
    bool canUndo = false;
    QObject::connect(&canvas, &DesignCanvas::undoRedoStateChanged, &canvas,
                     [&canUndo](bool undo, bool) { canUndo = undo; });
    document->assignToolToSelection(QUuid::createUuid());
    return expect(canUndo, "Undo controls receive the updated history state after pen assignment");
}

bool invalidProjectDoesNotReplaceTheDesign()
{
    Document document;
    document.addEntity(std::make_unique<LineEntity>(QPointF(0, 0), QPointF(10, 10)));
    const QUuid original = document.entityAt(0)->id();
    QJsonObject invalidLine = document.entityAt(0)->toJson();
    invalidLine["toolId"] = "not-a-pen-id";
    const bool rejected = !document.loadFromJson(QJsonDocument(QJsonObject{{"entities", QJsonArray{invalidLine}}}));
    return expect(rejected && document.entityCount() == 1 && document.entityAt(0)->id() == original,
                  "Invalid pen references are rejected without discarding the current design");
}

bool hiddenGroupedPathsDoNotPlot()
{
    Document document;
    ToolLibrary library;
    document.addLayer("Hidden");
    document.setLayerVisible(1, false);
    std::vector<std::unique_ptr<Entity>> children;
    auto visible = std::make_unique<LineEntity>(QPointF(10, 0), QPointF(20, 0));
    visible->setToolId(library.addTool("Visible pen", Qt::blue));
    children.push_back(std::move(visible));
    auto hidden = std::make_unique<LineEntity>(QPointF(200, 100), QPointF(250, 100));
    hidden->setToolId(QUuid::createUuid());
    hidden->setLayerId(document.layerAt(1).id);
    children.push_back(std::move(hidden));
    auto group = std::make_unique<GroupEntity>(std::move(children));
    group->setLayerId(document.layerAt(0).id);
    document.addEntity(std::move(group));
    const QStringList lines = document.generateGcode(1000, 500, 5, 0, &library);
    return expect(!lines.isEmpty() && !lines.join('\n').contains("X250"),
                  "Hidden children of a group are excluded from both pen resolution and G-code");
}

bool compactDesignPanelRemainsUsable()
{
    GraphicsDesignPanel panel;
    panel.resize(980, 600);
    panel.show();
    QApplication::processEvents();
    const auto* controls = panel.findChild<QScrollArea*>("designControlsScroll");
    return expect(controls && controls->verticalScrollBar()->maximum() > 0,
                  "The path and pen controls remain scrollable in compact windows");
}

bool polarCopiesKeepTheirPen()
{
    GraphicsDesignPanel panel;
    auto* document = panel.canvas()->document();
    const QUuid pen = QUuid::createUuid();
    auto line = std::make_unique<LineEntity>(QPointF(20, 20), QPointF(40, 40));
    line->setToolId(pen);
    document->addEntity(std::move(line));
    document->select(document->entityAt(0)->id());
    panel.findChild<TransformPanel*>()->polarArrayRequested(4, 360, 10, 10);
    QSet<QUuid> identities;
    bool assigned = document->entityCount() == 4;
    for (int i = 0; i < document->entityCount(); ++i) {
        assigned &= document->entityAt(i)->toolId() == pen;
        identities.insert(document->entityAt(i)->id());
    }
    return expect(assigned && identities.size() == 4,
                  "Polar array copies retain the source pen and have distinct identities");
}

bool rackEditorAndPreviewStaySynchronized()
{
    ToolLibrary library;
    const QUuid id = library.addTool("Test pen", Qt::blue);
    ToolsDialog dialog(library);
    auto* configured = dialog.findChild<QCheckBox*>("penRackConfiguredCheck");
    auto* slot = dialog.findChild<QLineEdit*>("penSlotEdit");
    auto* x = dialog.findChild<QDoubleSpinBox*>("penSlotX");
    auto* y = dialog.findChild<QDoubleSpinBox*>("penSlotY");
    auto* rack = dialog.findChild<QCheckBox*>("rackConfiguredCheck");
    auto* macro = dialog.findChild<QPlainTextEdit*>("penReturnMacro");
    auto* current = dialog.findChild<QComboBox*>("previewCurrentPen");
    auto* next = dialog.findChild<QComboBox*>("previewNextPen");
    auto* text = dialog.findChild<QPlainTextEdit*>("toolChangePreviewText");
    if (!configured || !slot || !x || !y || !rack || !macro || !current || !next || !text)
        return expect(false, "Rack station editor and macro preview controls are available");
    slot->setText("A"); x->setValue(-40); y->setValue(-60);
    configured->setChecked(true);
    rack->setChecked(true);
    macro->setPlainText("G4 P{settleSeconds}");
    current->setCurrentIndex(current->findData(QVariant::fromValue(id)));
    next->setCurrentIndex(0); // Return only.
    const bool saved = library.penRackStation(id).configured && library.penRackStation(id).slotX == -40
        && library.penRackStation(id).returnMacro == "G4 P{settleSeconds}" && library.rackSettings().configured;
    const bool compiled = text->toPlainText().contains("Return Test pen")
        && text->toPlainText().contains("G53 G1 X-40.000 Y-60.000") && text->isReadOnly();
    x->setValue(-70);
    QMetaObject::invokeMethod(x, "editingFinished");
    const bool refreshed = text->toPlainText().contains("G53 G1 X-70.000 Y-60.000");
    macro->setPlainText("M6");
    const bool rejected = text->toPlainText().contains("Only G53") && !text->toPlainText().contains("G53 G1 X-70.000");
    return expect(saved && compiled && refreshed && rejected,
                  "Editing station positions and macros persists values and replaces stale previews/errors");
}

bool rackSettingsSurviveRestart()
{
    auto* library = SettingsManager::instance().toolLibrary();
    const QUuid id = library->addTool("Rack restart", Qt::blue);
    auto pen = library->penRackStation(id);
    pen.configured = true; pen.slot = "Restart"; pen.slotX = -25; pen.slotY = -50;
    pen.pickupMacro = "G4 P{settleSeconds}";
    library->updatePenRackStation(id, pen);
    auto rack = library->rackSettings(); rack.configured = true; rack.travelFeed = 1200;
    library->updateRackSettings(rack);
    const int result = QProcess::execute(QCoreApplication::applicationFilePath(),
        {"--rack-settings-reader", id.toString(), QCoreApplication::applicationName()});
    return expect(result == 0, "Rack positions, templates and global rack settings survive a process restart");
}

bool renderPenWidgets(const QString& outputDirectory)
{
    auto* library = SettingsManager::instance().toolLibrary();
    library->loadFromJson({});
    const QUuid black = library->addTool("Black fine", QColor("#20232a"));
    const QUuid red = library->addTool("Red marker", QColor("#dc344b"));
    PenRackSettings rack;
    rack.configured = true;
    library->updateRackSettings(rack);
    auto blackPen = library->penRackStation(black);
    blackPen.configured = true; blackPen.slot = "A"; blackPen.slotX = -40; blackPen.slotY = -60;
    blackPen.returnZ = -12; blackPen.pickupZ = -15;
    blackPen.returnMacro = "; Release mechanism still being designed.\nG4 P{settleSeconds}";
    library->updatePenRackStation(black, blackPen);
    auto redPen = library->penRackStation(red);
    redPen.configured = true; redPen.slot = "B"; redPen.slotX = -80; redPen.slotY = -60;
    redPen.returnZ = -12; redPen.pickupZ = -15;
    redPen.pickupMacro = "; Grip mechanism still being designed.\nG53 G1 Z{pickupZ} F{dockFeed}";
    library->updatePenRackStation(red, redPen);
    GraphicsDesignPanel panel;
    auto* document = panel.canvas()->document();
    document->setDefaultToolId(black);
    document->addEntity(std::make_unique<LineEntity>(QPointF(20, 20), QPointF(100, 80)));
    document->addEntity(std::make_unique<RectangleEntity>(QPointF(110, 20), QPointF(180, 80)));
    document->addEntity(std::make_unique<CircleEntity>(QPointF(70, 130), 20));
    document->select(document->entityAt(0)->id());
    document->select(document->entityAt(1)->id());
    document->assignToolToSelection(red);
    QFile stylesheet(":/style.qss");
    if (stylesheet.open(QIODevice::ReadOnly)) qApp->setStyleSheet(QString::fromUtf8(stylesheet.readAll()));
    panel.resize(1240, 860);
    panel.show();
    panel.canvas()->autoFit();
    QEventLoop rendering;
    QTimer::singleShot(100, &rendering, &QEventLoop::quit);
    rendering.exec();
    QDir().mkpath(outputDirectory);
    const bool pathsSaved = panel.grab().save(outputDirectory + "/pen-assignment.png");
    ToolsDialog dialog(*library);
    dialog.show();
    QApplication::processEvents();
    const bool pensSaved = dialog.grab().save(outputDirectory + "/pen-definitions.png");
    auto* tabs = dialog.findChild<QTabWidget*>("penRackTabs");
    tabs->setCurrentIndex(1);
    QApplication::processEvents();
    const bool rackSaved = dialog.grab().save(outputDirectory + "/rack-settings.png");
    auto* current = dialog.findChild<QComboBox*>("previewCurrentPen");
    auto* next = dialog.findChild<QComboBox*>("previewNextPen");
    current->setCurrentIndex(current->findData(QVariant::fromValue(black)));
    next->setCurrentIndex(next->findData(QVariant::fromValue(red)));
    tabs->setCurrentIndex(2);
    QApplication::processEvents();
    const bool previewSaved = dialog.grab().save(outputDirectory + "/pen-change-preview.png");
    QFile commands(outputDirectory + "/pen-change-preview.txt");
    const bool commandsSaved = commands.open(QIODevice::WriteOnly)
        && commands.write(dialog.findChild<QPlainTextEdit*>("toolChangePreviewText")->toPlainText().toUtf8()) > 0;
    return expect(pathsSaved && pensSaved && rackSaved && previewSaved && commandsSaved,
                  "Rendered assignment, station, rack and macro preview screenshots");
}

} // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    // The offscreen Windows platform does not discover system fonts automatically.
    if (app.platformName() == "offscreen") {
        QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeui.ttf");
        QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeuib.ttf");
        QFontDatabase::addApplicationFont("C:/Windows/Fonts/consola.ttf");
        QFontDatabase::addApplicationFont("C:/Windows/Fonts/seguisym.ttf");
        app.setFont(QFont("Segoe UI", 9));
    }
    QStandardPaths::setTestModeEnabled(true);
    if (app.arguments().size() == 4 && app.arguments()[1] == "--settings-reader") {
        app.setApplicationName(app.arguments()[3]);
        const auto* profile = SettingsManager::instance().toolLibrary()->tool(QUuid::fromString(app.arguments()[2]));
        return profile && profile->name == "Persistent green" && profile->previewColor == QColor("#228844") ? 0 : 1;
    }
    if (app.arguments().size() == 4 && app.arguments()[1] == "--rack-settings-reader") {
        app.setApplicationName(app.arguments()[3]);
        const auto* library = SettingsManager::instance().toolLibrary();
        const auto* pen = library->tool(QUuid::fromString(app.arguments()[2]));
        const auto station = library->penRackStation(QUuid::fromString(app.arguments()[2]));
        return pen && station.configured && station.slotX == -25 && station.slotY == -50
            && station.pickupMacro == "G4 P{settleSeconds}" && library->rackSettings().configured
            && library->rackSettings().travelFeed == 1200 ? 0 : 1;
    }
    app.setApplicationName("GrblSenderPenTests-" + QUuid::createUuid().toString(QUuid::WithoutBraces));
    const bool project = assignmentSurvivesProjectRoundTrip();
    const bool group = nestedAssignmentSurvivesProjectRoundTrip();
    const bool undo = canvasUndoUsesDocumentCommands();
    const bool bulk = bulkAssignmentUndoesTogether();
    const bool copies = editsAndCopiesKeepThePen();
    const bool defaults = defaultAndLegacyProjectsWork();
    const bool generation = multiPenGenerationIsBlocked();
    const bool library = libraryPreservesStableReferences();
    const bool controls = assignmentControlsExist();
    const bool widgets = pathSelectionAndAssignmentWorkTogether();
    const bool missing = missingPenRemainsVisible();
    const bool editor = penEditorSupportsCreationAndEditing();
    const bool restart = settingsSurviveRestart();
    const bool groupUndo = groupAssignmentsUndoTogether();
    const bool trim = trimmedPathsKeepTheirPen();
    const bool split = splitPathsKeepTheirPen();
    const bool undoState = undoStateIsUpdatedAfterAssignment();
    const bool invalidProject = invalidProjectDoesNotReplaceTheDesign();
    const bool hiddenGroups = hiddenGroupedPathsDoNotPlot();
    const bool compactPanel = compactDesignPanelRemainsUsable();
    const bool polarCopies = polarCopiesKeepTheirPen();
    const bool rackWidgets = rackEditorAndPreviewStaySynchronized();
    const bool rackRestart = rackSettingsSurviveRestart();
    bool screenshots = true;
    const int screenshotArgument = app.arguments().indexOf("--screenshots");
    if (screenshotArgument >= 0 && screenshotArgument + 1 < app.arguments().size())
        screenshots = renderPenWidgets(app.arguments()[screenshotArgument + 1]);
    const QString testConfig = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QFile::remove(testConfig + "/settings.json");
    QDir().rmdir(testConfig);
    if (!project || !group || !undo || !bulk || !copies || !defaults || !generation || !library || !controls)
        return 1;
    if (!widgets || !missing || !editor || !restart || !groupUndo || !trim || !split || !undoState) return 1;
    if (!invalidProject || !hiddenGroups || !compactPanel || !polarCopies || !screenshots) return 1;
    if (!rackWidgets || !rackRestart) return 1;
    std::cout << "All pen-assignment tests passed.\n";
    return 0;
}
