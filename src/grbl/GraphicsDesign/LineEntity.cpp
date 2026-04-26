#include "LineEntity.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kHandleOffset = 8.0;
constexpr double kMinBoundsExtent = 1e-6;

double angleDegreesForVector(const QPointF& vector) {
    return std::atan2(vector.y(), vector.x()) * 180.0 / kPi;
}
}

LineEntity::LineEntity(const QPointF& start, const QPointF& end)
    : Entity(Kind::Line)
    , m_line(start, end)
{
}

Entity::Kind LineEntity::kind() const
{
    return Kind::Line;
}

QRectF LineEntity::boundingRect() const
{
    double minX = std::min(m_line.p1().x(), m_line.p2().x());
    double maxX = std::max(m_line.p1().x(), m_line.p2().x());
    double minY = std::min(m_line.p1().y(), m_line.p2().y());
    double maxY = std::max(m_line.p1().y(), m_line.p2().y());

    QRectF bounds(QPointF(minX, minY), QPointF(maxX, maxY));
    if (bounds.width() < kMinBoundsExtent) {
        bounds.adjust(-0.5 * kMinBoundsExtent, 0.0, 0.5 * kMinBoundsExtent, 0.0);
    }
    if (bounds.height() < kMinBoundsExtent) {
        bounds.adjust(0.0, -0.5 * kMinBoundsExtent, 0.0, 0.5 * kMinBoundsExtent);
    }
    return bounds;
}

QPointF LineEntity::center() const
{
    return (m_line.p1() + m_line.p2()) * 0.5;
}

void LineEntity::draw(QPainter& painter) const
{
    painter.drawLine(m_line);
}

double LineEntity::distanceTo(const QPointF& point) const
{
    const QPointF delta = m_line.p2() - m_line.p1();
    const double lengthSquared = delta.x() * delta.x() + delta.y() * delta.y();

    if (qFuzzyIsNull(lengthSquared)) {
        return QLineF(point, m_line.p1()).length();
    }

    const QPointF pointDelta = point - m_line.p1();
    double t = (pointDelta.x() * delta.x() + pointDelta.y() * delta.y()) / lengthSquared;
    t = std::clamp(t, 0.0, 1.0);

    const QPointF projection(m_line.p1().x() + delta.x() * t,
                             m_line.p1().y() + delta.y() * t);
    return QLineF(point, projection).length();
}

void LineEntity::translate(const QPointF& delta)
{
    m_line.translate(delta);
}

std::unique_ptr<Entity> LineEntity::clone() const
{
    auto e = std::make_unique<LineEntity>(m_line.p1(), m_line.p2());
    copyBaseFieldsTo(e.get());
    return e;
}

std::vector<Entity::EditHandle> LineEntity::editHandles() const
{
    std::vector<EditHandle> handles;
    const QPointF start = startPoint();
    const QPointF end = endPoint();
    const QPointF mid = center();
    QPointF dir = end - start;
    const double len = std::hypot(dir.x(), dir.y());
    if (len > 0.0) {
        dir /= len;
    }
    const QPointF normal(-dir.y(), dir.x());
    const double handleAngle = angleDegrees();

    handles.push_back({"start", start, HandleRole::Resize, handleAngle, false, {}});
    handles.push_back({"end", end, HandleRole::Resize, handleAngle, false, {}});
    handles.push_back({"rotate", mid - normal * kHandleOffset * 1.6, HandleRole::Rotate, handleAngle, true, mid});
    return handles;
}

std::unique_ptr<Entity> LineEntity::withDraggedHandle(const QString& handleId, const QPointF& worldPos) const
{
    auto updated = std::make_unique<LineEntity>(startPoint(), endPoint());
    if (handleId == "start") {
        updated->setStartEnd(worldPos, endPoint());
    } else if (handleId == "end") {
        updated->setStartEnd(startPoint(), worldPos);
    } else if (handleId == "rotate") {
        updated->setCenterLengthAngle(center(), length(), angleDegreesForVector(worldPos - center()));
    }
    return updated;
}

std::vector<QPointF> LineEntity::snapPoints() const
{
    return {startPoint(), endPoint(), center()};
}

QList<GcodeMove> LineEntity::toGcodeMoves(double zSafe, double zCut, double feedRate) const
{
    QList<GcodeMove> moves;

    // G0 rapid to start point (pen up)
    GcodeMove rapid1;
    rapid1.type = GcodeMove::Type::Rapid;
    rapid1.start = QVector3D(0, 0, zSafe);
    rapid1.end = QVector3D(startPoint().x(), startPoint().y(), zSafe);
    moves.append(rapid1);

    // G1 plunge (pen down)
    GcodeMove plunge;
    plunge.type = GcodeMove::Type::Linear;
    plunge.start = QVector3D(startPoint().x(), startPoint().y(), zSafe);
    plunge.end = QVector3D(startPoint().x(), startPoint().y(), zCut);
    plunge.feedRate = feedRate;
    moves.append(plunge);

    // G1 draw line
    GcodeMove draw;
    draw.type = GcodeMove::Type::Linear;
    draw.start = QVector3D(startPoint().x(), startPoint().y(), zCut);
    draw.end = QVector3D(endPoint().x(), endPoint().y(), zCut);
    draw.feedRate = feedRate;
    moves.append(draw);

    // G0 raise (pen up)
    GcodeMove rapid2;
    rapid2.type = GcodeMove::Type::Rapid;
    rapid2.start = QVector3D(endPoint().x(), endPoint().y(), zCut);
    rapid2.end = QVector3D(endPoint().x(), endPoint().y(), zSafe);
    moves.append(rapid2);

    return moves;
}

QJsonObject LineEntity::toJson() const
{
    QJsonObject json;
    json["type"] = "line";
    json["x1"] = m_line.p1().x();
    json["y1"] = m_line.p1().y();
    json["x2"] = m_line.p2().x();
    json["y2"] = m_line.p2().y();
    json["rotation"] = rotation();
    return json;
}

std::unique_ptr<LineEntity> LineEntity::fromJson(const QJsonObject& json)
{
    QPointF p1(json["x1"].toDouble(), json["y1"].toDouble());
    QPointF p2(json["x2"].toDouble(), json["y2"].toDouble());
    auto entity = std::make_unique<LineEntity>(p1, p2);
    entity->setRotation(json["rotation"].toDouble(0));
    return entity;
}

QPointF LineEntity::startPoint() const
{
    return m_line.p1();
}

QPointF LineEntity::endPoint() const
{
    return m_line.p2();
}

double LineEntity::length() const
{
    return m_line.length();
}

double LineEntity::angleDegrees() const
{
    return -m_line.angle();
}

void LineEntity::setStartEnd(const QPointF& start, const QPointF& end)
{
    m_line.setP1(start);
    m_line.setP2(end);
}

void LineEntity::setCenterLengthAngle(const QPointF& centerPoint, double lineLength, double angleDeg)
{
    lineLength = std::max(lineLength, 1.0);
    const double radians = angleDeg * kPi / 180.0;
    const QPointF halfDelta(std::cos(radians) * lineLength * 0.5,
                            std::sin(radians) * lineLength * 0.5);
    setStartEnd(centerPoint - halfDelta, centerPoint + halfDelta);
}