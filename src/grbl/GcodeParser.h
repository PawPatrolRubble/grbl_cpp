#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector3D>
#include <QHash>
#include <QRegularExpression>

struct GcodeMove
{
    enum class Type { Rapid, Linear, ArcCW, ArcCCW };
    Type type = Type::Rapid;
    QVector3D start;
    QVector3D end;
    double iOffset = 0.0;   // Arc center offset from start (X axis)
    double jOffset = 0.0;   // Arc center offset from start (Y axis)
    double radius = 0.0;    // Arc radius (if R form used)
    double feedRate = 0.0;
    double spindleSpeed = 0.0;
    int lineNumber = 0;
};

/// G-code file parser for visualization.
/// Thread-safe: call parse() multiple times; each call starts from a clean state.
class GcodeParser : public QObject
{
    Q_OBJECT

public:
    explicit GcodeParser(QObject *parent = nullptr);

    /// Parse a list of G-code lines. Thread-safe and reentrant.
    QList<GcodeMove> parse(const QStringList &lines);

    /// Static utility: parse a single line (stateless).
    static GcodeMove parseLine(const QString &line, int lineNumber,
                               const QVector3D &currentPos,
                               double feedRate,
                               double spindleSpeed,
                               bool absolute,
                               bool arcAbsolute);

    /// Remove comments and blank lines.
    static QStringList cleanLines(const QStringList &lines);

    /// Strip comments from a single line.
    static QString stripComments(const QString &line);

    /// Serialize moves back to G-code.
    static QStringList movesToGcode(const QList<GcodeMove> &moves);

    /// Apply geometric transforms to a list of moves.
    static QList<GcodeMove> transformMoves(const QList<GcodeMove> &moves,
                                           double scaleX, double scaleY,
                                           double offsetX, double offsetY,
                                           double rotateDeg);

signals:
    void parseError(int line, const QString &error);

private:
    QRegularExpression m_regex;
};
