#include "RectangleEntity.h"

#include <QPolygonF>
#include <algorithm>
#include <array>
#include <cmath>

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kHandleOffset = 8.0;

QPointF rotatePoint(const QPointF& point, double radians) {
    const double c = std::cos(radians);
    const double s = std::sin(radians);
    return QPointF(point.x() * c - point.y() * s, point.x() * s + point.y() * c);
}

std::array<QPointF, 4> rectangleCorners(const QPointF& center, double width, double height, double angleDegrees) {
    const double radians = angleDegrees * kPi / 180.0;
    const QPointF half(width * 0.5, height * 0.5);
    return {
        center + rotatePoint(QPointF(-half.x(), -half.y()), radians),
        center + rotatePoint(QPointF(half.x(), -half.y()), radians),
        center + rotatePoint(QPointF(half.x(), half.y()), radians),
        center + rotatePoint(QPointF(-half.x(), half.y()), radians)
    };
}
}

RectangleEntity::RectangleEntity(const QPointF& firstCorner, const QPointF& secondCorner)
    : Entity(Kind::Rectangle)
    , m_center((firstCorner + secondCorner) * 0.5)
    , m_width(std::abs(secondCorner.x() - firstCorner.x()))
    , m_height(std::abs(secondCorner.y() - firstCorner.y()))
    , m_angleDegrees(0.0)
{
}

RectangleEntity::RectangleEntity(const QPointF& centerPoint, double rectWidth, double rectHeight, double angleDeg)
    : Entity(Kind::Rectangle)
    , m_center(centerPoint)
    , m_width(std::max(rectWidth, 1.0))
    , m_height(std::max(rectHeight, 1.0))
    , m_angleDegrees(angleDeg)
{
}

Entity::Kind RectangleEntity::kind() const
{
    return Kind::Rectangle;
}

QRectF RectangleEntity::boundingRect() const
{
    const auto c = corners();
    double minX = std::min({c[0].x(), c[1].x(), c[2].x(), c[3].x()});
    double maxX = std::max({c[0].x(), c[1].x(), c[2].x(), c[3].x()});
    double minY = std::min({c[0].y(), c[1].y(), c[2].y(), c[3].y()});
    double maxY = std::max({c[0].y(), c[1].y(), c[2].y(), c[3].y()});
    return QRectF(QPointF(minX, minY), QPointF(maxX, maxY));
}

QPointF RectangleEntity::center() const
{
    return m_center;
}

void RectangleEntity::draw(QPainter& painter) const
{
    const auto c = corners();
    QPolygonF polygon;
    polygon << c[0] << c[1] << c[2] << c[3];

    painter.save();
    painter.setBrush(QColor(100, 149, 237, 30));  // Light fill for area visibility
    painter.drawPolygon(polygon);
    painter.restore();
}

double RectangleEntity::distanceTo(const QPointF& point) const
{
    const QPointF local = toLocal(point);
    const double halfW = m_width * 0.5;
    const double halfH = m_height * 0.5;
    const double dx = std::max(std::abs(local.x()) - halfW, 0.0);
    const double dy = std::max(std::abs(local.y()) - halfH, 0.0);

    if (dx == 0.0 && dy == 0.0) {
        // Point is inside the rectangle — return 0 for easy selection
        return 0.0;
    }

    return std::sqrt(dx * dx + dy * dy);
}

void RectangleEntity::translate(const QPointF& delta)
{
    m_center += delta;
}

std::unique_ptr<Entity> RectangleEntity::clone() const
{
    auto e = std::make_unique<RectangleEntity>(m_center, m_width, m_height, m_angleDegrees);
    copyBaseFieldsTo(e.get());
    return e;
}

std::vector<Entity::EditHandle> RectangleEntity::editHandles() const
{
    std::vector<EditHandle> handles;
    const auto c = corners();
    handles.push_back({"corner_tl", c[0], HandleRole::Resize, m_angleDegrees, false, {}});
    handles.push_back({"corner_tr", c[1], HandleRole::Resize, m_angleDegrees, false, {}});
    handles.push_back({"corner_br", c[2], HandleRole::Resize, m_angleDegrees, false, {}});
    handles.push_back({"corner_bl", c[3], HandleRole::Resize, m_angleDegrees, false, {}});

    const QPointF topMid = (c[0] + c[1]) * 0.5;
    QPointF topNormal = topMid - m_center;
    const double topLen = std::hypot(topNormal.x(), topNormal.y());
    if (topLen > 0.0) {
        topNormal /= topLen;
    }
    handles.push_back({"rotate", topMid + topNormal * kHandleOffset * 1.6, HandleRole::Rotate, m_angleDegrees, true, topMid});
    return handles;
}

