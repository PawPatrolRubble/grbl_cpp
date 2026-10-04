#include "grbl/ToolLibrary.h"
#include "ui/ToolsDialog.h"
#include "ui/ToolpathPanel.h"
#include "ui/panels/GraphicsDesignPanel.h"
#include "SettingsManager.h"
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QFontDatabase>
#include <QStyleFactory>
#include <QProcess>
#include <QPushButton>
#include <QStandardPaths>
#include <iostream>

namespace {
int checks = 0, failures = 0;
void expect(bool valid, const char* message) {
    ++checks;
    if (!valid) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
}
QJsonObject legacyTool(const QUuid& id) {
    return {{"id", id.toString()}, {"name", "Red fine"}, {"color", "#cc2233"}, {"enabled", false},
            {"rack", QJsonObject{{"configured", true}, {"motionCalibrated", true}, {"slot", "A"},
                                {"x", -40}, {"y", -60}, {"returnZ", -12}, {"pickupZ", -15},
                                {"returnMacro", "G4 P0.1"}, {"pickupMacro", "G4 P0.2"}}}};
}
void libraryMigration() {
    const auto id = QUuid::createUuid(); ToolLibrary library;
    expect(library.loadFromJson({legacyTool(id)}), "legacy pen definitions remain readable");
    const auto record = library.toJson().first().toObject();
    expect(record["id"].toString() == id.toString() && record["name"] == "Red fine" && !record["enabled"].toBool(),
           "migration retains stable IDs, names and availability");
    expect(record["kind"] == "pen", "migrated tools have an explicit supported Pen kind");
    expect(library.tool(id)->kind == ToolKind::Pen, "legacy and new definitions use the typed Pen kind");
    expect(QColor(record["previewColor"].toString()) == QColor("#cc2233"), "legacy ink color becomes the generic preview color");
    expect(record["rack"] == legacyTool(id)["rack"], "migration retains every station field and calibration");
    const auto fingerprint = library.configurationFingerprint();
    ToolLibrary restored;
    expect(restored.loadFromJson(library.toJson()) && restored.configurationFingerprint() == fingerprint,
           "canonical round trip preserves the configuration fingerprint");
    auto definition = *restored.tool(id); definition.name = "Red pen renamed"; definition.previewColor = Qt::blue;
    expect(restored.updateTool(definition) && restored.penRackStation(id).motionCalibrated
           && restored.penRackStation(id).slotX == -40, "name and preview-color edits leave the calibrated machine station intact");
    auto station = restored.penRackStation(id); station.slotX -= 1;
    expect(restored.updatePenRackStation(id, station) && !restored.penRackStation(id).motionCalibrated,
           "station motion edits still invalidate calibration independently of tool metadata");
    station = restored.penRackStation(id); station.motionCalibrated = true;
    expect(restored.updatePenRackStation(id, station) && restored.penRackStation(id).motionCalibrated,
           "calibration can be confirmed after the station motion is verified");
    definition.kind = static_cast<ToolKind>(99);
    expect(!restored.updateTool(definition) && restored.addTool("Unsupported", Qt::red, definition.kind).isNull(),
           "unsupported in-memory tool types cannot enter the library through add or update");
    expect(!restored.updatePenRackStation(QUuid::createUuid(), station), "stations must reference an existing supported tool");
    expect(restored.removeTool(id) && !restored.penRackStation(id).configured,
           "removing a definition also removes its station binding");
    auto invalid = legacyTool(id); invalid["kind"] = "endMill";
    expect(!library.loadFromJson({invalid}) && library.configurationFingerprint() == fingerprint,
           "unsupported tool kinds are rejected without replacing calibrated definitions");
    invalid["kind"] = 12;
    expect(!library.loadFromJson({invalid}), "malformed tool kinds cannot silently become pens");
    auto current = legacyTool(id); current["kind"] = "pen"; current["previewColor"] = "#2563eb";
    expect(library.loadFromJson({current}) && library.toJson().first().toObject()["previewColor"] == "#ff2563eb",
           "explicit current preview color takes precedence over a legacy color");
    current["previewColor"] = "invalid";
    expect(!library.loadFromJson({current}), "an invalid explicit preview color cannot fall back to a legacy color");
}
int settingsMigration(const QString& mode) {
    const auto id = QUuid::createUuid(), shadow = QUuid::createUuid();
    QJsonObject root{{"penRack", QJsonObject{{"configured", true}, {"motionCalibrated", true}}}};
    if (mode == "legacy") root["pens"] = QJsonArray{legacyTool(id)};
    else {
        auto current = legacyTool(id); current["kind"] = "pen"; current["previewColor"] = "#cc2233";
        root["tools"] = mode == "empty" ? QJsonArray{} : QJsonArray{current};
        root["pens"] = QJsonArray{legacyTool(shadow)};
    }
    const auto directory = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(directory); QFile file(directory + "/settings.json");
    expect(file.open(QIODevice::WriteOnly), "isolated settings fixture opens");
    file.write(QJsonDocument(root).toJson()); file.close();
    auto& settings = SettingsManager::instance(); auto* library = settings.toolLibrary();
    expect(mode == "empty" ? library->tools().isEmpty() : library->tool(id) && !library->tool(shadow),
           "settings read generic tools first and use legacy pens only when tools is absent");
    if (mode != "empty") expect(library->tool(id) && library->penRackStation(id).configured,
                                 "settings migration retains the legacy rack station");
    settings.save(); expect(file.open(QIODevice::ReadOnly), "migrated settings opens for verification");
    const auto saved = QJsonDocument::fromJson(file.readAll()).object();
    expect(saved.contains("tools") && !saved.contains("pens"), "settings write a single authoritative generic tools collection");
    expect(saved["tools"].toArray().size() == (mode == "empty" ? 0 : 1), "settings migration preserves the authoritative collection size");
    expect(saved["penRack"].toObject()["motionCalibrated"].toBool(), "global pen-rack calibration survives migration");
    return failures ? 1 : 0;
}
void uiTerminology(const QString& screenshots) {
    ToolLibrary library; library.addTool("Red pen", Qt::red);
    ToolsDialog dialog(library);
    expect(dialog.windowTitle().contains("Tools"), "library editor has a generic Tools title");
    auto* type = dialog.findChild<QLabel*>("toolTypeLabel");
    expect(type && type->text() == "Pen", "tool editor explicitly identifies the supported Pen type");
    if (!screenshots.isEmpty()) {
        dialog.show(); QApplication::processEvents();
        expect(dialog.grab().save(screenshots + "/tools-library.png"), "generic tools editor screenshot saves");
    }
    GraphicsDesignPanel design;
    bool assign = false, defaultTool = false;
    for (auto* label : design.findChildren<QLabel*>()) {
        assign = assign || label->text() == "Assign tool:";
        defaultTool = defaultTool || label->text() == "Default tool:";
    }
    expect(assign && defaultTool, "shared path controls use Assign tool and Default tool");
    ToolpathPanel preview;
    auto* filter = preview.findChild<QComboBox*>("toolpathToolFilter");
    expect(filter && filter->accessibleName() == "Show tool" && filter->itemText(0) == "All tools",
           "shared preview filtering uses Show tool and All tools");
}
}
int main(int argc, char** argv) {
    QApplication app(argc, argv);
    const auto args = app.arguments(); const auto child = args.indexOf("--settings");
    QCoreApplication::setOrganizationName("GrblSenderTests");
    QCoreApplication::setApplicationName("GenericToolLibrary" + (child >= 0 ? args.value(child + 1) : "parent"));
    QStandardPaths::setTestModeEnabled(true);
    if (child >= 0) return settingsMigration(args.value(child + 1));
    if (app.platformName() == "offscreen") {
        for (const auto* font : {"segoeui.ttf", "segoeuib.ttf", "seguisym.ttf", "consola.ttf"})
            QFontDatabase::addApplicationFont(QString("C:/Windows/Fonts/") + font);
    }
    app.setFont(QFont("Segoe UI", 10)); app.setStyle(QStyleFactory::create("Fusion"));
    QFile style(":/resources/style.qss");
    if (style.open(QIODevice::ReadOnly)) app.setStyleSheet(QString::fromUtf8(style.readAll()));
    QString screenshots; const auto screenshotFlag = args.indexOf("--screenshots");
    if (screenshotFlag >= 0 && screenshotFlag + 1 < args.size()) {
        screenshots = args[screenshotFlag + 1]; QDir().mkpath(screenshots);
    }
    libraryMigration(); uiTerminology(screenshots);
    for (const auto* mode : {"legacy", "current", "empty"}) {
        QProcess process; process.start(app.applicationFilePath(), {"--settings", mode});
        const bool finished = process.waitForFinished(15000);
        expect(finished && process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0,
               "legacy/current/empty settings migration succeeds in an independent application session");
        if (!finished || process.exitCode()) std::cerr << process.readAllStandardError().toStdString();
    }
    std::cout << checks << " tool library checks, " << failures << " failures.\n";
    return failures ? 1 : 0;
}
