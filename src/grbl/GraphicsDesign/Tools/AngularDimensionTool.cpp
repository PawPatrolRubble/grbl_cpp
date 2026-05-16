#include "AngularDimensionTool.h"

#include "../AngularDimensionEntity.h"
#include "../LineEntity.h"
#include "../PolylineEntity.h"
#include "../RectangleEntity.h"

#include <QLineF>
#include <QPainter>
#include <QPointF>
#include <QUuid>
#include <algorithm>
#include <cmath>

namespace {
constexpr double kPickTolerance = 15.0;
constexpr double kMinSegmentLength = 1e-6;
constexpr double kPi = 3.14159265358979323846;

double crossProduct(const QPointF& a, const QPointF& b)
{
    return a.x() * b.y() - a.y() * b.x();
}

QPointF closestPointOnSegment(const QPointF& point, const QPointF& start, const QPointF& end)
{
    const QPointF segment = end - start;
    const double lengthSquared = segment.x() * segment.x() + segment.y() * segment.y();
    if (lengthSquared < kMinSegmentLength * kMinSegmentLength)
        return start;

    const QPointF pointDelta = point - start;
    const double t = std::clamp((pointDelta.x() * segment.x() + pointDelta.y() * segment.y()) / lengthSquared,
                                0.0, 1.0);
    return start + segment * t;
}

double pointToSegmentDistance(const QPointF& point, const QPointF& start, const QPointF& end)
{
    return QLineF(point, closestPointOnSegment(point, start, end)).length();
}

double segmentLength(const AngularDimensionTool::Segment& segment)
{
    return QLineF(segment.start, segment.end).length();
}

void considerSegment(const QPointF& pos,
                     const AngularDimensionTool::Segment& segment,
                     double& bestDistance,
                     std::optional<AngularDimensionTool::Segment>& bestSegment)
{
    if (segmentLength(segment) < kMinSegmentLength)
        return;

    const double distance = pointToSegmentDistance(pos, segment.start, segment.end);
    if (distance < bestDistance) {
        AngularDimensionTool::Segment pickedSegment = segment;
        pickedSegment.pickPoint = closestPointOnSegment(pos, segment.start, segment.end);
        pickedSegment.mousePoint = pos;
        pickedSegment.hasPickPoint = true;
        pickedSegment.hasMousePoint = true;
        bestDistance = distance;
        bestSegment = pickedSegment;
    }
}

std::optional<QPointF> infiniteLineIntersection(const AngularDimensionTool::Segment& firstSegment,
                                                const AngularDimensionTool::Segment& secondSegment)
{
    const QPointF firstDirection = firstSegment.end - firstSegment.start;
    const QPointF secondDirection = secondSegment.end - secondSegment.start;
    const double denominator = crossProduct(firstDirection, secondDirection);
    if (std::abs(denominator) < 1e-9)
        return std::nullopt;

    const double t = crossProduct(secondSegment.start - firstSegment.start, secondDirection) / denominator;
    return firstSegment.start + firstDirection * t;
}

QPointF directionPointForSegment(const AngularDimensionTool::Segment& segment, const QPointF& vertex)
{
    QPointF directionPoint = segment.hasMousePoint ? segment.mousePoint
                                                   : (segment.hasPickPoint ? segment.pickPoint
                                                                           : (segment.start + segment.end) * 0.5);
    if (QLineF(vertex, directionPoint).length() > kMinSegmentLength)
        return directionPoint;

    const double startDistance = QLineF(vertex, segment.start).length();
    const double endDistance = QLineF(vertex, segment.end).length();
    return startDistance >= endDistance ? segment.start : segment.end;
}

double ccwSweepDegrees(const QPointF& vertex, const QPointF& p1, const QPointF& p2)
{
    const double a1 = std::atan2(p1.y() - vertex.y(), p1.x() - vertex.x()) * 180.0 / kPi;
    const double a2 = std::atan2(p2.y() - vertex.y(), p2.x() - vertex.x()) * 180.0 / kPi;
    double sweep = std::fmod(a2 - a1, 360.0);
    if (sweep < 0.0)
        sweep += 360.0;
    return sweep;
}

struct IncludedSector {
    QPointF start;
    QPointF end;
};

IncludedSector includedSector(const QPointF& vertex, const QPointF& firstPoint, const QPointF& secondPoint)
{
    const double sweep = ccwSweepDegrees(vertex, firstPoint, secondPoint);
    if (sweep <= 180.0)
        return {firstPoint, secondPoint};

    return {secondPoint, firstPoint};
}

double annotationRadius(const QPointF& vertex, const QPointF& p1, const QPointF& p2)
{
    const double availableRadius = std::min(QLineF(vertex, p1).length(), QLineF(vertex, p2).length());
    return std::max(availableRadius, 3.0);
}

std::optional<AngularDimensionEntity> previewEntity(const AngularDimensionTool::Segment& firstSegment,
                                                    const AngularDimensionTool::Segment& secondSegment)
{
    const auto vertex = infiniteLineIntersection(firstSegment, secondSegment);
    if (!vertex)
        return std::nullopt;

    QPointF firstPoint = directionPointForSegment(firstSegment, *vertex);
    QPointF secondPoint = directionPointForSegment(secondSegment, *vertex);
    if (QLineF(*vertex, firstPoint).length() < kMinSegmentLength ||
        QLineF(*vertex, secondPoint).length() < kMinSegmentLength) {
        return std::nullopt;
    }

    const double sweep = ccwSweepDegrees(*vertex, firstPoint, secondPoint);
    if (sweep < 1e-6 || std::abs(sweep - 360.0) < 1e-6)
        return std::nullopt;

    const IncludedSector sector = includedSector(*vertex, firstPoint, secondPoint);
    return AngularDimensionEntity(*vertex, sector.start, sector.end,
                                  annotationRadius(*vertex, sector.start, sector.end));
}
} // namespace

