#include "LinearDimensionEntity.h"

#include <QPainter>
#include <QLineF>
#include <QFontMetricsF>
#include <QJsonObject>
#include <QtMath>
#include <algorithm>

// -----------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------

static QPointF normalizedDir(const QPointF& a, const QPointF& b)
{
    QPointF d = b - a;
    double len = QLineF(a, b).length();
    return len > 1e-9 ? d / len : QPointF(1, 0);
}

// -----------------------------------------------------------------------
// Constructor
// -----------------------------------------------------------------------

LinearDimensionEntity::LinearDimensionEntity(const QPointF& p1, const QPointF& p2,
                                             double offsetDistance)
    : Entity(Kind::LinearDimension)
    , m_p1(p1)
    , m_p2(p2)
    , m_offsetDistance(offsetDistance)
{
}

// -----------------------------------------------------------------------
// Geometry
// -----------------------------------------------------------------------

QPointF LinearDimensionEntity::perp() const
{
    QPointF d = normalizedDir(m_p1, m_p2);
    return {-d.y(), d.x()};  // 90° CCW
}

double LinearDimensionEntity::measuredLength() const
{
    return QLineF(m_p1, m_p2).length();
}

// -----------------------------------------------------------------------
// Entity interface
// -----------------------------------------------------------------------

Entity::Kind LinearDimensionEntity::kind() const
{
    return Kind::LinearDimension;
}

QPointF LinearDimensionEntity::center() const
{
    return (m_p1 + m_p2) / 2.0;
}

QRectF LinearDimensionEntity::boundingRect() const
{
    QPointF p = perp();
    QPointF d1 = m_p1 + p * m_offsetDistance;
    QPointF d2 = m_p2 + p * m_offsetDistance;
    QRectF r;
    r.setLeft  (std::min({m_p1.x(), m_p2.x(), d1.x(), d2.x()}));
    r.setRight (std::max({m_p1.x(), m_p2.x(), d1.x(), d2.x()}));
    r.setBottom(std::min({m_p1.y(), m_p2.y(), d1.y(), d2.y()}));
    r.setTop   (std::max({m_p1.y(), m_p2.y(), d1.y(), d2.y()}));
    return r.adjusted(-2, -2, 2, 2);
}

void LinearDimensionEntity::translate(const QPointF& delta)
{
    m_p1 += delta;
    m_p2 += delta;
}

std::unique_ptr<Entity> LinearDimensionEntity::clone() const
{
    auto e = std::make_unique<LinearDimensionEntity>(m_p1, m_p2, m_offsetDistance);
    copyBaseFieldsTo(e.get());
    return e;
}

double LinearDimensionEntity::distanceTo(const QPointF& pt) const
{
    // Distance to the dimension line segment
    QPointF p = perp();
    QPointF d1 = m_p1 + p * m_offsetDistance;
    QPointF d2 = m_p2 + p * m_offsetDistance;
    QLineF seg(d1, d2);
    // Closest point on the dimension line segment
    QPointF dir = normalizedDir(d1, d2);
    double t = QPointF::dotProduct(pt - d1, dir);
    t = std::clamp(t, 0.0, seg.length());
    QPointF closest = d1 + dir * t;
    return QLineF(pt, closest).length();
}

// -----------------------------------------------------------------------
// Draw
// -----------------------------------------------------------------------

void LinearDimensionEntity::draw(QPainter& painter) const
{
    QPen savedPen = painter.pen();

    QPointF p = perp();
    QPointF d1 = m_p1 + p * m_offsetDistance;
    QPointF d2 = m_p2 + p * m_offsetDistance;

    // Extension lines (with small gap and overshoot)
    const double gap = 1.5;
    const double overshoot = 2.0;
    QPointF ext1Start = m_p1 + p * gap;
    QPointF ext1End   = d1 + p * overshoot;
    QPointF ext2Start = m_p2 + p * gap;
    QPointF ext2End   = d2 + p * overshoot;

    painter.drawLine(ext1Start, ext1End);
    painter.drawLine(ext2Start, ext2End);

    // Dimension line
    painter.drawLine(d1, d2);

    // Arrowheads (filled triangles)
    double arrowLen = 3.5;
    QPointF dir = normalizedDir(d1, d2);
    QPointF perpDir(-dir.y(), dir.x());
    // Arrow at d1 (pointing towards d2)
    QPolygonF arrow1;
    arrow1 << d1
           << (d1 + dir * arrowLen + perpDir * (arrowLen * 0.35))
           << (d1 + dir * arrowLen - perpDir * (arrowLen * 0.35));
    // Arrow at d2 (pointing towards d1)
    QPolygonF arrow2;
    arrow2 << d2
           << (d2 - dir * arrowLen + perpDir * (arrowLen * 0.35))
           << (d2 - dir * arrowLen - perpDir * (arrowLen * 0.35));

    painter.setBrush(savedPen.color());
    QPen noPen(Qt::NoPen);
    painter.setPen(noPen);
    painter.drawPolygon(arrow1);
    painter.drawPolygon(arrow2);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(savedPen);

    // Dimension text
    double length = measuredLength();
    QString text = length < 10.0
        ? QString::number(length, 'f', 3)
        : QString::number(length, 'f', 2);

    // Text position: midpoint of dimension line, slightly above
    QPointF mid = (d1 + d2) / 2.0;
    double angle = qRadiansToDegrees(qAtan2(-(d2.y() - d1.y()), d2.x() - d1.x()));

    painter.save();
    painter.translate(mid);
    painter.rotate(-angle);  // negate because Y is flipped in world coords
    // Scale text to world units: use a fixed size proportional to the dimension
    double textHeight = std::max(3.0, std::abs(m_offsetDistance) * 0.3);
    QFont font("Arial", 1);
    font.setPointSizeF(textHeight * 0.72);  // approximate world→pt
    painter.setFont(font);

    QFontMetricsF fm(font);
    QRectF textRect = fm.boundingRect(text);
    // Position text centered on the dimension line, slightly offset into perp direction
    painter.scale(1, -1);  // un-flip Y for text rendering
    painter.drawText(QRectF(-textRect.width() / 2 - 1, -textRect.height() - 0.5,
                             textRect.width() + 2, textRect.height() + 1),
                     Qt::AlignCenter, text);
    painter.restore();
}

