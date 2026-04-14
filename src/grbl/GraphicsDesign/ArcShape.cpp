#include "ArcShape.h"
#include <cmath>

static constexpr double PI = 3.14159265358979323846;

ArcShape::ArcShape(const QPointF &center, double radius, double startAngle, double sweepAngle)
    : Shape(Type::Arc)
    , m_center(center)
    , m_radius(radius)
    , m_startAngle(startAngle)
    , m_sweepAngle(sweepAngle)
{
}

void ArcShape::setCenter(const QPointF &c) { m_center = c; }
void ArcShape::setRadius(double r) { m_radius = r; }
void ArcShape::setStartAngle(double deg) { m_startAngle = deg; }
void ArcShape::setSweepAngle(double deg) { m_sweepAngle = deg; }

QPointF ArcShape::startPoint() const
{
    double rad = m_startAngle * PI / 180.0;
    return QPointF(m_center.x() + m_radius * std::cos(rad),
                   m_center.y() + m_radius * std::sin(rad));
}

QPointF ArcShape::endPoint() const
{
    double rad = (m_startAngle + m_sweepAngle) * PI / 180.0;
    return QPointF(m_center.x() + m_radius * std::cos(rad),
                   m_center.y() + m_radius * std::sin(rad));
}

QRectF ArcShape::boundingRect() const
{
    // Simplified bounding rect - use full circle bounds
    return QRectF(m_center.x() - m_radius, m_center.y() - m_radius,
                  2 * m_radius, 2 * m_radius);
}

void ArcShape::draw(QPainterPath &path) const
{
    QPointF startPt = startPoint();
    path.moveTo(startPt);

    // Use Qt's arcTo for accurate rendering
    QRectF arcRect(m_center.x() - m_radius, m_center.y() - m_radius,
                   2 * m_radius, 2 * m_radius);
    // Qt arcTo uses degrees, positive = CCW
    path.arcTo(arcRect, m_startAngle, m_sweepAngle);
}

QList<QPointF> ArcShape::handlePositions() const
{
    // Center + start point + end point
    return {
        m_center,
        startPoint(),
        endPoint()
    };
}

QList<GcodeMove> ArcShape::toGcodeMoves(double zSafe, double zCut, double feedRate) const
{
    QList<GcodeMove> moves;

    QPointF startPt = startPoint();
    QPointF endPt = endPoint();

    // G0 rapid to start (pen up)
    GcodeMove rapid1;
    rapid1.type = GcodeMove::Type::Rapid;
    rapid1.start = QVector3D(0, 0, zSafe);
    rapid1.end = QVector3D(startPt.x(), startPt.y(), zSafe);
    moves.append(rapid1);

    // G1 plunge
    GcodeMove plunge;
    plunge.type = GcodeMove::Type::Linear;
    plunge.start = QVector3D(startPt.x(), startPt.y(), zSafe);
    plunge.end = QVector3D(startPt.x(), startPt.y(), zCut);
    plunge.feedRate = feedRate;
    moves.append(plunge);

    // G2 or G3 arc depending on sweep direction
    GcodeMove arc;
    arc.type = (m_sweepAngle >= 0) ? GcodeMove::Type::ArcCCW : GcodeMove::Type::ArcCW;
    arc.start = QVector3D(startPt.x(), startPt.y(), zCut);
    arc.end = QVector3D(endPt.x(), endPt.y(), zCut);
    // I/J are offsets from start point to center
    arc.iOffset = m_center.x() - startPt.x();
    arc.jOffset = m_center.y() - startPt.y();
    arc.feedRate = feedRate;
    moves.append(arc);

    // G0 raise
    GcodeMove rapid2;
    rapid2.type = GcodeMove::Type::Rapid;
    rapid2.start = QVector3D(endPt.x(), endPt.y(), zCut);
    rapid2.end = QVector3D(endPt.x(), endPt.y(), zSafe);
    moves.append(rapid2);

    return moves;
}

QJsonObject ArcShape::toJson() const
{
    QJsonObject json;
    json["type"] = "arc";
    json["cx"] = m_center.x();
    json["cy"] = m_center.y();
    json["radius"] = m_radius;
    json["startAngle"] = m_startAngle;
    json["sweepAngle"] = m_sweepAngle;
    json["rotation"] = m_rotation;
    return json;
}

ArcShape* ArcShape::fromJson(const QJsonObject &json)
{
    QPointF center(json["cx"].toDouble(), json["cy"].toDouble());
    double r = json["radius"].toDouble(10);
    double start = json["startAngle"].toDouble(0);
    double sweep = json["sweepAngle"].toDouble(90);
    ArcShape *shape = new ArcShape(center, r, start, sweep);
    shape->setRotation(json["rotation"].toDouble(0));
    return shape;
}

void ArcShape::translate(double dx, double dy)
{
    m_center += QPointF(dx, dy);
}

void ArcShape::scale(double sx, double sy, const QPointF &pivot)
{
    m_center = QPointF(pivot.x() + (m_center.x() - pivot.x()) * sx,
                       pivot.y() + (m_center.y() - pivot.y()) * sy);
    m_radius *= std::max(sx, sy);
}