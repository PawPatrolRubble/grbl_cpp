#include "GcodeParser.h"
#include <QRegularExpression>

const QRegularExpression GcodeParser::s_regex("([A-Za-z])([+-]?[0-9]*\\.?[0-9]+)");

GcodeParser::GcodeParser(QObject *parent)
    : QObject(parent)
{
}

QList<GcodeMove> GcodeParser::parse(const QStringList &lines)
{
    QList<GcodeMove> moves;
    m_currentPos = QVector3D();
    m_feedRate = 0.0;
    m_spindleSpeed = 0.0;
    m_absolute = true;
    m_modalCodes.clear();

    QStringList cleaned = cleanLines(lines);

    for (int i = 0; i < cleaned.size(); ++i) {
        GcodeMove move = parseLine(cleaned[i], i);
        if (move.type == GcodeMove::Type::Rapid || move.type == GcodeMove::Type::Linear
            || move.type == GcodeMove::Type::ArcCW || move.type == GcodeMove::Type::ArcCCW) {
            moves.append(move);
        }
    }

    return moves;
}

GcodeMove GcodeParser::parseLine(const QString &line, int lineNumber)
{
    GcodeMove move;
    move.lineNumber = lineNumber;
    move.start = m_currentPos;

    QString stripped = stripComments(line).trimmed();
    if (stripped.isEmpty())
        return move;

    QVector3D target = m_currentPos;
    bool hasMotion = false;
    GcodeMove::Type motionType = GcodeMove::Type::Linear;
    double iVal = 0.0, jVal = 0.0, rVal = 0.0;

    QRegularExpressionMatchIterator it = s_regex.globalMatch(stripped);

    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        QChar letter = match.captured(1).at(0).toUpper();
        double value = match.captured(2).toDouble();

        switch (letter.unicode()) {
        case 'G': {
            int code = static_cast<int>(value);
            switch (code) {
            case 0: motionType = GcodeMove::Type::Rapid; hasMotion = true; break;
            case 1: motionType = GcodeMove::Type::Linear; hasMotion = true; break;
            case 2: motionType = GcodeMove::Type::ArcCW; hasMotion = true; break;
            case 3: motionType = GcodeMove::Type::ArcCCW; hasMotion = true; break;
            case 90: m_absolute = true; break;
            case 91: m_absolute = false; break;
            default: break;
            }
            break;
        }
        case 'F': m_feedRate = value; break;
        case 'S': m_spindleSpeed = value; break;
        case 'X':
            target.setX(m_absolute ? value : m_currentPos.x() + value);
            hasMotion = true;
            break;
        case 'Y':
            target.setY(m_absolute ? value : m_currentPos.y() + value);
            hasMotion = true;
            break;
        case 'Z':
            target.setZ(m_absolute ? value : m_currentPos.z() + value);
            hasMotion = true;
            break;
        case 'I': iVal = value; break;
        case 'J': jVal = value; break;
        case 'R': rVal = value; break;
        default: break;
        }
    }

    if (hasMotion) {
        move.type = motionType;
        move.end = target;
        move.feedRate = m_feedRate;
        move.spindleSpeed = m_spindleSpeed;
        move.iOffset = iVal;
        move.jOffset = jVal;
        move.radius = rVal;
        m_currentPos = target;
    }

    return move;
}

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

            // Rotate I/J offsets (they are relative vectors, rotate around 0,0)
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