// -----------------------------------------------------------------------
// Handle editing
// -----------------------------------------------------------------------

std::vector<Entity::EditHandle> LinearDimensionEntity::editHandles() const
{
    std::vector<EditHandle> handles;
    handles.push_back({"p1",     m_p1,                   HandleRole::Resize});
    handles.push_back({"p2",     m_p2,                   HandleRole::Resize});
    // Offset grip: midpoint of dimension line
    QPointF p = perp();
    QPointF mid = (m_p1 + m_p2) / 2.0 + p * m_offsetDistance;
    handles.push_back({"offset", mid, HandleRole::Resize});
    return handles;
}

std::unique_ptr<Entity> LinearDimensionEntity::withDraggedHandle(
    const QString& handleId, const QPointF& worldPos) const
{
    auto copy = std::make_unique<LinearDimensionEntity>(m_p1, m_p2, m_offsetDistance);
    copy->setId(id());
    copy->setLayerId(layerId());

    if (handleId == "p1") {
        copy->m_p1 = worldPos;
    } else if (handleId == "p2") {
        copy->m_p2 = worldPos;
    } else if (handleId == "offset") {
        // Project worldPos onto the perp direction to get the new offset
        QPointF mid = (m_p1 + m_p2) / 2.0;
        QPointF p = perp();
        double newOffset = QPointF::dotProduct(worldPos - mid, p);
        // Keep a minimum offset so dimension is legible
        if (std::abs(newOffset) < 3.0)
            newOffset = newOffset < 0 ? -3.0 : 3.0;
        copy->m_offsetDistance = newOffset;
    }
    return copy;
}

std::vector<QPointF> LinearDimensionEntity::snapPoints() const
{
    return {m_p1, m_p2, (m_p1 + m_p2) / 2.0};
}

// -----------------------------------------------------------------------
// G-code: dimension annotations do not produce toolpath moves
// -----------------------------------------------------------------------

QList<GcodeMove> LinearDimensionEntity::toGcodeMoves(double, double, double) const
{
    return {};
}

// -----------------------------------------------------------------------
// Serialization
// -----------------------------------------------------------------------

QJsonObject LinearDimensionEntity::toJson() const
{
    QJsonObject json;
    json["type"]   = "dimension";
    json["x1"]     = m_p1.x();
    json["y1"]     = m_p1.y();
    json["x2"]     = m_p2.x();
    json["y2"]     = m_p2.y();
    json["offset"] = m_offsetDistance;
    return json;
}

std::unique_ptr<LinearDimensionEntity> LinearDimensionEntity::fromJson(const QJsonObject& json)
{
    QPointF p1(json["x1"].toDouble(), json["y1"].toDouble());
    QPointF p2(json["x2"].toDouble(), json["y2"].toDouble());
    double  offset = json["offset"].toDouble(10.0);
    return std::make_unique<LinearDimensionEntity>(p1, p2, offset);
}

QString LinearDimensionEntity::displayName() const
{
    return QString("Dimension (%1)").arg(QString::number(measuredLength(), 'f', 2));
}

TrimEnd LinearDimensionEntity::getTrimPoint(const QPointF& clickPos, const QPointF& intersection) const
{
    // Dimensions are not directly trimmable - return None
    return TrimEnd::None;
}

QPointF LinearDimensionEntity::pointAtTrimEnd(TrimEnd end) const
{
    return (end == TrimEnd::Start) ? m_p1 : m_p2;
}