AngularDimensionTool::AngularDimensionTool(Document& document, CommandStack& commandStack)
    : Tool(document, commandStack)
{
}

QString AngularDimensionTool::name() const
{
    return "Angular Dimension";
}

QString AngularDimensionTool::instructions() const
{
    if (!m_hasFirst) {
        if (m_hasResult) {
            return QString("Angle annotation created: %1°. Click the first segment for another angle.")
                .arg(m_angle, 0, 'f', 2);
        }
        return "Click the first line segment for the angle annotation.";
    }
    return "Click the second line segment to create the angle annotation.";
}

std::optional<AngularDimensionTool::Segment> AngularDimensionTool::pickSegment(const QPointF& pos) const
{
    double bestDistance = kPickTolerance;
    std::optional<Segment> bestSegment;

    for (int i = 0; i < document().entityCount(); ++i) {
        const Entity* entity = document().entityAt(i);
        if (!entity || !entity->isEnabled() || !document().isEntityVisible(entity) || document().isEntityLocked(entity))
            continue;

        if (entity->kind() == Entity::Kind::Line) {
            const auto* line = static_cast<const LineEntity*>(entity);
            considerSegment(pos, {line->startPoint(), line->endPoint()}, bestDistance, bestSegment);
        } else if (entity->kind() == Entity::Kind::Polygon) {
            const auto* polyline = static_cast<const PolylineEntity*>(entity);
            const auto& points = polyline->points();
            for (int pointIndex = 0; pointIndex + 1 < points.size(); ++pointIndex)
                considerSegment(pos, {points[pointIndex], points[pointIndex + 1]}, bestDistance, bestSegment);
        } else if (entity->kind() == Entity::Kind::Rectangle) {
            const auto* rectangle = static_cast<const RectangleEntity*>(entity);
            const auto corners = rectangle->corners();
            for (int cornerIndex = 0; cornerIndex < 4; ++cornerIndex)
                considerSegment(pos, {corners[cornerIndex], corners[(cornerIndex + 1) % 4]}, bestDistance, bestSegment);
        }
    }

    return bestSegment;
}

bool AngularDimensionTool::createAngleAnnotation(const Segment& secondSegment)
{
    const auto preview = previewEntity(m_firstSegment, secondSegment);
    if (!preview)
        return false;

    m_secondSegment = secondSegment;
    m_angle = preview->measuredAngleDegrees();
    m_hasResult = true;

    const auto annotationId = QUuid::createUuid();
    auto annotation = std::make_unique<AngularDimensionEntity>(
        preview->vertex(), preview->p1(), preview->p2(), preview->radius());
    annotation->setId(annotationId);

    commandStack().execute(std::make_unique<AddEntityCommand>(std::move(annotation)));
    document().clearSelection();
    document().select(annotationId);

    m_hasFirst = false;
    m_hasHover = false;
    return true;
}

void AngularDimensionTool::mousePress(const QPointF& pos)
{
    const auto segment = pickSegment(pos);
    if (!segment)
        return;

    if (!m_hasFirst) {
        m_firstSegment = *segment;
        m_secondSegment = {};
        m_hasFirst = true;
        m_hasResult = false;
        m_angle = 0.0;
        return;
    }

    createAngleAnnotation(*segment);
}

void AngularDimensionTool::mouseMove(const QPointF& pos)
{
    if (!m_hasFirst)
        return;

    const auto segment = pickSegment(pos);
    m_hasHover = segment.has_value();
    if (segment)
        m_hoverSegment = *segment;
}

void AngularDimensionTool::mouseRelease(const QPointF& pos)
{
    Q_UNUSED(pos)
}

void AngularDimensionTool::cancel()
{
    m_hasFirst = false;
    m_hasHover = false;
    m_hasResult = false;
    m_angle = 0.0;
}

bool AngularDimensionTool::isActive() const
{
    return m_hasFirst;
}

void AngularDimensionTool::paintPreview(QPainter& painter) const
{
    if (!m_hasFirst)
        return;

    painter.save();

    QPen firstPen(QColor(255, 200, 50), 1.5);
    firstPen.setCosmetic(true);
    firstPen.setStyle(Qt::DashLine);
    painter.setPen(firstPen);
    painter.drawLine(m_firstSegment.start, m_firstSegment.end);

    if (m_hasHover) {
        QPen hoverPen(QColor(80, 220, 255), 1.5);
        hoverPen.setCosmetic(true);
        hoverPen.setStyle(Qt::DashLine);
        painter.setPen(hoverPen);
        painter.drawLine(m_hoverSegment.start, m_hoverSegment.end);

        if (auto preview = previewEntity(m_firstSegment, m_hoverSegment)) {
            QPen previewPen(Qt::cyan);
            previewPen.setWidthF(0.5);
            previewPen.setStyle(Qt::DashLine);
            painter.setPen(previewPen);
            preview->draw(painter);
        }
    }

    const double r = 3.0;
    QPen dotPen(QColor(255, 200, 50), 2.0);
    dotPen.setCosmetic(true);
    painter.setPen(dotPen);
    painter.setBrush(QColor(255, 200, 50, 180));
    painter.drawEllipse(m_firstSegment.start, r, r);
    painter.drawEllipse(m_firstSegment.end, r, r);

    painter.restore();
}