std::unique_ptr<Entity> RectangleEntity::withDraggedHandle(const QString& handleId, const QPointF& worldPos) const
{
    if (handleId == "rotate") {
        auto updated = std::make_unique<RectangleEntity>(m_center, m_width, m_height, m_angleDegrees);
        const auto c = corners();
        const QPointF topMid = (c[0] + c[1]) * 0.5;
        const double angle = std::atan2(worldPos.y() - topMid.y(), worldPos.x() - topMid.x()) * 180.0 / kPi + 90.0;
        updated->setGeometry(m_center, m_width, m_height, angle);
        return updated;
    }

    const auto c = corners();
    QPointF opposite;
    if (handleId == "corner_tl") opposite = c[2];
    else if (handleId == "corner_tr") opposite = c[3];
    else if (handleId == "corner_br") opposite = c[0];
    else if (handleId == "corner_bl") opposite = c[1];
    else return clone();

    const QPointF newCenter = (opposite + worldPos) * 0.5;
    const double angleRad = m_angleDegrees * kPi / 180.0;
    const QPointF local = rotatePoint(worldPos - opposite, -angleRad);
    return std::make_unique<RectangleEntity>(newCenter,
                                             std::max(2.0, std::abs(local.x())),
                                             std::max(2.0, std::abs(local.y())),
                                             m_angleDegrees);
}

std::vector<QPointF> RectangleEntity::snapPoints() const
{
    const auto c = corners();
    // 4 corners + center + 4 edge midpoints
    const QPointF topMid    = (c[0] + c[1]) * 0.5;
    const QPointF rightMid  = (c[1] + c[2]) * 0.5;
    const QPointF bottomMid = (c[2] + c[3]) * 0.5;
    const QPointF leftMid   = (c[3] + c[0]) * 0.5;
    return {c[0], c[1], c[2], c[3], m_center, topMid, rightMid, bottomMid, leftMid};
}

QList<GcodeMove> RectangleEntity::toGcodeMoves(double zSafe, double zCut, double feedRate) const
{
    QList<GcodeMove> moves;
    const auto c = corners();

    // G0 rapid to first corner (pen up)
    GcodeMove rapid1;
    rapid1.type = GcodeMove::Type::Rapid;
    rapid1.start = QVector3D(0, 0, zSafe);
    rapid1.end = QVector3D(c[0].x(), c[0].y(), zSafe);
    moves.append(rapid1);

    // G1 plunge
    GcodeMove plunge;
    plunge.type = GcodeMove::Type::Linear;
    plunge.start = QVector3D(c[0].x(), c[0].y(), zSafe);
    plunge.end = QVector3D(c[0].x(), c[0].y(), zCut);
    plunge.feedRate = feedRate;
    moves.append(plunge);

    // 4 edges
    for (int i = 0; i < 4; ++i) {
        GcodeMove edge;
        edge.type = GcodeMove::Type::Linear;
        edge.start = QVector3D(c[i].x(), c[i].y(), zCut);
        edge.end = QVector3D(c[(i + 1) % 4].x(), c[(i + 1) % 4].y(), zCut);
        edge.feedRate = feedRate;
        moves.append(edge);
    }

    // G0 raise
    GcodeMove rapid2;
    rapid2.type = GcodeMove::Type::Rapid;
    rapid2.start = QVector3D(c[0].x(), c[0].y(), zCut);
    rapid2.end = QVector3D(c[0].x(), c[0].y(), zSafe);
    moves.append(rapid2);

    return moves;
}

QJsonObject RectangleEntity::toJson() const
{
    QJsonObject json;
    json["type"] = "rectangle";
    json["cx"] = m_center.x();
    json["cy"] = m_center.y();
    json["width"] = m_width;
    json["height"] = m_height;
    json["angle"] = m_angleDegrees;
    json["rotation"] = rotation();
    return json;
}

std::unique_ptr<RectangleEntity> RectangleEntity::fromJson(const QJsonObject& json)
{
    QPointF center(json["cx"].toDouble(), json["cy"].toDouble());
    double w = json["width"].toDouble(10);
    double h = json["height"].toDouble(10);
    double angle = json["angle"].toDouble(0);
    auto entity = std::make_unique<RectangleEntity>(center, w, h, angle);
    entity->setRotation(json["rotation"].toDouble(0));
    return entity;
}

double RectangleEntity::width() const { return m_width; }
double RectangleEntity::height() const { return m_height; }
double RectangleEntity::angleDegrees() const { return m_angleDegrees; }

void RectangleEntity::setGeometry(const QPointF& centerPoint, double rectWidth, double rectHeight, double angleDeg)
{
    m_center = centerPoint;
    m_width = std::max(rectWidth, 1.0);
    m_height = std::max(rectHeight, 1.0);
    m_angleDegrees = angleDeg;
}

QPointF RectangleEntity::toLocal(const QPointF& worldPoint) const
{
    const QPointF delta = worldPoint - m_center;
    const double radians = -m_angleDegrees * kPi / 180.0;
    return rotatePoint(delta, radians);
}

std::array<QPointF, 4> RectangleEntity::corners() const
{
    return rectangleCorners(m_center, m_width, m_height, m_angleDegrees);
}

TrimEnd RectangleEntity::getTrimPoint(const QPointF& clickPos, const QPointF& intersection) const
{
    // Rectangles are not directly trimmable - return None
    return TrimEnd::None;
}

QPointF RectangleEntity::pointAtTrimEnd(TrimEnd end) const
{
    const auto c = corners();
    return (end == TrimEnd::Start) ? c[0] : c[3];
}