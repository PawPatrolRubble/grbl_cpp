#include "RadiusDimensionEntity.h"

#include <QPainter>
#include <QLineF>
#include <QFontMetricsF>
#include <QJsonObject>
#include <QtMath>
#include <algorithm>
#include <cmath>

// -----------------------------------------------------------------------
// Constructor
// -----------------------------------------------------------------------

RadiusDimensionEntity::RadiusDimensionEntity(const QPointF& center,
                                             double radius,
                                             double angleDeg,
                                             bool diameter)
    : Entity(Kind::RadiusDimension)
    , m_center(center)
    , m_radius(radius)
    , m_angleDeg(angleDeg)
    , m_diameter(diameter)
{
}

// -----------------------------------------------------------------------
// Geometry
// -----------------------------------------------------------------------

QPointF RadiusDimensionEntity::tipPoint() const
{
    double rad = qDegreesToRadians(m_angleDeg);
    return { m_center.x() + m_radius * qCos(rad),
             m_center.y() + m_radius * qSin(rad) };
}

// -----------------------------------------------------------------------
// Entity interface
// -----------------------------------------------------------------------

Entity::Kind RadiusDimensionEntity::kind() const
{
    return Kind::RadiusDimension;
}

QPointF RadiusDimensionEntity::center() const
{
    return m_center;
}

QRectF RadiusDimensionEntity::boundingRect() const
{
    QPointF tip = tipPoint();
    double minX = std::min(m_center.x(), tip.x());
    double minY = std::min(m_center.y(), tip.y());
    double maxX = std::max(m_center.x(), tip.x());
    double maxY = std::max(m_center.y(), tip.y());
    return QRectF(minX - 4, minY - 4, (maxX - minX) + 8, (maxY - minY) + 8);
}

void RadiusDimensionEntity::translate(const QPointF& delta)
{
    m_center += delta;
}

std::unique_ptr<Entity> RadiusDimensionEntity::clone() const
{
    auto e = std::make_unique<RadiusDimensionEntity>(m_center, m_radius, m_angleDeg, m_diameter);
    copyBaseFieldsTo(e.get());
    return e;
}

double RadiusDimensionEntity::distanceTo(const QPointF& pt) const
{
    // Distance to the leader line segment
    QPointF tip = tipPoint();
    QLineF seg(m_center, tip);
    double len = seg.length();
    if (len < 1e-9) return QLineF(m_center, pt).length();
    QPointF dir = (tip - m_center) / len;
    double t = QPointF::dotProduct(pt - m_center, dir);
    t = std::clamp(t, 0.0, len);
    QPointF closest = m_center + dir * t;
    return QLineF(pt, closest).length();
}

// -----------------------------------------------------------------------
// Draw
// -----------------------------------------------------------------------

void RadiusDimensionEntity::draw(QPainter& painter) const
{
    QPen savedPen = painter.pen();

    QPointF tip = tipPoint();
    QPointF dir;
    {
        QLineF line(m_center, tip);
        double len = line.length();
        dir = len > 1e-9 ? (tip - m_center) / len : QPointF(1, 0);
    }

    // Leader line: from center to tip, with a short extension beyond
    const double ext = 5.0;
    QPointF extTip = tip + dir * ext;
    painter.drawLine(m_center, extTip);

    // Arrowhead at tip (pointing outward from center)
    double arrowLen = 3.5;
    QPointF perpDir(-dir.y(), dir.x());
    QPolygonF arrow;
    arrow << tip
          << (tip - dir * arrowLen + perpDir * (arrowLen * 0.35))
          << (tip - dir * arrowLen - perpDir * (arrowLen * 0.35));
    painter.setBrush(savedPen.color());
    QPen noPen(Qt::NoPen);
    painter.setPen(noPen);
    painter.drawPolygon(arrow);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(savedPen);

    // Label
    double value = m_diameter ? m_radius * 2.0 : m_radius;
    QString prefix = m_diameter ? QString::fromUtf8("\xE2\x8C\x80 ") : "R ";
    QString text = prefix + (value < 10.0
        ? QString::number(value, 'f', 3)
        : QString::number(value, 'f', 2));

    // Place label beyond the extension tip, in the direction of the leader
    QPointF textPos = extTip + dir * 2.0;
    double angle = qRadiansToDegrees(qAtan2(dir.y(), dir.x()));

    painter.save();
    painter.translate(textPos);
    painter.rotate(-angle);
    double textHeight = std::max(3.0, m_radius * 0.2);
    QFont font("Arial", 1);
    font.setPointSizeF(textHeight * 0.72);
    painter.setFont(font);
    QFontMetricsF fm(font);
    QRectF textRect = fm.boundingRect(text);
    painter.scale(1, -1);
    // Anchor text to the left of the position (in the leader direction)
    painter.drawText(QRectF(0, -textRect.height() - 0.5,
                             textRect.width() + 2, textRect.height() + 1),
                     Qt::AlignLeft | Qt::AlignVCenter, text);
    painter.restore();
}

