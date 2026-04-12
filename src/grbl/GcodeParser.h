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

class GcodeParser : public QObject
{
    Q_OBJECT

public:
    explicit GcodeParser(QObject *parent = nullptr);

    QList<GcodeMove> parse(const QStringList &lines);
    GcodeMove parseLine(const QString &line, int lineNumber);

    static QStringList cleanLines(const QStringList &lines);
    static QString stripComments(const QString &line);

    // Transform helpers
    static QStringList movesToGcode(const QList<GcodeMove> &moves);
    static QList<GcodeMove> transformMoves(const QList<GcodeMove> &moves,
                                           double scaleX, double scaleY,
                                           double offsetX, double offsetY,
                                           double rotateDeg);

signals:
    void parseError(int line, const QString &error);

private:
    QHash<QChar, double> m_modalCodes;
    QVector3D m_currentPos;
    double m_feedRate = 0.0;
    double m_spindleSpeed = 0.0;
    bool m_absolute = true;
    bool m_arcAbsolute = false;

    static const QRegularExpression s_regex;
};