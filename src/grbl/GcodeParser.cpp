#include "GcodeParser.h"
#include <QRegularExpression>
#include <cmath>

const QRegularExpression GcodeParser::m_regex("([A-Za-z])([+-]?[0-9]*\\.?[0-9]+)");

GcodeParser::GcodeParser(QObject *parent)
    : QObject(parent)
{
}

// ---------------------------------------------------------------------------
// Main parser — fully stateless, thread-safe, reentrant
// ---------------------------------------------------------------------------

QList<GcodeMove> GcodeParser::parse(const QStringList &lines)
{
    QList<GcodeMove> moves;

    // Local state — NOT stored in member variables
    QVector3D currentPos;
    double feedRate = 0.0;
    double spindleSpeed = 0.0;
    bool absolute = true;

    QStringList cleaned = cleanLines(lines);

    for (int i = 0; i < cleaned.size(); ++i) {
        GcodeMove move;
        move.lineNumber = i;
        move.start = currentPos;

        QString stripped = stripComments(cleaned[i]).trimmed();
        if (stripped.isEmpty())
            continue;

        QVector3D target = currentPos;
        bool hasMotion = false;
        GcodeMove::Type motionType = GcodeMove::Type::Linear;
        double iVal = 0.0, jVal = 0.0, rVal = 0.0;

        QRegularExpressionMatchIterator it = m_regex.globalMatch(stripped);

        while (it.hasNext()) {
            QRegularExpressionMatch match = it.next();
            QChar letter = match.captured(1).at(0).toUpper();
            double value = match.captured(2).toDouble();

            switch (letter.unicode()) {
            case 'G': {
                int code = static_cast<int>(value);
                switch (code) {
                case 0:  motionType = GcodeMove::Type::Rapid;   hasMotion = true; break;
                case 1:  motionType = GcodeMove::Type::Linear;   hasMotion = true; break;
                case 2:  motionType = GcodeMove::Type::ArcCW;    hasMotion = true; break;
                case 3:  motionType = GcodeMove::Type::ArcCCW;   hasMotion = true; break;
                case 90: absolute = true; break;
                case 91: absolute = false; break;
                default: break;
                }
                break;
            }
            case 'F': feedRate = value; break;
            case 'S': spindleSpeed = value; break;
            case 'X': target.setX(absolute ? value : currentPos.x() + value); hasMotion = true; break;
            case 'Y': target.setY(absolute ? value : currentPos.y() + value); hasMotion = true; break;
            case 'Z': target.setZ(absolute ? value : currentPos.z() + value); hasMotion = true; break;
            case 'I': iVal = value; break;
            case 'J': jVal = value; break;
            case 'R': rVal = value; break;
            default: break;
            }
        }

        if (hasMotion) {
            move.type = motionType;
            move.end = target;
            move.feedRate = feedRate;
            move.spindleSpeed = spindleSpeed;
            move.iOffset = iVal;
            move.jOffset = jVal;
            move.radius = rVal;
            currentPos = target;
            moves.append(move);
        }
    }

    return moves;
}

// ---------------------------------------------------------------------------
// Static single-line parser (for callers who want full control)
// ---------------------------------------------------------------------------

GcodeMove GcodeParser::parseLine(const QString &line, int lineNumber,
                                  const QVector3D &currentPos,
                                  double feedRate,
                                  double spindleSpeed,
                                  bool absolute,
                                  bool /*arcAbsolute*/)
{
    GcodeMove move;
    move.lineNumber = lineNumber;
    move.start = currentPos;
    move.feedRate = feedRate;
    move.spindleSpeed = spindleSpeed;

    QString stripped = stripComments(line).trimmed();
    if (stripped.isEmpty())
        return move;

    QVector3D target = currentPos;
    bool hasMotion = false;
    GcodeMove::Type motionType = GcodeMove::Type::Linear;

    QRegularExpressionMatchIterator it = m_regex.globalMatch(stripped);

    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        QChar letter = match.captured(1).at(0).toUpper();
        double value = match.captured(2).toDouble();

        switch (letter.unicode()) {
        case 'G': {
            int code = static_cast<int>(value);
            switch (code) {
            case 0: motionType = GcodeMove::Type::Rapid;  hasMotion = true; break;
            case 1: motionType = GcodeMove::Type::Linear; hasMotion = true; break;
            case 2: motionType = GcodeMove::Type::ArcCW;  hasMotion = true; break;
            case 3: motionType = GcodeMove::Type::ArcCCW; hasMotion = true; break;
            default: break;
            }
            break;
        }
        case 'F': move.feedRate = value; break;
        case 'S': move.spindleSpeed = value; break;
        case 'X': target.setX(absolute ? value : currentPos.x() + value); hasMotion = true; break;
        case 'Y': target.setY(absolute ? value : currentPos.y() + value); hasMotion = true; break;
        case 'Z': target.setZ(absolute ? value : currentPos.z() + value); hasMotion = true; break;
        default: break;
        }
    }

    if (hasMotion) {
        move.type = motionType;
        move.end = target;
    }

    return move;
}

