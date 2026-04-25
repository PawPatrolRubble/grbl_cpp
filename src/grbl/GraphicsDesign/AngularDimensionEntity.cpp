#include "AngularDimensionEntity.h"

#include <QPainter>
#include <QLineF>
#include <QFontMetricsF>
#include <QJsonObject>
#include <QtMath>
#include <algorithm>
#include <cmath>

// -----------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------

static QPointF unitDir(const QPointF& from, const QPointF& to)
{
    QPointF d = to - from;
    double len = QLineF(from, to).length();
    return len > 1e-9 ? d / len : QPointF(1, 0);
}

// -----------------------------------------------------------------------
// Constructor
// -----------------------------------------------------------------------

AngularDimensionEntity::AngularDimensionEntity(const QPointF& vertex,
                                               const QPointF& p1,
                                               const QPointF& p2,
                                               double radius)
    : Entity(Kind::AngularDimension)
    , m_vertex(vertex)
    , m_p1(p1)
    , m_p2(p2)
    , m_radius(radius)
{
}

// -----------------------------------------------------------------------
// Angle utilities
// -----------------------------------------------------------------------

double AngularDimensionEntity::angleDeg(const QPointF& p) const
{
    QPointF d = p - m_vertex;
    return normDeg(qRadiansToDegrees(qAtan2(d.y(), d.x())));
}

double AngularDimensionEntity::normDeg(double a)
{
    a = std::fmod(a, 360.0);
    if (a < 0) a += 360.0;
    return a;
}

double AngularDimensionEntity::sweepCCW(double a1, double a2)
{
    double s = normDeg(a2 - a1);
    return s == 0.0 ? 360.0 : s;
}

double AngularDimensionEntity::measuredAngleDegrees() const
{
    return sweepCCW(angleDeg(m_p1), angleDeg(m_p2));
}

// -----------------------------------------------------------------------
// Entity interface
// -----------------------------------------------------------------------

Entity::Kind AngularDimensionEntity::kind() const
{
    return Kind::AngularDimension;
}

QPointF AngularDimensionEntity::center() const
{
    return m_vertex;
}

QRectF AngularDimensionEntity::boundingRect() const
{
    // Bounding box covering vertex + arc radius
    return QRectF(m_vertex.x() - m_radius - 4,
                  m_vertex.y() - m_radius - 4,
                  m_radius * 2 + 8,
                  m_radius * 2 + 8);
}

void AngularDimensionEntity::translate(const QPointF& delta)
{
    m_vertex += delta;
    m_p1 += delta;
    m_p2 += delta;
}

std::unique_ptr<Entity> AngularDimensionEntity::clone() const
{
    auto e = std::make_unique<AngularDimensionEntity>(m_vertex, m_p1, m_p2, m_radius);
    copyBaseFieldsTo(e.get());
    return e;
}

double AngularDimensionEntity::distanceTo(const QPointF& pt) const
{
    // Distance to the arc
    double dist = QLineF(m_vertex, pt).length();
    return std::abs(dist - m_radius);
}

// -----------------------------------------------------------------------
// Draw
// -----------------------------------------------------------------------

void AngularDimensionEntity::draw(QPainter& painter) const
{
    QPen savedPen = painter.pen();

    double a1 = angleDeg(m_p1);
    double sweep = sweepCCW(a1, angleDeg(m_p2));

    // Draw radial lines from vertex to arc start/end
    QPointF arcStart(m_vertex.x() + m_radius * qCos(qDegreesToRadians(a1)),
                     m_vertex.y() + m_radius * qSin(qDegreesToRadians(a1)));
    QPointF arcEnd  (m_vertex.x() + m_radius * qCos(qDegreesToRadians(a1 + sweep)),
                     m_vertex.y() + m_radius * qSin(qDegreesToRadians(a1 + sweep)));

    // Extension lines
    QPointF dir1 = unitDir(m_vertex, m_p1);
    QPointF dir2 = unitDir(m_vertex, m_p2);
    const double overshoot = 3.0;
    painter.drawLine(m_vertex + dir1 * 2.0, m_vertex + dir1 * (m_radius + overshoot));
    painter.drawLine(m_vertex + dir2 * 2.0, m_vertex + dir2 * (m_radius + overshoot));

    // Arc — QPainter uses degrees * 16, and CCW in world coords needs care
    // QPainter drawArc: startAngle in 1/16°, CCW positive
    QRectF arcRect(m_vertex.x() - m_radius, m_vertex.y() - m_radius,
                   m_radius * 2, m_radius * 2);
    painter.drawArc(arcRect,
                    static_cast<int>(a1 * 16),
                    static_cast<int>(sweep * 16));

    // Arrowheads at arc ends (tangent to arc)
    auto drawArrow = [&](const QPointF& tip, double arcAngleDeg, double direction) {
        // Tangent direction at tip: perpendicular to radius
        double tang = arcAngleDeg + 90.0 * direction;
        QPointF tangDir(qCos(qDegreesToRadians(tang)), qSin(qDegreesToRadians(tang)));
        QPointF perpDir(-tangDir.y(), tangDir.x());
        double arrowLen = 3.5;
        QPolygonF arrow;
        arrow << tip
              << (tip + tangDir * arrowLen + perpDir * (arrowLen * 0.35))
              << (tip + tangDir * arrowLen - perpDir * (arrowLen * 0.35));
        painter.setBrush(savedPen.color());
        QPen noPen(Qt::NoPen);
        painter.setPen(noPen);
        painter.drawPolygon(arrow);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(savedPen);
    };

    // Arrow at arcStart pointing CCW (direction +1)
    drawArrow(arcStart, a1, -1.0);
    // Arrow at arcEnd pointing CW (direction -1)
    drawArrow(arcEnd, a1 + sweep, 1.0);

    // Dimension text at arc midpoint
    double midAngle = a1 + sweep / 2.0;
    QPointF textPos(m_vertex.x() + m_radius * qCos(qDegreesToRadians(midAngle)),
                    m_vertex.y() + m_radius * qSin(qDegreesToRadians(midAngle)));

    double angleDeg = measuredAngleDegrees();
    QString text = QString::number(angleDeg, 'f', 1) + "°";

    painter.save();
    painter.translate(textPos);
    double textHeight = std::max(3.0, m_radius * 0.2);
    QFont font("Arial", 1);
    font.setPointSizeF(textHeight * 0.72);
    painter.setFont(font);
    QFontMetricsF fm(font);
    QRectF textRect = fm.boundingRect(text);
    painter.scale(1, -1);
    painter.drawText(QRectF(-textRect.width() / 2 - 1, -textRect.height() - 0.5,
                             textRect.width() + 2, textRect.height() + 1),
                     Qt::AlignCenter, text);
    painter.restore();
}

