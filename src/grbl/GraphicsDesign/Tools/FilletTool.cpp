#include "FilletTool.h"

#include "../Document.h"
#include "../Entity.h"
#include "../LineEntity.h"
#include "../ArcEntity.h"
#include "../CommandStack.h"

#include <QPainter>
#include <QLineF>
#include <cmath>
#include <algorithm>

namespace {

constexpr double kEps         = 1e-9;
constexpr double kClickRadius = 15.0;
constexpr double kPi          = 3.14159265358979323846;

inline double degToRad(double d) { return d * kPi / 180.0; }
inline double radToDeg(double r) { return r * 180.0 / kPi; }
inline double normAngle(double a)
{
    a = std::fmod(a, 360.0);
    return (a < 0.0) ? a + 360.0 : a;
}

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

// Distance from point to line segment, returns param t on [0,1].
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

// Unit vector perpendicular to (p2-p1), rotated 90° CCW.
QPointF perp(const QPointF &p1, const QPointF &p2)
{
    double dx = p2.x()-p1.x(), dy = p2.y()-p1.y();
    double len = std::sqrt(dx*dx + dy*dy);
    if (len < kEps) return {0.0, 0.0};
    return {-dy/len, dx/len};
}

// Foot of perpendicular from point P to infinite line through (a, b).
QPointF footOfPerpendicular(const QPointF &P, const QPointF &a, const QPointF &b)
{
    double dx = b.x()-a.x(), dy = b.y()-a.y();
    double len2 = dx*dx + dy*dy;
    if (len2 < kEps*kEps) return a;
    double t = ((P.x()-a.x())*dx + (P.y()-a.y())*dy) / len2;
    return a + QPointF(dx, dy)*t;
}

} // namespace

// ---------------------------------------------------------------------------

FilletTool::FilletTool(Document& doc, CommandStack& cs)
    : Tool(doc, cs)
{
}

void FilletTool::setFilletRadius(double r)
{
    if (r > kEps) m_radius = r;
}

QString FilletTool::name() const { return "Fillet"; }

QString FilletTool::instructions() const
{
    if (m_firstIdx < 0)
        return "Click first line for fillet. Esc cancels.";
    return "Click second line to complete fillet. Esc cancels.";
}

bool FilletTool::isActive() const { return m_firstIdx >= 0; }

void FilletTool::cancel()
{
    m_firstIdx     = -1;
    m_hoverIdx     = -1;
    m_previewValid = false;
}

// ---------------------------------------------------------------------------

