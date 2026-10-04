#include "PlotJobBuilder.h"
#include "grbl/GcodeParser.h"
#include "grbl/ToolLibrary.h"
#include "grbl/ToolChangeMacroCompiler.h"
#include "grbl/GraphicsDesign/Document.h"
#include "grbl/GraphicsDesign/GroupEntity.h"
#include <cmath>
#include <limits>
#include <QRegularExpression>

namespace {
bool firmwareNumber(double value)
{
    return std::isfinite(value) && std::abs(value) <= std::numeric_limits<float>::max();
}
void collectLeaves(const Entity* entity, const Document& document, QList<const Entity*>& leaves)
{
    if (!entity || !entity->isDrawable() || !entity->isEnabled()
        || !document.isEntityVisible(entity)) return;
    if (const auto* group = dynamic_cast<const GroupEntity*>(entity)) {
        for (const auto& child : group->children()) collectLeaves(child.get(), document, leaves);
    } else leaves << entity;
}
}

PlotJobBuildResult PlotJobBuilder::build(const Document& document, const PlotOptions& options,
                                       const ToolLibrary* tools)
{
    PlotJobBuildResult result;
    if (!firmwareNumber(options.drawingFeed) || !firmwareNumber(options.plungeFeed)
        || std::round(options.drawingFeed) <= 0 || std::round(options.plungeFeed) <= 0)
        result.errors << "Drawing and Z feeds must be positive finite firmware values.";
    if (!firmwareNumber(options.safeZ) || !firmwareNumber(options.drawingZ)
        || options.safeZ == options.drawingZ)
        result.errors << "Choose distinct finite pen-up and drawing heights.";
    if (options.workCoordinateSystem < 54 || options.workCoordinateSystem > 59)
        result.errors << "Choose a plotting coordinate system G54–G59.";
    if (!result.valid()) return result;

    QList<const Entity*> leaves;
    for (const auto& entity : document.entities()) collectLeaves(entity.get(), document, leaves);
    QSet<QUuid> pens;
    QList<PlotJobBlock> drawing;
    std::array<double, 3> minimum{INFINITY, INFINITY, INFINITY};
    std::array<double, 3> maximum{-INFINITY, -INFINITY, -INFINITY};
    auto include = [&](double x, double y, double z) {
        const std::array<double, 3> point{x, y, z};
        for (int axis = 0; axis < 3; ++axis) {
            // Include both model and serialized coordinates (four decimal places).
            minimum[axis] = std::min(minimum[axis], std::min(point[axis], std::round(point[axis] * 10000) / 10000));
            maximum[axis] = std::max(maximum[axis], std::max(point[axis], std::round(point[axis] * 10000) / 10000));
        }
    };
    for (const Entity* entity : leaves) {
        const auto pen = entity->toolId().isNull() ? document.defaultToolId() : entity->toolId();
        auto moves = entity->toGcodeMoves(options.safeZ, options.drawingZ, options.drawingFeed);
        if (moves.isEmpty()) continue;
        if (options.automaticRack && (moves.first().type != GcodeMove::Type::Rapid
            || std::abs(moves.first().end.z() - options.safeZ) > 0.0001
            || std::abs(moves.last().end.z() - options.safeZ) > 0.0001))
            result.errors << "Every automatic path must enter and finish at the pen-up height.";
        pens.insert(pen);
        if (!pen.isNull()) {
            const auto* profile = tools ? tools->tool(pen) : nullptr;
            if (!profile || !profile->enabled || profile->kind != ToolKind::Pen)
                result.errors << "A path uses an unavailable pen: " + document.toolNameHint(pen)
                    + ". Reassign it to continue.";
        }
        for (auto& move : moves) {
            if (move.type == GcodeMove::Type::Linear
                && qFuzzyCompare(move.start.x(), move.end.x())
                && qFuzzyCompare(move.start.y(), move.end.y())
                && !qFuzzyCompare(move.start.z(), move.end.z()))
                move.feedRate = options.plungeFeed;
            bool finite = true;
            for (double value : {double(move.end.x()), double(move.end.y()), double(move.end.z()),
                                 move.iOffset, move.jOffset, move.radius, move.feedRate})
                finite = finite && firmwareNumber(value);
            if (!finite) { result.errors << "A path contains invalid motion coordinates."; break; }
            include(move.end.x(), move.end.y(), move.end.z());
            if (move.type == GcodeMove::Type::ArcCW || move.type == GcodeMove::Type::ArcCCW) {
                // A conservative full-circle envelope also covers partial arcs.
                const double radius = move.radius != 0 ? std::abs(move.radius) * 2
                    : std::hypot(move.iOffset, move.jOffset);
                const double x = move.start.x() + (move.radius != 0 ? 0 : move.iOffset);
                const double y = move.start.y() + (move.radius != 0 ? 0 : move.jOffset);
                include(x - radius - 0.001, y - radius - 0.001, move.end.z());
                include(x + radius + 0.001, y + radius + 0.001, move.end.z());
            }
            drawing.append({GcodeParser::movesToGcode({move}).front(), -1, entity->id(), pen,
                            PlotJobBlock::Kind::Drawing, PlotJobBlock::Frame::Work});
        }
    }
    if (pens.size() > 1 && !options.automaticRack)
        result.errors << "This design needs pen changes. Enable automatic rack exchanges or use one pen.";
    if (drawing.isEmpty()) result.errors << "The design has no drawable paths.";
    result.errors.removeDuplicates();
    if (!result.valid()) return result;

    if (options.automaticRack) {
        if (!tools || pens.contains(QUuid{})) result.errors << "Assign a defined pen to every automatic path, including the default pen.";
        if (!options.initialHolderKnown) result.errors << "Choose the initial holder: empty or a defined loaded pen.";
        const double emittedSafeZ = QString::number(static_cast<float>(options.safeZ), 'f', 4).toDouble();
        const double emittedDrawingZ = QString::number(static_cast<float>(options.drawingZ), 'f', 4).toDouble();
        if (emittedSafeZ <= emittedDrawingZ)
            result.errors << "Automatic rack jobs require pen-up Z above drawing Z.";
        if (!result.valid()) return result;

        auto& job = result.job;
        const auto& rack = tools->rackSettings();
        // Validate even when the first pen is already loaded and kept at the end.
        result.errors << ToolChangeMacroCompiler::compile(*tools,
            {{}, drawing.first().toolId, options.workCoordinateSystem, options.drawingFeed}).errors;
        if (!options.initialToolId.isNull())
            result.errors << ToolChangeMacroCompiler::compile(*tools,
                {options.initialToolId, {}, options.workCoordinateSystem, options.drawingFeed}).errors;
        if (!result.valid()) return result;
        job.automaticRack = true;
        job.initialHolderKnown = true;
        job.initialToolId = options.initialToolId;
        job.returnFinalPen = options.returnFinalPen;
        job.workCoordinateSystem = options.workCoordinateSystem;
        job.safeZ = options.safeZ;
        job.workMinimum = minimum;
        job.workMaximum = maximum;
        job.configurationFingerprint = tools->configurationFingerprint();
        const double distance = std::hypot(std::hypot(rack.maxX - rack.minX, rack.maxY - rack.minY), rack.maxZ - rack.minZ);
        const double feed = std::min({rack.dockFeed, rack.travelFeed, options.plungeFeed, options.drawingFeed});
        // Allow the minimum GRBL feed override (10%), 48 buffered drawing moves,
        // conservative arc circumference, maximum template dwell and controller overhead.
        const double timeoutMs = (60 * distance * 7 * 48 / (feed * 0.1)
                                  + ToolChangeMacroCompiler::maxDwellSeconds + 30) * 1000;
        if (!std::isfinite(timeoutMs) || timeoutMs <= 0 || timeoutMs > std::numeric_limits<int>::max())
            result.errors << "Rack bounds/feeds produce an unsupported synchronization timeout.";
        else job.phaseTimeoutMs = int(std::ceil(timeoutMs));
        if (!result.valid()) return result;
        auto appendManaged = [&](const QString& text, PlotJobBlock::Kind kind, PlotJobBlock::Frame frame,
                                 const QString& phase, const QUuid& tool = QUuid{},
                                 PlotJobBlock::HolderEffect effect = PlotJobBlock::HolderEffect::None) {
            job.blocks.append({text, int(job.blocks.size()), {}, tool, kind, frame, phase, effect});
            static const QRegularExpression feedWord("F([0-9]+(?:\\.[0-9]+)?)");
            static const QRegularExpression dwellWord("P([0-9]+(?:\\.[0-9]+)?)");
            const auto feedMatch = feedWord.match(text);
            const auto dwellMatch = dwellWord.match(text);
            const double phaseFeed = feedMatch.hasMatch() ? feedMatch.captured(1).toDouble() : feed;
            const double dwell = text.startsWith("G4") && dwellMatch.hasMatch() ? dwellMatch.captured(1).toDouble() : 0;
            const double deadline = (60 * distance / (phaseFeed * 0.1) + dwell + 30) * 1000;
            job.blocks.last().timeoutMs = int(std::min(deadline, double(std::numeric_limits<int>::max())));
            if (phase.startsWith("Draw with ") || phase == "Finish drawing") job.blocks.last().timeoutMs = job.phaseTimeoutMs;
        };
        appendManaged("; GRBL_CPP_MANAGED_RACK: execute the complete typed job", PlotJobBlock::Kind::Setup,
                      PlotJobBlock::Frame::Unspecified, "Prepare automatic plotting");
        appendManaged(QString("G21 G90 G17 G94 G91.1 G%1 M5 M9").arg(options.workCoordinateSystem),
                      PlotJobBlock::Kind::Setup, PlotJobBlock::Frame::Work, "Prepare automatic plotting");
        appendManaged(QString("G53 G1 Z%1 F%2").arg(rack.clearanceZ, 0, 'f', 3).arg(rack.dockFeed, 0, 'f', 3),
                      PlotJobBlock::Kind::Rack, PlotJobBlock::Frame::Machine, "Raise to rack clearance");
        appendManaged("G4 P0.01", PlotJobBlock::Kind::Synchronization, PlotJobBlock::Frame::Unspecified, "Raise to rack clearance");
        QUuid current = options.initialToolId;
        auto exchange = [&](const QUuid& next) {
            const auto preview = ToolChangeMacroCompiler::compile(*tools, {current, next, options.workCoordinateSystem, options.drawingFeed});
            result.errors << preview.errors;
            for (const auto& phase : preview.phases) {
                for (int i = 0; i < phase.commands.size(); ++i) {
                    const auto& command = phase.commands[i];
                    const bool checkpoint = command.startsWith("G4");
                    auto effect = PlotJobBlock::HolderEffect::None;
                    if (i == phase.commands.size() - 1) {
                        if (phase.holderEffect == ToolChangePhase::HolderEffect::Empty) effect = PlotJobBlock::HolderEffect::Empty;
                        if (phase.holderEffect == ToolChangePhase::HolderEffect::Loaded) effect = PlotJobBlock::HolderEffect::Loaded;
                    }
                    appendManaged(command, checkpoint ? PlotJobBlock::Kind::Synchronization : PlotJobBlock::Kind::Rack,
                                  command.contains("G53") ? PlotJobBlock::Frame::Machine : PlotJobBlock::Frame::Unspecified,
                                  phase.title, phase.toolId, effect);
                }
            }
            current = next;
        };
        QUuid previousPath;
        for (auto block : drawing) {
            if (block.pathId != previousPath) {
                const bool changed = job.penSequence.isEmpty() || job.penSequence.last() != block.toolId;
                if (changed) {
                    job.penSequence << block.toolId;
                    if (current != block.toolId) exchange(block.toolId);
                    // Traverse XY while still at machine clearance. The path's first
                    // rapid then lowers at the drawing entry, never over the rack.
                    const auto entry = GcodeParser::parseLine(block.text, 0, {}, options.drawingFeed, 0, true, false).end;
                    if (!block.text.startsWith("G0") || std::abs(entry.z() - options.safeZ) > 0.0001)
                        result.errors << "A path does not provide a safe raised entry move.";
                    appendManaged(QString("G1 X%1 Y%2 F%3").arg(entry.x(), 0, 'f', 4).arg(entry.y(), 0, 'f', 4)
                                  .arg(rack.travelFeed, 0, 'f', 3), PlotJobBlock::Kind::Rack, PlotJobBlock::Frame::Work,
                                  "Travel raised to drawing entry", block.toolId);
                    appendManaged("G4 P0.01", PlotJobBlock::Kind::Synchronization, PlotJobBlock::Frame::Unspecified,
                                  "Travel raised to drawing entry", block.toolId);
                }
                previousPath = block.pathId;
            }
            block.sourceLine = int(job.blocks.size());
            block.phase = "Draw with " + tools->tool(block.toolId)->name;
            job.blocks << block;
            // Every complete path ends raised; a following rack block drains and
            // checkpoints drawing before travel. The explicit dwell below does this.
            if (block.text.startsWith("G0") && block.text.contains(QString("Z%1").arg(options.safeZ, 0, 'f', 4))) {
                // Entry rapids may also match; extra checkpoints are harmless and
                // keep arbitrary entity move lists physically synchronized.
                appendManaged("G4 P0.01", PlotJobBlock::Kind::Synchronization, PlotJobBlock::Frame::Unspecified,
                              block.phase, block.toolId);
            }
        }
        appendManaged("G4 P0.01", PlotJobBlock::Kind::Synchronization, PlotJobBlock::Frame::Unspecified, "Finish drawing", current);
        if (options.returnFinalPen) exchange({});
        else {
            appendManaged(QString("G53 G1 Z%1 F%2").arg(rack.clearanceZ, 0, 'f', 3).arg(rack.dockFeed, 0, 'f', 3),
                          PlotJobBlock::Kind::Finish, PlotJobBlock::Frame::Machine, "Keep final pen raised", current);
            appendManaged("G4 P0.01", PlotJobBlock::Kind::Synchronization, PlotJobBlock::Frame::Unspecified, "Keep final pen raised", current);
        }
        if (!result.valid()) job.blocks.clear();
        else job.executionSeal = job.executionDigest();
        return result;
    }

    auto append = [&](const QString& text, PlotJobBlock::Kind kind) {
        result.job.blocks.append({text, int(result.job.blocks.size()), {}, {}, kind, PlotJobBlock::Frame::Work});
    };
    append(QString("G21 G90 G17 G94 G91.1 G%1").arg(options.workCoordinateSystem), PlotJobBlock::Kind::Setup);
    append(QString("G0 Z%1").arg(options.safeZ, 0, 'f', 4), PlotJobBlock::Kind::Setup);
    for (auto& block : drawing) {
        block.sourceLine = int(result.job.blocks.size());
        result.job.blocks.append(std::move(block));
    }
    append(QString("G0 Z%1").arg(options.safeZ, 0, 'f', 4), PlotJobBlock::Kind::Finish);
    append("G0 X0 Y0", PlotJobBlock::Kind::Finish);
    if (pens.size() == 1 && !pens.begin()->isNull())
        result.warnings << "Load the assigned pen manually before plotting.";
    return result;
}

// Compatibility entry point. All plotting policy lives in PlotEngine's builder.
QStringList Document::generateGcode(double feedRateXY, double plungeRateZ,
                                    double safeZ, double cutZ,
                                    const ToolLibrary* tools) const
{
    const auto result = PlotJobBuilder::build(*this, {feedRateXY, plungeRateZ, safeZ, cutZ, 54}, tools);
    return result.valid() ? result.job.lines() : QStringList{};
}
