#include "OffsetTool.h"

#include "../Document.h"
#include "../Entity.h"
#include "../LineEntity.h"
#include "../CircleEntity.h"
#include "../ArcEntity.h"
#include "../RectangleEntity.h"
#include "../PolylineEntity.h"
#include "../CommandStack.h"

#include <QPainter>
#include <QLineF>
#include <cmath>
#include <algorithm>
#include <optional>

namespace {

constexpr double kPickRadius = 20.0;  // world units for entity picking

// Signed distance from point to infinite line through p1→p2 (positive = left of direction)
double signedDistToLine(const QPointF &p, const QPointF &p1, const QPointF &p2)
{
    QPointF d = p2 - p1;
    double len = std::hypot(d.x(), d.y());
    if (len < 1e-9) return 0.0;
    // Normal pointing left of direction: (-dy, dx) / len
    return ((-d.y()) * (p.x() - p1.x()) + d.x() * (p.y() - p1.y())) / len;
}

// Perpendicular unit normal of a line segment (pointing left of p1→p2)
QPointF lineNormal(const QPointF &p1, const QPointF &p2)
{
    QPointF d = p2 - p1;
    double len = std::hypot(d.x(), d.y());
    if (len < 1e-9) return QPointF(0, 1);
    return QPointF(-d.y() / len, d.x() / len);
}

// Calculate intersection of two lines defined by points and directions
// Returns intersection point, or false if parallel
std::optional<QPointF> intersectOffsetLines(const QPointF& p1, const QPointF& dir1,
                                            const QPointF& p2, const QPointF& dir2)
{
    // Solve: p1 + t*dir1 = p2 + s*dir2
    double denom = dir1.x() * dir2.y() - dir1.y() * dir2.x();
    if (std::abs(denom) < 1e-9) {
        return std::nullopt;  // Lines are parallel
    }
    QPointF diff = p2 - p1;
    double t = (diff.x() * dir2.y() - diff.y() * dir2.x()) / denom;
    return p1 + t * dir1;
}

} // namespace

OffsetTool::OffsetTool(Document& doc, CommandStack& cs)
    : Tool(doc, cs)
{}

void OffsetTool::setOffsetDistance(double d)
{
    m_distance = std::max(0.001, d);
}

QString OffsetTool::name() const { return "Offset"; }

QString OffsetTool::instructions() const
{
    return "Hover over an entity and click to create an offset copy on the cursor side. "
           "Set the offset distance in the toolbar.";
}

bool OffsetTool::isActive() const { return true; }

void OffsetTool::cancel()
{
    m_hoverIdx = -1;
    m_previewValid = false;
    m_previewEntity.reset();
}

// ── Nearest entity pick ──────────────────────────────────────────────────────

int OffsetTool::nearestEntity(const QPointF& worldPos) const
{
    int   best  = -1;
    double bestD = kPickRadius;
    for (int i = 0; i < document().entityCount(); ++i) {
        Entity *e = document().entityAt(i);
        if (!e || !document().isEntityVisible(e)) continue;
        // Only support offset for these entity types
        const auto k = e->kind();
        if (k != Entity::Kind::Line && k != Entity::Kind::Circle &&
            k != Entity::Kind::Arc  && k != Entity::Kind::Rectangle &&
            k != Entity::Kind::Polygon) continue;
        double d = e->distanceTo(worldPos);
        if (d < bestD) { bestD = d; best = i; }
    }
    return best;
}

// ── Build offset entity ──────────────────────────────────────────────────────

