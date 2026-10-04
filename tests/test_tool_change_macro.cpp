#include "grbl/ToolChangeMacroCompiler.h"
#include <QCoreApplication>
#include <QJsonObject>
#include <iostream>
#include <limits>

namespace {
int failures = 0;
void check(bool condition, const char* description)
{
    std::cout << (condition ? "PASS: " : "FAIL: ") << description << '\n';
    if (!condition) ++failures;
}
struct Fixture
{
    ToolLibrary library;
    QUuid black, red;
    Fixture()
    {
        PenRackSettings rack;
        rack.configured = true;
        library.updateRackSettings(rack);
        black = library.addTool("Black", Qt::black);
        red = library.addTool("Red", Qt::red);
        configure(black, "A", -40, -60);
        configure(red, "B", -80, -60);
    }
    void configure(const QUuid& id, const QString& slot, double x, double y)
    {
        auto pen = library.penRackStation(id);
        pen.configured = true;
        pen.slot = slot;
        pen.slotX = x; pen.slotY = y;
        pen.returnZ = -12; pen.pickupZ = -15;
        library.updatePenRackStation(id, pen);
    }
    ToolChangePreview change() { return ToolChangeMacroCompiler::compile(library, {black, red}); }
};
void persistence()
{
    Fixture f;
    auto pen = f.library.penRackStation(f.black);
    pen.returnMacro = "G53 G1 X{slotX} Z{returnZ} F{dockFeed}";
    f.library.updatePenRackStation(f.black, pen);
    ToolLibrary restored;
    const bool loaded = restored.loadFromJson(f.library.toJson()) && restored.loadRackFromJson(f.library.rackToJson());
    const auto copy = restored.penRackStation(f.black);
    check(loaded && copy.configured && copy.slot == "A" && copy.slotX == -40
              && copy.returnMacro == pen.returnMacro && restored.rackSettings().configured,
          "Rack positions, contact heights, templates and rack settings round-trip");
    QJsonArray legacy{QJsonObject{{"id", f.black.toString()}, {"name", "Old"}, {"color", "#000000"}}};
    check(restored.loadFromJson(legacy) && !restored.penRackStation(f.black).configured
              && restored.loadRackFromJson({}) && !restored.rackSettings().configured,
          "Existing pen settings load with an unconfigured rack");
    auto invalid = f.library.toJson();
    auto object = invalid[0].toObject();
    object["rack"] = QJsonObject{{"x", "bad"}};
    invalid[0] = object;
    check(!f.library.loadFromJson(invalid) && f.library.penRackStation(f.black).slotX == -40,
          "Malformed station data is rejected without replacing saved pens");
    auto rack = f.library.rackSettings();
    rack.travelFeed = std::numeric_limits<double>::infinity();
    check(!f.library.updateRackSettings(rack), "Non-finite rack values cannot be persisted");
}
void sequence()
{
    Fixture f;
    const auto change = f.change();
    const auto commands = change.commandLines();
    const QString joined = commands.join('\n');
    check(change.valid() && commands.contains("G53 G1 X-40.000 Y-60.000 F1000.000")
              && commands.indexOf("G53 G1 Z-12.000 F300.000") < commands.indexOf("G53 G1 Z-15.000 F300.000")
              && commands.contains("G53 G1 X-80.000 Y-60.000 F1000.000")
              && commands.last() == "G1 F1000.000" && !joined.contains('{')
              && joined.count("G4 P0.01") >= 5,
          "Return then pickup use separate clearance moves, machine coordinates and checkpoints");
    check(change.valid() && !change.warnings.isEmpty() && change.displayText().contains("release", Qt::CaseInsensitive),
          "Undefined release/grip hooks are clearly identified as motion-only drafts");
    const auto load = ToolChangeMacroCompiler::compile(f.library, {{}, f.red});
    check(load.valid() && !load.displayText().contains("Return Black") && load.commandLines().join('\n').contains("Z-15.000"),
          "An empty holder loads the requested pen without returning a pen");
    const auto returned = ToolChangeMacroCompiler::compile(f.library, {f.black, {}});
    check(returned.valid() && !returned.commandLines().join('\n').contains("Z-15.000"),
          "Final return omits pickup");
    const auto same = ToolChangeMacroCompiler::compile(f.library, {f.black, f.black});
    check(same.valid() && same.commandLines().isEmpty(), "Requesting the current pen avoids an unnecessary exchange");
}
void templatesAndValidation()
{
    Fixture f;
    auto pen = f.library.penRackStation(f.red);
    pen.pickupMacro = "G53 G1 X{slotX} Y{slotY} Z{pickupZ} F{dockFeed}\nG4 P{settleSeconds}";
    f.library.updatePenRackStation(f.red, pen);
    auto preview = f.change();
    check(preview.valid() && preview.commandLines().contains("G53 G1 X-80.000 Y-60.000 Z-15.000 F300.000")
              && preview.commandLines().contains("G4 P0.300"),
          "Named macro values expand to finite numeric commands");
    for (const QString& rejected : {"G53 G1 Z{unknown} F300", "G1 Z-15 F300", "G91", "$H", "M6", "M3 S1000",
                                   "G53 G1 Z-999 F300", "G53 G1 X-150 F300", "G53 G1 Z-15 F0",
                                   "G4 P-1", "G53 G1 Z-15 Z-14 F300", "G53 G1 Z-15 F300!"}) {
        pen.pickupMacro = rejected;
        f.library.updatePenRackStation(f.red, pen);
        preview = f.change();
        check(!preview.valid() && preview.commandLines().isEmpty()
                  && preview.errors.join('\n').contains("Red"), qPrintable("Reject invalid Red pickup: " + rejected));
    }
    pen.pickupMacro = "G4 P0.1";
    pen.slot = "a";
    f.library.updatePenRackStation(f.red, pen);
    check(!f.change().valid(), "Two pens cannot share a physical slot name");
    pen.slot = "B"; pen.slotX = -40;
    f.library.updatePenRackStation(f.red, pen);
    check(!f.change().valid(), "Different slot labels cannot conceal the same station coordinates");
    pen.slotX = -80; pen.pickupZ = 1;
    f.library.updatePenRackStation(f.red, pen);
    check(!f.change().valid(), "Contact heights must fit the travel bounds and stay below clearance");
    pen.pickupZ = -15;
    auto disabled = *f.library.tool(f.red); disabled.enabled = false; f.library.updateTool(disabled);
    f.library.updatePenRackStation(f.red, pen);
    check(!f.change().valid(), "Unavailable target pens cannot be loaded");
    check(!ToolChangeMacroCompiler::compile(f.library, {{}, QUuid::createUuid()}).valid(),
          "Missing target references produce a compilation error");
    auto rack = f.library.rackSettings(); rack.configured = false;
    f.library.updateRackSettings(rack);
    check(!ToolChangeMacroCompiler::compile(f.library, {{}, f.black}).valid(),
          "Unconfigured racks cannot produce an exchange preview");
    rack.configured = true; rack.dockFeed = 0.0001;
    f.library.updateRackSettings(rack);
    check(!ToolChangeMacroCompiler::compile(f.library, {{}, f.black}).valid(),
          "Feeds that round to zero cannot generate invalid motion commands");
}

void formattedCommandsAndComments()
{
    Fixture f;
    auto pen = f.library.penRackStation(f.red);
    pen.pickupMacro = "g53g1z{pickupZ}f{dockFeed} ; M6 and $H stay comments\ng4p.25";
    f.library.updatePenRackStation(f.red, pen);
    auto result = f.change();
    check(result.valid() && result.commandLines().contains("G4 P0.250")
              && !result.commandLines().join('\n').contains("M6"),
          "Compact lowercase commands normalize and comments never enter compiled commands");
    pen.pickupMacro = "G53 G1 Z-15 F0.0001";
    f.library.updatePenRackStation(f.red, pen);
    check(!f.change().valid(), "Macro feeds cannot round to zero");
    pen.pickupMacro = "G53 G1 Z-15 F" + QString(90, '9');
    f.library.updatePenRackStation(f.red, pen);
    check(!f.change().valid(), "Oversized expanded command blocks are rejected");
    pen.pickupMacro = QString("G4 P0.1\n").repeated(257);
    f.library.updatePenRackStation(f.red, pen);
    check(!f.change().valid(), "Oversized templates are rejected");
    pen.pickupMacro = "; grip mechanism unset";
    auto renamed = *f.library.tool(f.red); renamed.name = "Red\nM6"; f.library.updateTool(renamed);
    f.library.updatePenRackStation(f.red, pen);
    result = f.change();
    check(result.valid() && !result.displayText().contains("\nM6"),
          "Pen labels cannot inject a command line into the displayed preview");
    const auto badFrame = ToolChangeMacroCompiler::compile(f.library, {f.black, f.red, 53, 1000});
    const auto badFeed = ToolChangeMacroCompiler::compile(f.library, {f.black, f.red, 54, 0.0001});
    check(!badFrame.valid() && !badFeed.valid(), "Plotting-mode restoration validates frame and formatted feed");
    auto rackJson = f.library.rackToJson(); rackJson["clearanceZ"] = "not a coordinate";
    check(!f.library.loadRackFromJson(rackJson) && f.library.rackSettings().clearanceZ == 0,
          "Malformed global rack settings preserve the previous configuration");
    pen.pickupMacro = "G4 P3600";
    f.library.updatePenRackStation(f.red, pen);
    check(!f.change().valid(), "Dwell duration stays within stock GRBL's timer capacity");
    pen.pickupMacro = "G53 G1 Z-15 F" + QString(42, '9');
    f.library.updatePenRackStation(f.red, pen);
    check(!f.change().valid(), "Numeric commands must fit GRBL's floating-point range");
    auto rack = f.library.rackSettings(); rack.dockFeed = 1e45;
    f.library.updateRackSettings(rack);
    check(!ToolChangeMacroCompiler::compile(f.library, {{}, f.black}).valid(),
          "Generated rack commands also reject firmware numeric overflow");
}
}
int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    persistence(); sequence(); templatesAndValidation(); formattedCommandsAndComments();
    std::cout << failures << " tool-change test failures.\n";
    return failures ? 1 : 0;
}
