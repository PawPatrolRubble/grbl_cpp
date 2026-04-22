#include "ChamferTool.h"

#include "../Document.h"
#include "../Entity.h"
#include "../LineEntity.h"
#include "../CommandStack.h"

#include <QPainter>
#include <QLineF>
#include <cmath>
#include <algorithm>

namespace {

constexpr double kEps         = 1e-9;
constexpr double kClickRadius = 15.0;  // world units for line picking

// Solve for the intersection of two INFINITE lines p1→p2 and q1→q2.
// Returns true and sets t (param on first line) and u (param on second line).
bool infiniteLineIntersect(const QPointF &p1, const QPointF &p2,
                           const QPointF &q1, const QPointF &q2,
                           double &t, double &u)
{
    double dx = p2.x()-p1.x(), dy = p2.y()-p1.y();
    double ex = q2.x()-q1.x(), ey = q2.y()-q1.y();
    double denom = dx*ey - dy*ex;
    if (std::abs(denom) < kEps) return false;
    double fx = q1.x()-p1.x(), fy = q1.y()-p1.y();
    t = (fx*ey - fy*ex) / denom;
    u = (fx*dy - fy*dx) / denom;
    return true;
}

// Return the point on segment [p1,p2] at parameter t.
QPointF paramPoint(const QPointF &p1, const QPointF &p2, double t)
{
    return p1 + t * (p2 - p1);
}

// Distance from point to infinite line through p1→p2 clamped to segment.
double distToSegment(const QPointF &pt, const QPointF &p1, const QPointF &p2, double &t)
{
    double dx = p2.x()-p1.x(), dy = p2.y()-p1.y();
    double len2 = dx*dx + dy*dy;
    if (len2 < kEps*kEps) { t = 0.0; return QLineF(pt, p1).length(); }
    t = ((pt.x()-p1.x())*dx + (pt.y()-p1.y())*dy) / len2;
    t = std::clamp(t, 0.0, 1.0);
    QPointF proj = p1 + QPointF(dx, dy)*t;
    return QLineF(pt, proj).length();
}

} // namespace

// ---------------------------------------------------------------------------

ChamferTool::ChamferTool(Document& doc, CommandStack& cs)
    : Tool(doc, cs)
{
}

void ChamferTool::setChamferDistance(double d)
{
    if (d > kEps) m_chamferDist = d;
}

QString ChamferTool::name() const { return "Chamfer"; }

QString ChamferTool::instructions() const
{
    if (m_firstIdx < 0)
        return "Click first line for chamfer. Esc cancels.";
    return "Click second line to complete chamfer. Esc cancels.";
}

bool ChamferTool::isActive() const { return m_firstIdx >= 0; }

void ChamferTool::cancel()
{
    m_firstIdx    = -1;
    m_hoverIdx    = -1;
    m_previewValid = false;
}

// ---------------------------------------------------------------------------
// Nearest line helper
// ---------------------------------------------------------------------------

int ChamferTool::nearestLine(const QPointF& worldPos, double& paramOut) const
{
    int best = -1;
    double bestDist = kClickRadius;
    for (int i = 0; i < document().entityCount(); ++i) {
        if (m_firstIdx >= 0 && i == m_firstIdx) continue;   // don't re-pick the same line
        const Entity *e = document().entityAt(i);
        if (!e || e->kind() != Entity::Kind::Line) continue;
        if (!document().isEntityVisible(e)) continue;
        const auto *line = static_cast<const LineEntity*>(e);
        double t;
        double dist = distToSegment(worldPos, line->startPoint(), line->endPoint(), t);
        if (dist < bestDist) {
            bestDist = dist;
            best = i;
            paramOut = t;
        }
    }
    return best;
}

// ---------------------------------------------------------------------------
// Preview computation
// ---------------------------------------------------------------------------

