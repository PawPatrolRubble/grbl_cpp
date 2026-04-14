#include "ArcEntity.h"

#include <QLineF>
#include <algorithm>
#include <cmath>

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kHandleOffset = 18.0;

QPointF pointOnCircle(const QPointF& center, double radius, double angleDegrees) {
    const double radians = angleDegrees * kPi / 180.0;
    return QPointF(center.x() + radius * std::cos(radians),
                   center.y() + radius * std::sin(radians));
}
}

ArcEntity::ArcEntity(const QPointF& center, double radius,
                     double startAngleDegrees, double sweepAngleDegrees)
    : Entity(Kind::Arc)
    , m_center(center)
    , m_radius(std::max(radius, 1.0))
    , m_startAngleDegrees(startAngleDegrees)
    , m_sweepAngleDegrees(sweepAngleDegrees)
{
}

Entity::Kind ArcEntity::kind() const
{
    return Kind::Arc;
}

QRectF ArcEntity::boundingRect() const
{
    // Simplified - use full circle bounds
    return QRectF(m_center.x() - m_radius, m_center.y() - m_radius,
                  2.0 * m_radius, 2.0 * m_radius);
}

QPointF ArcEntity::center() const
{
    return m_center;
}

void ArcEntity::draw(QPainter& painter) const
{
    QRectF arcRect(m_center.x() - m_radius, m_center.y() - m_radius,
                   2.0 * m_radius, 2.0 * m_radius);
    painter.drawArc(arcRect,
                    static_cast<int>(m_startAngleDegrees * 16),  // Qt uses 1/16th degree
                    static_cast<int>(m_sweepAngleDegrees * 16));
}

double ArcEntity::distanceTo(const QPointF& point) const
{
    // Distance to arc path
    const double distToCenter = QLineF(m_center, point).length();
    const double distToCircleEdge = std::abs(distToCenter - m_radius);

    // Check if point is within arc's angular range
    const double pointAngle = std::atan2(point.y() - m_center.y(),
                                          point.x() - m_center.x()) * 180.0 / kPi;
    // Normalize angle to [0, 360)
    double normalizedPointAngle = pointAngle;
    while (normalizedPointAngle < 0) normalizedPointAngle += 360.0;

    double normalizedStart = m_startAngleDegrees;
    while (normalizedStart < 0) normalizedStart += 360.0;

    double normalizedEnd = normalizedStart + m_sweepAngleDegrees;
    while (normalizedEnd < 0) normalizedEnd += 360.0;

    // Check if point angle is within arc sweep
    bool withinArc = false;
    if (m_sweepAngleDegrees >= 0) {
        withinArc = (normalizedPointAngle >= normalizedStart &&
                     normalizedPointAngle <= normalizedEnd);
    } else {
        withinArc = (normalizedPointAngle <= normalizedStart &&
                     normalizedPointAngle >= normalizedEnd);
    }

    if (withinArc) {
        return distToCircleEdge;
    }

    // Distance to endpoints
    const double distToStart = QLineF(point, startPoint()).length();
    const double distToEnd = QLineF(point, endPoint()).length();
    return std::min(distToStart, distToEnd);
}

void ArcEntity::translate(const QPointF& delta)
{
    m_center += delta;
}

std::unique_ptr<Entity> ArcEntity::clone() const
{
    return std::make_unique<ArcEntity>(m_center, m_radius,
                                        m_startAngleDegrees, m_sweepAngleDegrees);
}

std::vector<Entity::EditHandle> ArcEntity::editHandles() const
{
    std::vector<EditHandle> handles;
    const QPointF startPt = startPoint();
    const QPointF endPt = endPoint();

    handles.push_back({"start", startPt, HandleRole::Resize, m_startAngleDegrees, false, {}});
    handles.push_back({"end", endPt, HandleRole::Resize,
                       m_startAngleDegrees + m_sweepAngleDegrees, false, {}});
    handles.push_back({"center", m_center, HandleRole::Resize, 0.0, false, {}});

    // Radius handle (on perpendicular bisector of arc)
    const double midAngle = m_startAngleDegrees + m_sweepAngleDegrees * 0.5;
    const QPointF midPoint = pointOnCircle(m_center, m_radius, midAngle);
    handles.push_back({"radius", midPoint, HandleRole::Resize, midAngle, false, {}});
    handles.push_back({"radius_outer",
                       pointOnCircle(m_center, m_radius + kHandleOffset, midAngle),
                       HandleRole::Resize, midAngle, true, midPoint});

    return handles;
}

