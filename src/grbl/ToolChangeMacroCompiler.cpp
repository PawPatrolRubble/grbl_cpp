#include "ToolChangeMacroCompiler.h"
#include <QMap>
#include <QRegularExpression>
#include <cmath>
#include <limits>

namespace {
constexpr int maxBlockBytes = 79; // Stock GRBL's 80-byte line buffer includes its terminator.
constexpr int maxMacroLines = 256;
QString number(double value) { return QString::number(value, 'f', 3); }
bool firmwareNumber(double value)
{
    return std::isfinite(value) && std::abs(value) <= static_cast<double>(std::numeric_limits<float>::max());
}
bool inRange(double value, double minimum, double maximum)
{
    return firmwareNumber(value) && value >= minimum && value <= maximum;
}
bool bounded(double value, double minimum, double maximum)
{
    // Check the coordinate that will actually be sent, after formatting.
    return inRange(value, minimum, maximum) && inRange(number(value).toDouble(), minimum, maximum);
}
QStringList compileLocal(const QString& source, const PenRackStation& station, const PenRackSettings& rack,
                         const QString& context, QStringList& errors)
{
    if (source.size() > 32768 || source.count('\n') >= maxMacroLines) {
        errors << context + ": template exceeds 256 lines or 32,768 characters.";
        return {};
    }
    const QMap<QString, double> values{{"slotX", station.slotX}, {"slotY", station.slotY},
        {"returnZ", station.returnZ}, {"pickupZ", station.pickupZ}, {"clearanceZ", rack.clearanceZ},
        {"travelFeed", rack.travelFeed}, {"dockFeed", rack.dockFeed}, {"settleSeconds", rack.settleSeconds}};
    const QRegularExpression placeholder("\\{([A-Za-z][A-Za-z0-9]*)\\}");
    const QRegularExpression word("([A-Za-z])([+-]?(?:[0-9]+(?:\\.[0-9]*)?|\\.[0-9]+))");
    QStringList output;
    double x = station.slotX, y = station.slotY;
    int lineNumber = 0;
    for (QString line : source.split('\n')) {
        ++lineNumber;
        line = line.section(';', 0, 0).trimmed();
        if (line.isEmpty()) continue;
        const auto fail = [&](const QString& message) {
            errors << context + QString(" / line %1: ").arg(lineNumber) + message;
        };
        auto placeholders = placeholder.globalMatch(line);
        bool valid = true;
        while (placeholders.hasNext()) {
            const auto match = placeholders.next();
            if (!values.contains(match.captured(1))) {
                fail("Unknown value {" + match.captured(1) + "}."); valid = false; break;
            }
        }
        if (!valid) continue;
        for (auto it = values.constBegin(); it != values.constEnd(); ++it)
            line.replace("{" + it.key() + "}", number(it.value()));
        line.remove(QRegularExpression("[ \\t\\r]"));
        line = line.toUpper();
        QList<double> gCodes;
        QMap<QChar, double> words;
        auto matches = word.globalMatch(line);
        qsizetype consumed = 0;
        while (matches.hasNext()) {
            const auto match = matches.next();
            const QChar letter = match.captured(1)[0];
            bool numeric = false;
            const double value = match.captured(2).toDouble(&numeric);
            if (match.capturedStart() != consumed || !numeric || !firmwareNumber(value)
                || (letter != 'G' && words.contains(letter))) { valid = false; break; }
            consumed = match.capturedEnd();
            if (letter == 'G') gCodes.append(value);
            else words[letter] = value;
        }
        if (!valid || consumed != line.size()) { fail("Invalid or repeated command words."); continue; }
        QString normalized;
        if (gCodes.size() == 1 && gCodes[0] == 4 && words.size() == 1 && words.contains('P')) {
            if (!inRange(words['P'], 0, ToolChangeMacroCompiler::maxDwellSeconds)) {
                fail("Dwell must be between 0 and 3000 seconds."); continue;
            }
            normalized = "G4 P" + number(words['P']);
        } else if (gCodes.size() == 2 && gCodes.contains(53) && (gCodes.contains(0) || gCodes.contains(1))) {
            const bool linear = gCodes.contains(1);
            bool axes = false;
            for (auto it = words.constBegin(); it != words.constEnd(); ++it) {
                if (it.key() == 'X' || it.key() == 'Y' || it.key() == 'Z') axes = true;
                else if (it.key() != 'F' || !linear) valid = false;
            }
            if (!axes || !valid || (linear && (!words.contains('F') || words['F'] <= 0 || number(words['F']).toDouble() <= 0))) {
                fail("Use explicit G53 G0/G1 with axes and a positive F for G1."); continue;
            }
            const double targetX = words.value('X', x), targetY = words.value('Y', y);
            if (!bounded(targetX, rack.minX, rack.maxX) || !bounded(targetY, rack.minY, rack.maxY)
                || (words.contains('Z') && !bounded(words['Z'], rack.minZ, rack.maxZ))) {
                fail("Local motion is outside the configured machine bounds."); continue;
            }
            if (std::hypot(number(targetX).toDouble() - station.slotX, number(targetY).toDouble() - station.slotY)
                > rack.localRadius) { fail("Local motion leaves this station's docking radius."); continue; }
            normalized = linear ? "G53 G1" : "G53 G0";
            for (QChar axis : {'X', 'Y', 'Z', 'F'})
                if (words.contains(axis)) normalized += " " + QString(axis) + number(words[axis]);
            x = targetX; y = targetY;
        } else { fail("Only G53 G0/G1 local motion and G4 dwell are supported; actuator commands are not configured."); continue; }
        if (normalized.toUtf8().size() > maxBlockBytes) { fail("Expanded block exceeds 79 bytes."); continue; }
        output << normalized;
    }
    return output;
}
void validateRack(const PenRackSettings& rack, QStringList& errors)
{
    if (!rack.configured) errors << "Configure the rack's clearance, feeds and machine bounds first.";
    if (rack.minX >= rack.maxX || rack.minY >= rack.maxY || rack.minZ >= rack.maxZ)
        errors << "Machine minimum coordinates must be below their maximum coordinates.";
    if (!bounded(rack.clearanceZ, rack.minZ, rack.maxZ)) errors << "Rack clearance Z is outside machine bounds.";
    if (!firmwareNumber(rack.travelFeed) || !firmwareNumber(rack.dockFeed)
        || rack.travelFeed <= 0 || rack.dockFeed <= 0 || number(rack.travelFeed).toDouble() <= 0
        || number(rack.dockFeed).toDouble() <= 0) errors << "Rack travel and docking feeds must be positive.";
    if (!inRange(rack.settleSeconds, 0, ToolChangeMacroCompiler::maxDwellSeconds) || rack.localRadius <= 0)
        errors << "Settle time must be 0–3000 seconds and the local docking radius must be positive.";
}
void validatePen(const ToolDefinition& pen, const PenRackStation& station, const PenRackSettings& rack, QStringList& errors)
{
    if (!station.configured || station.slot.trimmed().isEmpty()) errors << pen.name + ": configure its rack slot first.";
    if (!bounded(station.slotX, rack.minX, rack.maxX) || !bounded(station.slotY, rack.minY, rack.maxY))
        errors << pen.name + ": station X/Y is outside machine bounds.";
    if (!bounded(station.returnZ, rack.minZ, rack.maxZ) || !bounded(station.pickupZ, rack.minZ, rack.maxZ)
        || station.returnZ >= rack.clearanceZ || station.pickupZ >= rack.clearanceZ
        || number(station.returnZ).toDouble() >= number(rack.clearanceZ).toDouble()
        || number(station.pickupZ).toDouble() >= number(rack.clearanceZ).toDouble())
        errors << pen.name + ": contact heights must lie below clearance and within machine bounds.";
}
} // namespace