// -----------------------------------------------------------------------
// Handle editing
// -----------------------------------------------------------------------

std::vector<Entity::EditHandle> RadiusDimensionEntity::editHandles() const
{
    return {
        {"center", m_center,  HandleRole::Resize},
        {"tip",    tipPoint(), HandleRole::Resize},
    };
}

std::unique_ptr<Entity> RadiusDimensionEntity::withDraggedHandle(
    const QString& handleId, const QPointF& worldPos) const
{
    auto copy = std::make_unique<RadiusDimensionEntity>(m_center, m_radius, m_angleDeg, m_diameter);
    copy->setId(id());
    copy->setLayerId(layerId());

    if (handleId == "center") {
        copy->m_center = worldPos;
    } else if (handleId == "tip") {
        double newR = QLineF(m_center, worldPos).length();
        if (newR < 0.5) newR = 0.5;
        copy->m_radius   = newR;
        copy->m_angleDeg = qRadiansToDegrees(qAtan2(worldPos.y() - m_center.y(),
                                                     worldPos.x() - m_center.x()));
    }
    return copy;
}

std::vector<QPointF> RadiusDimensionEntity::snapPoints() const
{
    return { m_center, tipPoint() };
}

// -----------------------------------------------------------------------
// G-code: annotations don't produce toolpath moves
// -----------------------------------------------------------------------

QList<GcodeMove> RadiusDimensionEntity::toGcodeMoves(double, double, double) const
{
    return {};
}

// -----------------------------------------------------------------------
// Serialization
// -----------------------------------------------------------------------

QJsonObject RadiusDimensionEntity::toJson() const
{
    QJsonObject obj;
    obj["type"]     = "radiusDimension";
    obj["cx"]       = m_center.x();
    obj["cy"]       = m_center.y();
    obj["radius"]   = m_radius;
    obj["angle"]    = m_angleDeg;
    obj["diameter"] = m_diameter;
    return obj;
}

std::unique_ptr<RadiusDimensionEntity> RadiusDimensionEntity::fromJson(const QJsonObject& json)
{
    QPointF center(json["cx"].toDouble(), json["cy"].toDouble());
    double radius   = json["radius"].toDouble(10.0);
    double angle    = json["angle"].toDouble(0.0);
    bool diameter   = json["diameter"].toBool(false);
    return std::make_unique<RadiusDimensionEntity>(center, radius, angle, diameter);
}

QString RadiusDimensionEntity::displayName() const
{
    if (m_diameter)
        return QString("Diameter (%1)").arg(m_radius * 2.0, 0, 'f', 2);
    return QString("Radius (%1)").arg(m_radius, 0, 'f', 2);
}

TrimEnd RadiusDimensionEntity::getTrimPoint(const QPointF& clickPos, const QPointF& intersection) const
{
    // Dimensions are not directly trimmable - return None
    return TrimEnd::None;
}

QPointF RadiusDimensionEntity::pointAtTrimEnd(TrimEnd end) const
{
    return m_center;
}
