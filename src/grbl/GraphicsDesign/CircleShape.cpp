#include "CircleShape.h"
#include <cmath>

CircleShape::CircleShape(const QPointF &center, double radius)
    : Shape(Type::Circle)
    , m_center(center)
    , m_radius(radius)
{
}

void CircleShape::setCenter(const QPointF &c) { m_center = c; }
void CircleShape::setRadius(double r) { m_radius = r; }

QRectF CircleShape::boundingRect() const
{
    return QRectF(m_center.x() - m_radius, m_center.y() - m_radius,
                  2 * m_radius, 2 * m_radius);
}

void CircleShape::draw(QPainterPath &path) const
{
    path.addEllipse(m_center, m_radius, m_radius);
}

QList<QPointF> CircleShape::handlePositions() const
{
    // Center + radius handle on right
    return {
        m_center,
        QPointF(m_center.x() + m_radius, m_center.y())
    };
}

QList<GcodeMove> CircleShape::toGcodeMoves(double zSafe, double zCut, double feedRate) const
{
    QList<GcodeMove> moves;

    // Start point at right side of circle (center + radius on X axis)
    double startX = m_center.x() + m_radius;
    double startY = m_center.y();

    // G0 rapid to start (pen up)
    GcodeMove rapid1;
    rapid1.type = GcodeMove::Type::Rapid;
    rapid1.start = QVector3D(0, 0, zSafe);
    rapid1.end = QVector3D(startX, startY, zSafe);
    moves.append(rapid1);

    // G1 plunge
    GcodeMove plunge;
    plunge.type = GcodeMove::Type::Linear;
    plunge.start = QVector3D(startX, startY, zSafe);
    plunge.end = QVector3D(startX, startY, zCut);
    plunge.feedRate = feedRate;
    moves.append(plunge);

    // G2 full circle (CW) - use I/J offset from start to center
    GcodeMove circle;
    circle.type = GcodeMove::Type::ArcCW;
    circle.start = QVector3D(startX, startY, zCut);
    circle.end = QVector3D(startX, startY, zCut);  // Full circle ends at start
    circle.iOffset = -m_radius;  // Center is at start.x - radius
    circle.jOffset = 0;
    circle.feedRate = feedRate;
    moves.append(circle);

    // G0 raise
    GcodeMove rapid2;
    rapid2.type = GcodeMove::Type::Rapid;
    rapid2.start = QVector3D(startX, startY, zCut);
    rapid2.end = QVector3D(startX, startY, zSafe);
    moves.append(rapid2);

    return moves;
}

QJsonObject CircleShape::toJson() const
{
    QJsonObject json;
    json["type"] = "circle";
    json["cx"] = m_center.x();
    json["cy"] = m_center.y();
    json["radius"] = m_radius;
    json["rotation"] = m_rotation;
    return json;
}

CircleShape* CircleShape::fromJson(const QJsonObject &json)
{
    QPointF center(json["cx"].toDouble(), json["cy"].toDouble());
    double r = json["radius"].toDouble(10);
    CircleShape *shape = new CircleShape(center, r);
    shape->setRotation(json["rotation"].toDouble(0));
    return shape;
}

void CircleShape::translate(double dx, double dy)
{
    m_center += QPointF(dx, dy);
}

void CircleShape::scale(double sx, double sy, const QPointF &pivot)
{
    m_center = QPointF(pivot.x() + (m_center.x() - pivot.x()) * sx,
                       pivot.y() + (m_center.y() - pivot.y()) * sy);
    m_radius *= std::max(sx, sy);
}