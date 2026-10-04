#include "PlotJobTransform.h"
#include <QRegularExpression>
#include <QSet>
#include <cmath>

namespace {
const QRegularExpression words("([A-Za-z])([+-]?(?:[0-9]+(?:\\.[0-9]*)?|\\.[0-9]+))");
bool equal(double a, double b) { return std::abs(a - b) < 0.00001; }
bool motionCode(double value) { return equal(value, 0) || equal(value, 1) || equal(value, 2) || equal(value, 3); }
QString comments(const QString& line)
{
    QStringList result;
    bool inComment = false;
    int start = -1;
    for (int i = 0; i < line.size(); ++i) {
        if (line[i] == '(' && !inComment) { inComment = true; start = i; }
        else if (line[i] == ')' && inComment) { result << line.mid(start, i - start + 1); inComment = false; }
        else if (line[i] == ';' && !inComment) { result << line.mid(i); break; }
    }
    return result.join(' ');
}
}

QString PlotJobTransform::unsupportedReason(const PlotJob& job)
{
    bool metric = false, absolute = false, motionSeen = false;
    int coordinateSystem = -1;
    for (const auto& block : job.blocks) {
        if (block.kind == PlotJobBlock::Kind::Rack || block.frame == PlotJobBlock::Frame::Machine)
            return "Rack and machine-coordinate programs cannot be transformed as drawing paths.";
        const QString command = GcodeParser::stripComments(block.text).trimmed();
        if (command.isEmpty()) continue;
        QString unmatched = command;
        unmatched.remove(words);
        if (!unmatched.trimmed().isEmpty()) return "This program contains unsupported command syntax.";
        bool hasCoordinates = false, dwell = false;
        auto matches = words.globalMatch(command);
        while (matches.hasNext()) {
            const auto match = matches.next();
            const auto letter = match.captured(1).toUpper();
            const double value = match.captured(2).toDouble();
            if (letter == "G") {
                if (motionCode(value)) continue;
                if (equal(value, 21)) metric = true;
                else if (equal(value, 90)) absolute = true;
                else if (equal(value, 4)) dwell = true;
                else if (equal(value, 17) || equal(value, 94) || equal(value, 91.1)) {}
                else if (value >= 54 && value <= 59 && equal(value, std::round(value))) {
                    const int frame = int(value);
                    if (motionSeen && coordinateSystem != frame)
                        return "Programs that switch work coordinate systems cannot be transformed.";
                    coordinateSystem = frame;
                } else return "Transforms require metric, absolute XY drawing moves; this program uses an unsupported G-code mode.";
            } else if (letter == "X" || letter == "Y" || letter == "Z") hasCoordinates = true;
            else if (letter != "I" && letter != "J" && letter != "R" && letter != "F"
                     && letter != "S" && letter != "M" && letter != "N" && letter != "P"
                     && letter != "T") return "This program contains unsupported coordinate or auxiliary words.";
        }
        if (hasCoordinates) {
            if (dwell || !metric || !absolute)
                return "Set G21 and G90 before drawing moves to enable transforms.";
            motionSeen = true;
        }
    }
    return {};
}

PlotJobTransformResult PlotJobTransform::apply(const PlotJob& job, const QList<GcodeMove>& transformed)
{
    PlotJobTransformResult result{job, unsupportedReason(job)};
    if (!result.valid()) return result;
    GcodeParser parser;
    const auto original = parser.parse(job.lines());
    if (transformed.size() != original.size()) {
        result.error = "The transformed geometry no longer matches this program.";
        return result;
    }
    for (int i = 0; i < original.size(); ++i) {
        const auto& before = original[i];
        const auto& after = transformed[i];
        if (after.lineNumber != before.lineNumber || after.type != before.type
            || before.lineNumber < 0 || before.lineNumber >= job.blocks.size()) {
            result.error = "The transform changed program identity or motion type.";
            return result;
        }
        for (double value : {double(after.end.x()), double(after.end.y()), double(after.end.z()),
                             after.iOffset, after.jOffset, after.radius}) {
            if (!std::isfinite(value)) { result.error = "The transform produced invalid coordinates."; return result; }
        }
        if (after.type == GcodeMove::Type::ArcCW || after.type == GcodeMove::Type::ArcCCW) {
            if (after.radius == 0) {
                const double radius = std::hypot(after.iOffset, after.jOffset);
                const double endRadius = std::hypot(after.end.x() - after.start.x() - after.iOffset,
                                                   after.end.y() - after.start.y() - after.jOffset);
                if (std::abs(radius - endRadius) > std::max(0.002, radius * 0.0001)) {
                    result.error = "The transform would turn a circular arc into an unsupported curve.";
                    return result;
                }
            }
        }
        auto& block = result.job.blocks[before.lineNumber];
        // Finish/setup blocks of generated jobs are not part of the editable drawing.
        if (block.kind == PlotJobBlock::Kind::Setup || block.kind == PlotJobBlock::Kind::Finish) continue;
        QStringList preservedWords;
        auto matches = words.globalMatch(GcodeParser::stripComments(block.text));
        while (matches.hasNext()) {
            const auto match = matches.next();
            const QString letter = match.captured(1).toUpper();
            if (letter == "X" || letter == "Y" || letter == "Z" || letter == "I"
                || letter == "J" || letter == "R") continue;
            if (letter == "G" && motionCode(match.captured(2).toDouble())) continue;
            preservedWords << match.captured(0);
        }
        auto serialized = after;
        serialized.feedRate = 0; // Keep the original F word and modal feed semantics.
        const QString annotation = comments(block.text);
        QStringList parts = preservedWords;
        parts << GcodeParser::movesToGcode({serialized}).front();
        if (!annotation.isEmpty()) parts << annotation;
        block.text = parts.join(' ');
    }
    return result;
}