// -----------------------------------------------------------------------
// Handle editing
// -----------------------------------------------------------------------

std::vector<Entity::EditHandle> AngularDimensionEntity::editHandles() const
{
    std::vector<EditHandle> handles;
    handles.push_back({"vertex", m_vertex, HandleRole::Resize});
    handles.push_back({"p1",     m_p1,     HandleRole::Resize});
    handles.push_back({"p2",     m_p2,     HandleRole::Resize});
    // Radius grip: on arc at midpoint angle
    double a1 = angleDeg(m_p1);
    double sweep = sweepCCW(a1, angleDeg(m_p2));
    double midA = a1 + sweep / 2.0;
    QPointF radiusGrip(m_vertex.x() + m_radius * qCos(qDegreesToRadians(midA)),
                       m_vertex.y() + m_radius * qSin(qDegreesToRadians(midA)));
    handles.push_back({"radius", radiusGrip, HandleRole::Resize});
    return handles;
}

std::unique_ptr<Entity> AngularDimensionEntity::withDraggedHandle(
    const QString& handleId, const QPointF& worldPos) const
{
    auto copy = std::make_unique<AngularDimensionEntity>(m_vertex, m_p1, m_p2, m_radius);
    copy->setId(id());
    copy->setLayerId(layerId());

    if (handleId == "vertex") {
        QPointF delta = worldPos - m_vertex;
        copy->m_vertex = worldPos;
        copy->m_p1 = m_p1 + delta;
        copy->m_p2 = m_p2 + delta;
    } else if (handleId == "p1") {
        copy->m_p1 = worldPos;
    } else if (handleId == "p2") {
        copy->m_p2 = worldPos;
    } else if (handleId == "radius") {
        double newR = QLineF(m_vertex, worldPos).length();
        if (newR < 3.0) newR = 3.0;
        copy->m_radius = newR;
    }
    return copy;
}

std::vector<QPointF> AngularDimensionEntity::snapPoints() const
{
    return {m_vertex, m_p1, m_p2};
}

// -----------------------------------------------------------------------
// G-code: annotations don't produce toolpath moves
// -----------------------------------------------------------------------

QList<GcodeMove> AngularDimensionEntity::toGcodeMoves(double, double, double) const
{
    return {};
}

// -----------------------------------------------------------------------
// Serialization
// -----------------------------------------------------------------------

QJsonObject AngularDimensionEntity::toJson() const
{
    QJsonObject obj;
    obj["type"]   = "angularDimension";
    obj["vx"]     = m_vertex.x();
    obj["vy"]     = m_vertex.y();
    obj["p1x"]    = m_p1.x();
    obj["p1y"]    = m_p1.y();
    obj["p2x"]    = m_p2.x();
    obj["p2y"]    = m_p2.y();
    obj["radius"] = m_radius;
    return obj;
}

std::unique_ptr<AngularDimensionEntity> AngularDimensionEntity::fromJson(const QJsonObject& json)
{
    QPointF vertex(json["vx"].toDouble(), json["vy"].toDouble());
    QPointF p1(json["p1x"].toDouble(), json["p1y"].toDouble());
    QPointF p2(json["p2x"].toDouble(), json["p2y"].toDouble());
    double radius = json["radius"].toDouble(15.0);
    return std::make_unique<AngularDimensionEntity>(vertex, p1, p2, radius);
}

QString AngularDimensionEntity::displayName() const
{
    return QString("Angle (%1°)").arg(measuredAngleDegrees(), 0, 'f', 1);
}
