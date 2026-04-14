#include "CircleEntity.h"

#include <QLineF>
#include <algorithm>
#include <cmath>

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kHandleOffset = 18.0;
}

CircleEntity::CircleEntity(const QPointF& center, double radius)
    : Entity(Kind::Circle)
    , m_center(center)
    , m_radius(std::max(radius, 1.0))
{
}

Entity::Kind CircleEntity::kind() const
{
    return Kind::Circle;
}

QRectF CircleEntity::boundingRect() const
{
    return QRectF(m_center.x() - m_radius, m_center.y() - m_radius,
                  m_radius * 2.0, m_radius * 2.0);
}

QPointF CircleEntity::center() const
{
    return m_center;
}

void CircleEntity::draw(QPainter& painter) const
{
    painter.drawEllipse(m_center, m_radius, m_radius);
}

double CircleEntity::distanceTo(const QPointF& point) const
{
    // Distance to circle edge (not to center)
    return std::abs(QLineF(m_center, point).length() - m_radius);
}

void CircleEntity::translate(const QPointF& delta)
{
    m_center += delta;
}

std::unique_ptr<Entity> CircleEntity::clone() const
{
    return std::make_unique<CircleEntity>(m_center, m_radius);
}

std::vector<Entity::EditHandle> CircleEntity::editHandles() const
{
    std::vector<EditHandle> handles;
    // Radius handle on right side
    handles.push_back({"radius", QPointF(m_center.x() + m_radius, m_center.y()),
                       HandleRole::Resize, 0.0, false, {}});
    // Outer radius handle with leader line for easier grabbing
    handles.push_back({"radius_outer",
                       QPointF(m_center.x() + m_radius + kHandleOffset, m_center.y()),
                       HandleRole::Resize, 0.0, true,
                       QPointF(m_center.x() + m_radius, m_center.y())});
    return handles;
}

std::unique_ptr<Entity> CircleEntity::withDraggedHandle(const QString& handleId, const QPointF& worldPos) const
{
    if (handleId == "radius" || handleId == "radius_outer") {
        double newRadius = std::max(1.0, QLineF(m_center, worldPos).length());
        return std::make_unique<CircleEntity>(m_center, newRadius);
    }
    return clone();
}

std::vector<QPointF> CircleEntity::snapPoints() const
{
    // Center + 4 quadrant points
    return {
        m_center,
        QPointF(m_center.x() + m_radius, m_center.y()),  // Right
        QPointF(m_center.x() - m_radius, m_center.y()),  // Left
        QPointF(m_center.x(), m_center.y() + m_radius),  // Top
        QPointF(m_center.x(), m_center.y() - m_radius)   // Bottom
    };
}

QList<GcodeMove> CircleEntity::toGcodeMoves(double zSafe, double zCut, double feedRate) const
{
    QList<GcodeMove> moves;

    // G0 rapid to right quadrant point (pen up)
    QPointF startPoint(m_center.x() + m_radius, m_center.y());
    GcodeMove rapid1;
    rapid1.type = GcodeMove::Type::Rapid;
    rapid1.start = QVector3D(0, 0, zSafe);
    rapid1.end = QVector3D(startPoint.x(), startPoint.y(), zSafe);
    moves.append(rapid1);

    // G1 plunge (pen down)
    GcodeMove plunge;
    plunge.type = GcodeMove::Type::Linear;
    plunge.start = QVector3D(startPoint.x(), startPoint.y(), zSafe);
    plunge.end = QVector3D(startPoint.x(), startPoint.y(), zCut);
    plunge.feedRate = feedRate;
    moves.append(plunge);

    // G2 clockwise full circle arc
    // I, J are relative offsets from start point to center
    // Start is at (center.x + radius, center.y), so I = -radius, J = 0
    GcodeMove arc;
    arc.type = GcodeMove::Type::ArcCW;
    arc.start = QVector3D(startPoint.x(), startPoint.y(), zCut);
    arc.end = QVector3D(startPoint.x(), startPoint.y(), zCut);  // Full circle ends at start
    arc.iOffset = -m_radius;  // Center is at (-radius, 0) relative to start
    arc.jOffset = 0.0;
    arc.feedRate = feedRate;
    moves.append(arc);

    // G0 raise (pen up)
    GcodeMove rapid2;
    rapid2.type = GcodeMove::Type::Rapid;
    rapid2.start = QVector3D(startPoint.x(), startPoint.y(), zCut);
    rapid2.end = QVector3D(startPoint.x(), startPoint.y(), zSafe);
    moves.append(rapid2);

    return moves;
}

QJsonObject CircleEntity::toJson() const
{
    QJsonObject json;
    json["type"] = "circle";
    json["cx"] = m_center.x();
    json["cy"] = m_center.y();
    json["radius"] = m_radius;
    json["rotation"] = rotation();
    return json;
}

std::unique_ptr<CircleEntity> CircleEntity::fromJson(const QJsonObject& json)
{
    QPointF center(json["cx"].toDouble(), json["cy"].toDouble());
    double radius = json["radius"].toDouble(10);
    auto entity = std::make_unique<CircleEntity>(center, radius);
    entity->setRotation(json["rotation"].toDouble(0));
    return entity;
}

double CircleEntity::radius() const
{
    return m_radius;
}

void CircleEntity::setGeometry(const QPointF& center, double radius)
{
    m_center = center;
    m_radius = std::max(radius, 1.0);
}