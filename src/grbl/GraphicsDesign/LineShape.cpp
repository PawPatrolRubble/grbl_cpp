#include "LineShape.h"
#include <cmath>

LineShape::LineShape(const QPointF &p1, const QPointF &p2)
    : Shape(Type::Line)
    , m_p1(p1)
    , m_p2(p2)
{
}

void LineShape::setStartPoint(const QPointF &p) { m_p1 = p; }
void LineShape::setEndPoint(const QPointF &p) { m_p2 = p; }

QRectF LineShape::boundingRect() const
{
    double minX = std::min(m_p1.x(), m_p2.x());
    double maxX = std::max(m_p1.x(), m_p2.x());
    double minY = std::min(m_p1.y(), m_p2.y());
    double maxY = std::max(m_p1.y(), m_p2.y());
    return QRectF(QPointF(minX, minY), QPointF(maxX, maxY));
}

void LineShape::draw(QPainterPath &path) const
{
    path.moveTo(m_p1);
    path.lineTo(m_p2);
}

QList<QPointF> LineShape::handlePositions() const
{
    return {m_p1, m_p2};
}

QList<GcodeMove> LineShape::toGcodeMoves(double zSafe, double zCut, double feedRate) const
{
    QList<GcodeMove> moves;

    // G0 rapid to start point (pen up)
    GcodeMove rapid1;
    rapid1.type = GcodeMove::Type::Rapid;
    rapid1.start = QVector3D(0, 0, zSafe);
    rapid1.end = QVector3D(m_p1.x(), m_p1.y(), zSafe);
    moves.append(rapid1);

    // G1 plunge (pen down)
    GcodeMove plunge;
    plunge.type = GcodeMove::Type::Linear;
    plunge.start = QVector3D(m_p1.x(), m_p1.y(), zSafe);
    plunge.end = QVector3D(m_p1.x(), m_p1.y(), zCut);
    plunge.feedRate = feedRate;
    moves.append(plunge);

    // G1 draw line
    GcodeMove draw;
    draw.type = GcodeMove::Type::Linear;
    draw.start = QVector3D(m_p1.x(), m_p1.y(), zCut);
    draw.end = QVector3D(m_p2.x(), m_p2.y(), zCut);
    draw.feedRate = feedRate;
    moves.append(draw);

    // G0 raise (pen up)
    GcodeMove rapid2;
    rapid2.type = GcodeMove::Type::Rapid;
    rapid2.start = QVector3D(m_p2.x(), m_p2.y(), zCut);
    rapid2.end = QVector3D(m_p2.x(), m_p2.y(), zSafe);
    moves.append(rapid2);

    return moves;
}

QJsonObject LineShape::toJson() const
{
    QJsonObject json;
    json["type"] = "line";
    json["x1"] = m_p1.x();
    json["y1"] = m_p1.y();
    json["x2"] = m_p2.x();
    json["y2"] = m_p2.y();
    json["rotation"] = m_rotation;
    return json;
}

LineShape* LineShape::fromJson(const QJsonObject &json)
{
    QPointF p1(json["x1"].toDouble(), json["y1"].toDouble());
    QPointF p2(json["x2"].toDouble(), json["y2"].toDouble());
    LineShape *shape = new LineShape(p1, p2);
    shape->setRotation(json["rotation"].toDouble(0));
    return shape;
}

void LineShape::translate(double dx, double dy)
{
    m_p1 += QPointF(dx, dy);
    m_p2 += QPointF(dx, dy);
}

void LineShape::scale(double sx, double sy, const QPointF &pivot)
{
    m_p1 = QPointF(pivot.x() + (m_p1.x() - pivot.x()) * sx,
                   pivot.y() + (m_p1.y() - pivot.y()) * sy);
    m_p2 = QPointF(pivot.x() + (m_p2.x() - pivot.x()) * sx,
                   pivot.y() + (m_p2.y() - pivot.y()) * sy);
}