#include "GcodeParser.h"
#include <QRegularExpression>

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

    QRegularExpression regex("([A-Za-z])([+-]?[0-9]*\\.?[0-9]+)");
    QRegularExpressionMatchIterator it = regex.globalMatch(stripped);

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
        default: break;
        }
    }

    if (hasMotion) {
        move.type = motionType;
        move.end = target;
        move.feedRate = m_feedRate;
        move.spindleSpeed = m_spindleSpeed;
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
    return lines;
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