// ---------------------------------------------------------------------------
// Static utilities
// ---------------------------------------------------------------------------

QStringList GcodeParser::cleanLines(const QStringList &lines)
{
    QStringList result;
    for (const QString &line : lines) {
        QString cleaned = stripComments(line).trimmed();
        if (!cleaned.isEmpty())
            result << line;
    }
    return result;
}

QString GcodeParser::stripComments(const QString &line)
{
    QString result;
    bool inComment = false;
    for (const QChar &ch : line) {
        if (ch == '(' || ch == ';') {
            inComment = true;
            continue;
        }
        if (ch == ')') {
            inComment = false;
            continue;
        }
        if (!inComment)
            result += ch;
    }
    return result;
}

// ---------------------------------------------------------------------------
// G-code regeneration from moves
// ---------------------------------------------------------------------------

QStringList GcodeParser::movesToGcode(const QList<GcodeMove> &moves)
{
    QStringList lines;
    for (const auto &mv : moves) {
        QString cmd;
        switch (mv.type) {
        case GcodeMove::Type::Rapid:  cmd = "G0"; break;
        case GcodeMove::Type::Linear: cmd = "G1"; break;
        case GcodeMove::Type::ArcCW:  cmd = "G2"; break;
        case GcodeMove::Type::ArcCCW: cmd = "G3"; break;
        }

        cmd += QString(" X%1 Y%2 Z%3")
                   .arg(mv.end.x(), 0, 'f', 4)
                   .arg(mv.end.y(), 0, 'f', 4)
                   .arg(mv.end.z(), 0, 'f', 4);

        if (mv.type == GcodeMove::Type::ArcCW || mv.type == GcodeMove::Type::ArcCCW) {
            cmd += QString(" I%1 J%2")
                       .arg(mv.iOffset, 0, 'f', 4)
                       .arg(mv.jOffset, 0, 'f', 4);
        }

        if (mv.feedRate > 0 && mv.type != GcodeMove::Type::Rapid) {
            cmd += QString(" F%1").arg(mv.feedRate, 0, 'f', 0);
        }

        lines.append(cmd);
    }
    return lines;
}

// ---------------------------------------------------------------------------
// Transform moves (scale, translate, rotate around bounding-box center)
// ---------------------------------------------------------------------------

static QPointF rotatePoint(const QPointF &pt, const QPointF &center, double radians)
{
    double dx = pt.x() - center.x();
    double dy = pt.y() - center.y();
    double cosA = std::cos(radians);
    double sinA = std::sin(radians);
    return QPointF(center.x() + dx * cosA - dy * sinA,
                   center.y() + dx * sinA + dy * cosA);
}

QList<GcodeMove> GcodeParser::transformMoves(const QList<GcodeMove> &moves,
                                              double scaleX, double scaleY,
                                              double offsetX, double offsetY,
                                              double rotateDeg)
{
    if (moves.isEmpty()) return moves;

    // Compute bounding-box center for scale and rotation pivot
    double minX = 1e9, maxX = -1e9, minY = 1e9, maxY = -1e9;
    for (const auto &mv : moves) {
        for (const auto &pt : {mv.start, mv.end}) {
            minX = std::min(minX, (double)pt.x());
            maxX = std::max(maxX, (double)pt.x());
            minY = std::min(minY, (double)pt.y());
            maxY = std::max(maxY, (double)pt.y());
        }
    }
    QPointF center((minX + maxX) / 2.0, (minY + maxY) / 2.0);
    double radians = rotateDeg * M_PI / 180.0;

    QList<GcodeMove> result;
    result.reserve(moves.size());

    for (const auto &mv : moves) {
        GcodeMove out = mv;

        // Scale around center
        auto scalePoint = [&](const QVector3D &p) -> QVector3D {
            double x = center.x() + (p.x() - center.x()) * scaleX;
            double y = center.y() + (p.y() - center.y()) * scaleY;
            return QVector3D(x, y, p.z());
        };

        out.start = scalePoint(mv.start);
        out.end   = scalePoint(mv.end);
        out.iOffset = mv.iOffset * scaleX;
        out.jOffset = mv.jOffset * scaleY;

        // Rotate around center
        if (std::abs(rotateDeg) > 0.001) {
            QPointF rs = rotatePoint(QPointF(out.start.x(), out.start.y()), center, radians);
            QPointF re = rotatePoint(QPointF(out.end.x(), out.end.y()), center, radians);
            out.start = QVector3D(rs.x(), rs.y(), out.start.z());
            out.end   = QVector3D(re.x(), re.y(), out.end.z());

            // Rotate I/J offsets (they are relative vectors)
            double ci = out.iOffset;
            double cj = out.jOffset;
            double cosA = std::cos(radians);
            double sinA = std::sin(radians);
            out.iOffset = ci * cosA - cj * sinA;
            out.jOffset = ci * sinA + cj * cosA;
        }

        // Translate
        out.start += QVector3D(offsetX, offsetY, 0);
        out.end   += QVector3D(offsetX, offsetY, 0);

        result.append(out);
    }

    return result;
}