void ChamferTool::updatePreview(int firstIdx, int secondIdx)
{
    m_previewValid = false;
    if (firstIdx < 0 || secondIdx < 0) return;

    const Entity *e1 = document().entityAt(firstIdx);
    const Entity *e2 = document().entityAt(secondIdx);
    if (!e1 || !e2) return;
    if (e1->kind() != Entity::Kind::Line || e2->kind() != Entity::Kind::Line) return;

    const auto *l1 = static_cast<const LineEntity*>(e1);
    const auto *l2 = static_cast<const LineEntity*>(e2);

    QPointF a1 = l1->startPoint(), a2 = l1->endPoint();
    QPointF b1 = l2->startPoint(), b2 = l2->endPoint();

    double t, u;
    if (!infiniteLineIntersect(a1, a2, b1, b2, t, u)) return;  // parallel

    m_corner = paramPoint(a1, a2, t);

    // Chamfer points: walk m_chamferDist from corner along each line direction
    // toward the segment (i.e., away from the intersection if it's outside [0,1]).
    double len1 = QLineF(a1, a2).length();
    double len2 = QLineF(b1, b2).length();
    if (len1 < kEps || len2 < kEps) return;

    // Unit vectors pointing from corner back along each line (into the segment)
    double t1dist = m_chamferDist / len1;  // parameter step for chamfer distance on line1
    double t2dist = m_chamferDist / len2;

    // The corner is at param t on line1 and u on line2.
    // We want to move toward the "meat" of the segment — away from the corner.
    // Heuristic: if t >= 0, move toward t=0 (direction a1), else toward t=1 (direction a2).
    double t1 = (t >= 0.0) ? (t - t1dist) : (t + t1dist);
    double u1 = (u >= 0.0) ? (u - t2dist) : (u + t2dist);

    // Clamp to segment to avoid going beyond endpoints
    t1 = std::clamp(t1, 0.0, 1.0);
    u1 = std::clamp(u1, 0.0, 1.0);

    m_chamferP1 = paramPoint(a1, a2, t1);
    m_chamferP2 = paramPoint(b1, b2, u1);
    m_previewValid = true;
}

// ---------------------------------------------------------------------------
// Mouse events
// ---------------------------------------------------------------------------

void ChamferTool::mouseMove(const QPointF& pos)
{
    if (m_firstIdx >= 0) {
        double p = 0.0;
        m_hoverIdx = nearestLine(pos, p);
        m_hoverParam = p;
        updatePreview(m_firstIdx, m_hoverIdx);
    }
}

void ChamferTool::mousePress(const QPointF& pos)
{
    if (m_firstIdx < 0) {
        // Pick first line
        double p = 0.0;
        int idx = nearestLine(pos, p);
        if (idx < 0) return;
        m_firstIdx = idx;
        m_firstParam = p;
    } else {
        // Pick second line → apply chamfer
        double p = 0.0;
        int second = nearestLine(pos, p);
        if (second < 0) return;
        updatePreview(m_firstIdx, second);
        if (!m_previewValid) { cancel(); return; }

        const auto *l1 = static_cast<const LineEntity*>(document().entityAt(m_firstIdx));
        const auto *l2 = static_cast<const LineEntity*>(document().entityAt(second));
        QPointF a1 = l1->startPoint(), a2 = l1->endPoint();
        QPointF b1 = l2->startPoint(), b2 = l2->endPoint();

        double t, u;
        infiniteLineIntersect(a1, a2, b1, b2, t, u);

        // Determine trimmed endpoints for each line (keep side away from corner)
        // For line1: trim toward corner (i.e., replace endpoint nearest corner with m_chamferP1)
        // The corner is at parameter t on line1. If t > 0.5 the far end is start; else far end is end.
        auto trimmedLine = [](const LineEntity *l, double cornerT, const QPointF &newCornerPt)
            -> std::unique_ptr<LineEntity>
        {
            if (cornerT >= 0.5) {
                // corner is near the end — keep start, replace end with chamfer point
                return std::make_unique<LineEntity>(l->startPoint(), newCornerPt);
            } else {
                // corner is near the start — replace start with chamfer point, keep end
                return std::make_unique<LineEntity>(newCornerPt, l->endPoint());
            }
        };

        auto newL1 = trimmedLine(l1, t, m_chamferP1);
        auto newL2 = trimmedLine(l2, u, m_chamferP2);
        auto chamferLine = std::make_unique<LineEntity>(m_chamferP1, m_chamferP2);

        commandStack().execute(std::make_unique<UpdateEntityCommand>(
            m_firstIdx, l1->clone(), std::move(newL1)));
        commandStack().execute(std::make_unique<UpdateEntityCommand>(
            second, l2->clone(), std::move(newL2)));
        commandStack().execute(std::make_unique<AddEntityCommand>(std::move(chamferLine)));

        cancel();
    }
}

void ChamferTool::mouseRelease(const QPointF& pos) { Q_UNUSED(pos) }

// ---------------------------------------------------------------------------
// Preview painting
// ---------------------------------------------------------------------------

void ChamferTool::paintPreview(QPainter& painter) const
{
    if (!m_previewValid) return;

    painter.save();
    // Draw the proposed chamfer line in cyan
    QPen pen(QColor(0, 180, 220), 2);
    pen.setCosmetic(true);
    painter.setPen(pen);
    painter.drawLine(m_chamferP1, m_chamferP2);

    // Draw small circles at chamfer points
    painter.setBrush(QColor(0, 180, 220, 160));
    const double r = 4.0;
    painter.drawEllipse(m_chamferP1, r, r);
    painter.drawEllipse(m_chamferP2, r, r);
    painter.restore();
}
