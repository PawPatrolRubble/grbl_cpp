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
        // Simple per-segment offset: move each point by average of adjacent segment normals
        // Determine side sign using centroid vs sidePos
        QPointF centroid;
        for (const auto &p : pts) centroid += p;
        centroid /= static_cast<double>(pts.size());
        double sign = QLineF(centroid, sidePos).length() > QLineF(centroid, pts[0]).length() ? 1.0 : -1.0;

        QList<QPointF> newPts;
        newPts.reserve(pts.size());
        for (int i = 0; i < pts.size(); ++i) {
            // Average normal of surrounding segments
            QPointF n(0, 0);
            int count = 0;
            if (i > 0) {
                n += lineNormal(pts[i-1], pts[i]);
                ++count;
            }
            if (i < pts.size() - 1) {
                n += lineNormal(pts[i], pts[i+1]);
                ++count;
            }
            if (count > 0) n /= static_cast<double>(count);
            else n = QPointF(0, 1);
            double len = std::hypot(n.x(), n.y());
            if (len > 1e-9) n /= len;
            newPts.append(pts[i] + n * (sign * d));
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