QStringList ToolChangePreview::commandLines() const
{
    QStringList lines;
    if (valid()) for (const auto& phase : phases) lines << phase.commands;
    return lines;
}
QString ToolChangePreview::displayText() const
{
    if (!valid()) return "Cannot compile this preview:\n\n" + errors.join('\n');
    QStringList text;
    text << "; Preview only — no commands are sent to the machine.";
    for (const auto& warning : warnings) text << "; " + warning.simplified();
    if (phases.isEmpty()) text << "; No pen exchange is needed.";
    for (const auto& phase : phases) { text << "\n; " + phase.title.simplified(); text << phase.commands; }
    return text.join('\n');
}
ToolChangePreview ToolChangeMacroCompiler::compile(const ToolLibrary& library, const ToolChangeRequest& request)
{
    ToolChangePreview preview;
    const auto* current = request.currentToolId.isNull() ? nullptr : library.tool(request.currentToolId);
    const auto* next = request.nextToolId.isNull() ? nullptr : library.tool(request.nextToolId);
    if (!request.currentToolId.isNull() && !current) preview.errors << "The current pen reference is missing.";
    if (!request.nextToolId.isNull() && !next) preview.errors << "The requested pen reference is missing.";
    if ((current && current->kind != ToolKind::Pen) || (next && next->kind != ToolKind::Pen))
        preview.errors << "Passive pen rack exchanges require Pen tools.";
    if (next && !next->enabled) preview.errors << next->name + ": the requested pen is unavailable.";
    if (!preview.valid() || request.currentToolId == request.nextToolId) return preview;
    const auto& rack = library.rackSettings();
    validateRack(rack, preview.errors);
    if (request.workCoordinateSystem < 54 || request.workCoordinateSystem > 59
        || !firmwareNumber(request.drawingFeed) || request.drawingFeed <= 0 || number(request.drawingFeed).toDouble() <= 0)
        preview.errors << "Choose a plotting work coordinate system G54–G59 and a positive drawing feed.";
    if (current) validatePen(*current, library.penRackStation(current->id), rack, preview.errors);
    if (next) validatePen(*next, library.penRackStation(next->id), rack, preview.errors);
    const auto& tools = library.tools();
    for (qsizetype i = 0; i < tools.size(); ++i) {
        const auto first = library.penRackStation(tools[i].id);
        if (!first.configured) continue;
        for (qsizetype j = i + 1; j < tools.size(); ++j) {
            const auto second = library.penRackStation(tools[j].id);
            if (!second.configured) continue;
            if (first.slot.compare(second.slot, Qt::CaseInsensitive) == 0
                || (number(first.slotX) == number(second.slotX) && number(first.slotY) == number(second.slotY)))
                preview.errors << tools[i].name + " and " + tools[j].name + ": rack slots must be unique.";
        }
    }
    if (!preview.valid()) return preview;
    preview.warnings << "Passive rack motion: verify pickup, release and clear travel on the machine before enabling automatic jobs.";
    auto add = [&](const QString& title, const QUuid& pen, const QStringList& lines) {
        preview.phases.append({title, pen, lines});
    };
    add("Set metric / absolute modes", {}, {"G21 G90 G94"});
    auto exchange = [&](const ToolDefinition& pen, bool pickup) {
        const auto station = library.penRackStation(pen.id);
        const QString action = pickup ? "Pickup " : "Return ";
        const QString title = action + pen.name;
        add(title + " — raise to clearance", pen.id,
            {"G53 G1 Z" + number(rack.clearanceZ) + " F" + number(rack.dockFeed), "G4 P0.01"});
        add(title + " — approach slot " + station.slot, pen.id,
            {"G53 G1 X" + number(station.slotX) + " Y" + number(station.slotY) + " F" + number(rack.travelFeed), "G4 P0.01"});
        add(title + " — lower to contact", pen.id,
            {"G53 G1 Z" + number(pickup ? station.pickupZ : station.returnZ) + " F" + number(rack.dockFeed), "G4 P0.01"});
        auto local = compileLocal(pickup ? station.pickupMacro : station.returnMacro, station, rack,
                                  pen.name + (pickup ? " / pickup" : " / return"), preview.errors);
        if (local.isEmpty()) preview.warnings << pen.name + ": " + (pickup ? "grip" : "release") + " steps are undefined.";
        local << "G4 P" + number(rack.settleSeconds);
        local << "G4 P0.01";
        add(title + (pickup ? " — grip / local steps and settle" : " — release / local steps and settle"), pen.id, local);
        add(title + " — retract", pen.id,
            {"G53 G1 Z" + number(rack.clearanceZ) + " F" + number(rack.dockFeed), "G4 P0.01"});
        preview.phases.last().holderEffect = pickup ? ToolChangePhase::HolderEffect::Loaded
                                                   : ToolChangePhase::HolderEffect::Empty;
    };
    if (current) exchange(*current, false);
    if (next) exchange(*next, true);
    add("Restore plotting modes (holder remains raised)", {},
        {QString("G21 G90 G17 G94 G91.1 G%1").arg(request.workCoordinateSystem), "G1 F" + number(request.drawingFeed)});
    if (!preview.valid()) preview.phases.clear();
    for (const auto& line : preview.commandLines())
        if (line.toUtf8().size() > maxBlockBytes) { preview.errors << "A generated block exceeds 79 bytes."; break; }
    return preview;
}