std::unique_ptr<Entity> OffsetTool::makeOffset(int idx, const QPointF& sidePos) const
{
    Entity *e = document().entityAt(idx);
    if (!e) return nullptr;

    const double d = m_distance;

    switch (e->kind()) {
    case Entity::Kind::Line: {
        const auto *ln = static_cast<const LineEntity*>(e);
        QPointF n = lineNormal(ln->startPoint(), ln->endPoint());
        // Determine sign: which side of the line is sidePos on?
        double sign = signedDistToLine(sidePos, ln->startPoint(), ln->endPoint()) >= 0 ? 1.0 : -1.0;
        QPointF offset = n * (sign * d);
        return std::make_unique<LineEntity>(ln->startPoint() + offset, ln->endPoint() + offset);
    }
    case Entity::Kind::Circle: {
        const auto *c = static_cast<const CircleEntity*>(e);
        // Offset outward if cursor is outside, inward if inside
        double cursorDist = QLineF(c->center(), sidePos).length();
        double newR = cursorDist > c->radius() ? c->radius() + d : c->radius() - d;
        if (newR <= 0.0) return nullptr;
        return std::make_unique<CircleEntity>(c->center(), newR);
    }
    case Entity::Kind::Arc: {
        const auto *arc = static_cast<const ArcEntity*>(e);
        double cursorDist = QLineF(arc->center(), sidePos).length();
        double newR = cursorDist > arc->radius() ? arc->radius() + d : arc->radius() - d;
        if (newR <= 0.0) return nullptr;
        return std::make_unique<ArcEntity>(arc->center(), newR,
                                           arc->startAngleDegrees(), arc->sweepAngleDegrees());
    }
    case Entity::Kind::Rectangle: {
        const auto *rect = static_cast<const RectangleEntity*>(e);
        // Offset outward if cursor is outside bb, inward otherwise
        bool outside = !rect->boundingRect().contains(sidePos);
        double sign = outside ? 1.0 : -1.0;
        double newW = rect->width()  + sign * 2.0 * d;
        double newH = rect->height() + sign * 2.0 * d;
        if (newW <= 0.0 || newH <= 0.0) return nullptr;
        return std::make_unique<RectangleEntity>(rect->center(), newW, newH, rect->angleDegrees());
    }
    case Entity::Kind::Polygon: {
        const auto *poly = static_cast<const PolylineEntity*>(e);
        const auto &pts = poly->points();
        if (pts.size() < 2) return nullptr;

        // Determine offset side by comparing click distance to centroid
        QPointF centroid;
        for (const auto &p : pts) centroid += p;
        centroid /= static_cast<double>(pts.size());

        // Use signed distance to first segment to determine side
        double sign = signedDistToLine(sidePos, pts[0], pts[1]) >= 0 ? 1.0 : -1.0;
        const double offsetDist = sign * d;

        QList<QPointF> newPts;
        newPts.reserve(pts.size());

        // Process each segment
        for (int i = 0; i < pts.size() - 1; ++i) {
            const QPointF& p1 = pts[i];
            const QPointF& p2 = pts[i + 1];

            // Calculate offset for this segment
            QPointF n = lineNormal(p1, p2);
            QPointF offsetVec = n * offsetDist;

            QPointF offsetP1 = p1 + offsetVec;
            QPointF offsetP2 = p2 + offsetVec;

            if (i == 0) {
                // First point: just add the offset start point
                newPts.append(offsetP1);
            }

            // For subsequent segments, calculate intersection with previous offset segment
            if (i > 0 && newPts.size() >= 2) {
                // Previous segment endpoints (original and offset)
                const QPointF& prevP1 = pts[i - 1];
                const QPointF& prevP2 = pts[i];

                // Calculate intersection of previous and current offset lines
                auto intersection = intersectOffsetLines(
                    newPts.last(),  // Previous offset end point
                    prevP2 - prevP1, // Previous segment direction
                    offsetP1,        // Current offset start point
                    p2 - p1          // Current segment direction
                );

                if (intersection) {
                    // Use intersection point as the corner point
                    newPts.append(*intersection);
                } else {
                    // Lines are parallel - add a bridging segment or just use offset points
                    newPts.append(offsetP1);
                }
            }

            // Add the end point of current segment
            if (i == pts.size() - 2) {
                // Last segment - add the offset end point
                newPts.append(offsetP2);
            } else {
                // Will be handled in next iteration's intersection calculation
            }
        }

        return std::make_unique<PolylineEntity>(newPts);
    }
    default:
        return nullptr;
    }
}

// ── Mouse events ─────────────────────────────────────────────────────────────

void OffsetTool::mouseMove(const QPointF& pos)
{
    m_mousePos = pos;
    m_hoverIdx = nearestEntity(pos);
    m_previewValid = false;
    m_previewEntity.reset();

    if (m_hoverIdx >= 0) {
        m_previewEntity = makeOffset(m_hoverIdx, pos);
        m_previewValid  = (m_previewEntity != nullptr);
    }
}

void OffsetTool::mousePress(const QPointF& pos)
{
    if (m_hoverIdx < 0 || !m_previewValid || !m_previewEntity) return;

    // Re-compute to get a clean copy for the command
    auto offsetEntity = makeOffset(m_hoverIdx, pos);
    if (!offsetEntity) return;
    offsetEntity->copyPlotMetadataFrom(*document().entityAt(m_hoverIdx));

    commandStack().execute(std::make_unique<AddEntityCommand>(std::move(offsetEntity)));
    // Keep hover active so user can immediately offset again
    mouseMove(pos);
}

void OffsetTool::mouseRelease(const QPointF& /*pos*/) {}

// ── Preview painting ─────────────────────────────────────────────────────────

void OffsetTool::paintPreview(QPainter& painter) const
{
    if (m_hoverIdx < 0) return;

    // Highlight hovered entity
    painter.save();
    QPen hoverPen(QColor(80, 160, 255), 2.5);
    hoverPen.setCosmetic(true);
    painter.setPen(hoverPen);
    Entity *hovered = document().entityAt(m_hoverIdx);
    if (hovered) hovered->draw(painter);

    // Draw preview offset copy
    if (m_previewValid && m_previewEntity) {
        QPen previewPen(QColor(100, 220, 100, 200), 2);
        previewPen.setStyle(Qt::DashLine);
        previewPen.setCosmetic(true);
        painter.setPen(previewPen);
        m_previewEntity->draw(painter);
    }
    painter.restore();
}