std::unique_ptr<Entity> ArcEntity::withDraggedHandle(const QString& handleId, const QPointF& worldPos) const
{
    if (handleId == "center") {
        return std::make_unique<ArcEntity>(worldPos, m_radius,
                                            m_startAngleDegrees, m_sweepAngleDegrees);
    }

    if (handleId == "radius" || handleId == "radius_outer") {
        double newRadius = std::max(1.0, QLineF(m_center, worldPos).length());
        return std::make_unique<ArcEntity>(m_center, newRadius,
                                            m_startAngleDegrees, m_sweepAngleDegrees);
    }

    if (handleId == "start") {
        // Update start angle based on new position
        double newStartAngle = std::atan2(worldPos.y() - m_center.y(),
                                           worldPos.x() - m_center.x()) * 180.0 / kPi;
        double newRadius = std::max(1.0, QLineF(m_center, worldPos).length());
        return std::make_unique<ArcEntity>(m_center, newRadius,
                                            newStartAngle, m_sweepAngleDegrees);
    }

    if (handleId == "end") {
        // Update sweep angle based on new end position
        double endAngle = std::atan2(worldPos.y() - m_center.y(),
                                     worldPos.x() - m_center.x()) * 180.0 / kPi;
        double newSweep = endAngle - m_startAngleDegrees;
        double newRadius = std::max(1.0, QLineF(m_center, worldPos).length());
        return std::make_unique<ArcEntity>(m_center, newRadius,
                                            m_startAngleDegrees, newSweep);
    }

    return clone();
}

std::vector<QPointF> ArcEntity::snapPoints() const
{
    return {m_center, startPoint(), endPoint()};
}

QList<GcodeMove> ArcEntity::toGcodeMoves(double zSafe, double zCut, double feedRate) const
{
    QList<GcodeMove> moves;
    const QPointF startPt = startPoint();
    const QPointF endPt = endPoint();

    // G0 rapid to start point (pen up)
    GcodeMove rapid1;
    rapid1.type = GcodeMove::Type::Rapid;
    rapid1.start = QVector3D(0, 0, zSafe);
    rapid1.end = QVector3D(startPt.x(), startPt.y(), zSafe);
    moves.append(rapid1);

    // G1 plunge (pen down)
    GcodeMove plunge;
    plunge.type = GcodeMove::Type::Linear;
    plunge.start = QVector3D(startPt.x(), startPt.y(), zSafe);
    plunge.end = QVector3D(startPt.x(), startPt.y(), zCut);
    plunge.feedRate = feedRate;
    moves.append(plunge);

    // G2/G3 arc move
    GcodeMove arc;
    arc.type = (m_sweepAngleDegrees >= 0) ? GcodeMove::Type::ArcCCW : GcodeMove::Type::ArcCW;
    arc.start = QVector3D(startPt.x(), startPt.y(), zCut);
    arc.end = QVector3D(endPt.x(), endPt.y(), zCut);
    // I, J are offsets from start point to center
    arc.iOffset = m_center.x() - startPt.x();
    arc.jOffset = m_center.y() - startPt.y();
    arc.feedRate = feedRate;
    moves.append(arc);

    // G0 raise (pen up)
    GcodeMove rapid2;
    rapid2.type = GcodeMove::Type::Rapid;
    rapid2.start = QVector3D(endPt.x(), endPt.y(), zCut);
    rapid2.end = QVector3D(endPt.x(), endPt.y(), zSafe);
    moves.append(rapid2);

    return moves;
}

QJsonObject ArcEntity::toJson() const
{
    QJsonObject json;
    json["type"] = "arc";
    json["cx"] = m_center.x();
    json["cy"] = m_center.y();
    json["radius"] = m_radius;
    json["startAngle"] = m_startAngleDegrees;
    json["sweepAngle"] = m_sweepAngleDegrees;
    json["rotation"] = rotation();
    return json;
}

std::unique_ptr<ArcEntity> ArcEntity::fromJson(const QJsonObject& json)
{
    QPointF center(json["cx"].toDouble(), json["cy"].toDouble());
    double r = json["radius"].toDouble(10);
    double start = json["startAngle"].toDouble(0);
    double sweep = json["sweepAngle"].toDouble(90);
    auto entity = std::make_unique<ArcEntity>(center, r, start, sweep);
    entity->setRotation(json["rotation"].toDouble(0));
    return entity;
}

double ArcEntity::radius() const
{
    return m_radius;
}

double ArcEntity::startAngleDegrees() const
{
    return m_startAngleDegrees;
}

double ArcEntity::sweepAngleDegrees() const
{
    return m_sweepAngleDegrees;
}

QPointF ArcEntity::startPoint() const
{
    return pointOnCircle(m_center, m_radius, m_startAngleDegrees);
}

QPointF ArcEntity::endPoint() const
{
    return pointOnCircle(m_center, m_radius, m_startAngleDegrees + m_sweepAngleDegrees);
}

void ArcEntity::setGeometry(const QPointF& center, double radius,
                             double startAngleDegrees, double sweepAngleDegrees)
{
    m_center = center;
    m_radius = std::max(radius, 1.0);
    m_startAngleDegrees = startAngleDegrees;
    m_sweepAngleDegrees = sweepAngleDegrees;
}