int FilletTool::nearestLine(const QPointF& worldPos, double& paramOut) const
{
    int best = -1;
    double bestDist = kClickRadius;
    for (int i = 0; i < document().entityCount(); ++i) {
        if (m_firstIdx >= 0 && i == m_firstIdx) continue;
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
// Core fillet geometry
// ---------------------------------------------------------------------------

bool FilletTool::computeFillet(int firstIdx, int secondIdx)
{
    m_previewValid = false;
    if (firstIdx < 0 || secondIdx < 0) return false;

    const Entity *e1 = document().entityAt(firstIdx);
    const Entity *e2 = document().entityAt(secondIdx);
    if (!e1 || !e2 || e1->kind() != Entity::Kind::Line || e2->kind() != Entity::Kind::Line)
        return false;

    const auto *l1 = static_cast<const LineEntity*>(e1);
    const auto *l2 = static_cast<const LineEntity*>(e2);

    QPointF a1 = l1->startPoint(), a2 = l1->endPoint();
    QPointF b1 = l2->startPoint(), b2 = l2->endPoint();

    double t, u;
    if (!infiniteLineIntersect(a1, a2, b1, b2, t, u)) return false;  // parallel

    // The four possible fillet center positions are offset from each line's perpendicular
    // on either side. The correct center is the one closest to the corner
    // (i.e., the "inner" corner of the intersection angle).
    QPointF p1 = perp(a1, a2);
    QPointF p2 = perp(b1, b2);

    // Corner point
    QPointF corner = a1 + t * (a2 - a1);

    // Try all four combinations of ±p1, ±p2 offsets
    // The fillet center lies on the angle bisector at distance r / sin(half-angle)
    // Equivalently: offset each line inward by r and intersect.
    // We enumerate the 4 intersection combinations and pick the one closest to the corner.
    QPointF bestCenter;
    double  bestDist = std::numeric_limits<double>::max();
    bool    found    = false;

    for (int s1 : {-1, 1}) {
        for (int s2 : {-1, 1}) {
            // Offset line 1 inward
            QPointF oa1 = a1 + m_radius * double(s1) * p1;
            QPointF oa2 = a2 + m_radius * double(s1) * p1;
            // Offset line 2 inward
            QPointF ob1 = b1 + m_radius * double(s2) * p2;
            QPointF ob2 = b2 + m_radius * double(s2) * p2;

            double tc, uc;
            if (!infiniteLineIntersect(oa1, oa2, ob1, ob2, tc, uc)) continue;

            QPointF center = oa1 + tc * (oa2 - oa1);

            // Verify the actual distance to each original line is m_radius
            double d = 0.0;
            distToSegment(center, a1, a2, d);  // d is param; recompute distance manually
            // Use foot-of-perpendicular distance:
            QPointF foot1 = footOfPerpendicular(center, a1, a2);
            QPointF foot2 = footOfPerpendicular(center, b1, b2);
            double dist1 = QLineF(center, foot1).length();
            double dist2 = QLineF(center, foot2).length();

            if (std::abs(dist1 - m_radius) > 1.0 || std::abs(dist2 - m_radius) > 1.0) continue;

            // Pick candidate closest to corner
            double dcorner = QLineF(center, corner).length();
            if (dcorner < bestDist) {
                bestDist   = dcorner;
                bestCenter = center;
                found      = true;
            }
        }
    }

    if (!found) return false;

    // Tangent points = foot of perpendicular from center to each line
    m_tan1 = footOfPerpendicular(bestCenter, a1, a2);
    m_tan2 = footOfPerpendicular(bestCenter, b1, b2);
    m_arcCenter = bestCenter;
    m_arcRadius = m_radius;

    // Arc angles
    double ang1 = normAngle(radToDeg(std::atan2(m_tan1.y() - bestCenter.y(),
                                                 m_tan1.x() - bestCenter.x())));
    double ang2 = normAngle(radToDeg(std::atan2(m_tan2.y() - bestCenter.y(),
                                                 m_tan2.x() - bestCenter.x())));

    // Compute CCW sweep from ang1 to ang2
    double sweep = normAngle(ang2 - ang1);
    // Choose the shorter arc (< 180°). If sweep > 180, reverse.
    if (sweep > 180.0) {
        // Go CW instead — swap start/sweep
        std::swap(ang1, ang2);
        std::swap(m_tan1, m_tan2);
        sweep = 360.0 - sweep;
    }

    m_arcStart = ang1;
    m_arcSweep = sweep;
    m_previewValid = true;
    return true;
}

// ---------------------------------------------------------------------------

void FilletTool::mouseMove(const QPointF& pos)
{
    if (m_firstIdx >= 0) {
        double p = 0.0;
        m_hoverIdx = nearestLine(pos, p);
        m_hoverParam = p;
        computeFillet(m_firstIdx, m_hoverIdx);
    }
}

void FilletTool::mousePress(const QPointF& pos)
{
    if (m_firstIdx < 0) {
        double p = 0.0;
        int idx = nearestLine(pos, p);
        if (idx < 0) return;
        m_firstIdx = idx;
        m_firstParam = p;
    } else {
        double p = 0.0;
        int second = nearestLine(pos, p);
        if (second < 0) return;
        if (!computeFillet(m_firstIdx, second)) { cancel(); return; }

        const auto *l1 = static_cast<const LineEntity*>(document().entityAt(m_firstIdx));
        const auto *l2 = static_cast<const LineEntity*>(document().entityAt(second));
        QPointF a1 = l1->startPoint(), a2 = l1->endPoint();
        QPointF b1 = l2->startPoint(), b2 = l2->endPoint();

        // Find param of tan1 on l1 and tan2 on l2 for trimming
        double tTan1, tTan2;
        distToSegment(m_tan1, a1, a2, tTan1);
        distToSegment(m_tan2, b1, b2, tTan2);

        // Trim each line: keep the part away from the corner
        // Determine which end is closer to the corner (t > 0.5 → corner near end; else near start)
        double tCorner1, tCorner2;
        infiniteLineIntersect(a1, a2, b1, b2, tCorner1, tCorner2);

        auto trimLine = [](const LineEntity *l, double cornerT, const QPointF &tanPt)
            -> std::unique_ptr<LineEntity>
        {
            if (cornerT >= 0.5) {
                return std::make_unique<LineEntity>(l->startPoint(), tanPt);
            } else {
                return std::make_unique<LineEntity>(tanPt, l->endPoint());
            }
        };

        auto newL1 = trimLine(l1, tCorner1, m_tan1);
        auto newL2 = trimLine(l2, tCorner2, m_tan2);
        auto arc   = std::make_unique<ArcEntity>(m_arcCenter, m_arcRadius,
                                                  m_arcStart, m_arcSweep);

        commandStack().execute(std::make_unique<UpdateEntityCommand>(
            m_firstIdx, l1->clone(), std::move(newL1)));
        commandStack().execute(std::make_unique<UpdateEntityCommand>(
            second, l2->clone(), std::move(newL2)));
        commandStack().execute(std::make_unique<AddEntityCommand>(std::move(arc)));

        cancel();
    }
}

void FilletTool::mouseRelease(const QPointF& pos) { Q_UNUSED(pos) }

// ---------------------------------------------------------------------------

void FilletTool::paintPreview(QPainter& painter) const
{
    if (!m_previewValid) return;

    painter.save();
    QPen pen(QColor(0, 200, 120), 2);
    pen.setCosmetic(true);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);

    // Draw the fillet arc using QPainterPath so we can use world coords
    // QPainter::drawArc takes rect, startAngle in 1/16°, spanAngle in 1/16°
    // Qt arc angles: 0 = East, positive = CCW (same as our convention)
    QRectF arcRect(m_arcCenter.x() - m_arcRadius,
                   m_arcCenter.y() - m_arcRadius,
                   2.0 * m_arcRadius,
                   2.0 * m_arcRadius);
    painter.drawArc(arcRect,
                    static_cast<int>(m_arcStart * 16.0),
                    static_cast<int>(m_arcSweep * 16.0));

    // Tangent point markers
    painter.setBrush(QColor(0, 200, 120, 160));
    const double r = 4.0;
    painter.drawEllipse(m_tan1, r, r);
    painter.drawEllipse(m_tan2, r, r);
    painter.restore();
}
