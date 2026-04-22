#include "TrimExtendTool.h"

#include "../Document.h"
#include "../Entity.h"
#include "../LineEntity.h"
#include "../ArcEntity.h"
#include "../CircleEntity.h"
#include "../CommandStack.h"

#include <QPainter>
#include <QLineF>
#include <algorithm>
#include <cmath>

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kEps = 1e-9;
constexpr double kClickRadius = 15.0;  // world units for hover detection

inline double degToRad(double d) { return d * kPi / 180.0; }
inline double normAngle(double a) {
    a = std::fmod(a, 360.0);
    return a < 0.0 ? a + 360.0 : a;
}

// Parameterise a line as P(t) = p1 + t*(p2-p1); returns sorted ts in (eps, 1-eps)
// for the intersection of segment [p1,p2] with an infinite line through (lineP, lineQ).
bool lineLineParam(const QPointF &p1, const QPointF &p2,
                   const QPointF &q1, const QPointF &q2,
                   double &t, double &u)
{
    double dx = p2.x() - p1.x(),  dy = p2.y() - p1.y();
    double ex = q2.x() - q1.x(),  ey = q2.y() - q1.y();
    double denom = dx * ey - dy * ex;
    if (std::abs(denom) < kEps) return false;   // parallel
    double fx = q1.x() - p1.x(), fy = q1.y() - p1.y();
    t = (fx * ey - fy * ex) / denom;
    u = (fx * dy - fy * dx) / denom;
    return true;
}

// Intersection parameters on line segment [p1,p2] with a circle (center, r).
// Returns parameters sorted ascending; only keeps values in (eps, 1-eps).
std::vector<double> lineCircleParams(const QPointF &p1, const QPointF &p2,
                                     const QPointF &center, double r)
{
    double dx = p2.x() - p1.x(), dy = p2.y() - p1.y();
    double fx = p1.x() - center.x(), fy = p1.y() - center.y();
    double a = dx*dx + dy*dy;
    if (a < kEps*kEps) return {};
    double b = 2.0 * (fx*dx + fy*dy);
    double c = fx*fx + fy*fy - r*r;
    double disc = b*b - 4.0*a*c;
    if (disc < 0.0) return {};
    double sq = std::sqrt(disc);
    std::vector<double> ts;
    for (double sign : {-1.0, 1.0}) {
        double t = (-b + sign*sq) / (2.0*a);
        if (t > kEps && t < 1.0 - kEps) ts.push_back(t);
    }
    std::sort(ts.begin(), ts.end());
    return ts;
}

// Check whether a point on a circle (given as angle in degrees) is within the arc
// [startDeg, startDeg+sweepDeg].
bool angleInArc(double angleDeg, double startDeg, double sweepDeg)
{
    double a = normAngle(angleDeg - startDeg);
    double s = std::abs(sweepDeg);
    return a <= s + kEps || a >= 360.0 - kEps;
}

// Compute the angle (degrees) of a point relative to a circle center
double pointAngle(const QPointF &pt, const QPointF &center) {
    return normAngle(std::atan2(pt.y() - center.y(), pt.x() - center.x()) * 180.0 / kPi);
}

// Returns cut parameters on entity at index `idx` with every other entity.
std::vector<double> cutsOnLine(const LineEntity *line, const Document &doc, int skip)
{
    std::vector<double> result;
    QPointF p1 = line->startPoint(), p2 = line->endPoint();

    for (int i = 0; i < doc.entityCount(); ++i) {
        if (i == skip) continue;
        const Entity *other = doc.entityAt(i);
        if (!other || !doc.isEntityVisible(other)) continue;

        if (other->kind() == Entity::Kind::Line) {
            const auto *ol = static_cast<const LineEntity*>(other);
            double t, u;
            if (lineLineParam(p1, p2, ol->startPoint(), ol->endPoint(), t, u)) {
                if (t > kEps && t < 1.0 - kEps && u > -kEps && u < 1.0 + kEps)
                    result.push_back(t);
            }
        } else if (other->kind() == Entity::Kind::Circle) {
            const auto *c = static_cast<const CircleEntity*>(other);
            for (double t : lineCircleParams(p1, p2, c->center(), c->radius()))
                result.push_back(t);
        } else if (other->kind() == Entity::Kind::Arc) {
            const auto *arc = static_cast<const ArcEntity*>(other);
            for (double t : lineCircleParams(p1, p2, arc->center(), arc->radius())) {
                QPointF hit = p1 + t * (p2 - p1);
                double ang = pointAngle(hit, arc->center());
                if (angleInArc(ang, arc->startAngleDegrees(), arc->sweepAngleDegrees()))
                    result.push_back(t);
            }
        }
    }
    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}

// Find the Line entity closest to worldPos, within kClickRadius.
int nearestLine(const QPointF &worldPos, const Document &doc, double &paramOut)
{
    int best = -1;
    double bestDist = kClickRadius;
    for (int i = 0; i < doc.entityCount(); ++i) {
        const Entity *e = doc.entityAt(i);
        if (!e || e->kind() != Entity::Kind::Line) continue;
        if (!doc.isEntityVisible(e)) continue;
        const auto *line = static_cast<const LineEntity*>(e);
        QLineF seg(line->startPoint(), line->endPoint());
        if (seg.length() < kEps) continue;
        // Project worldPos onto segment
        double dx = seg.dx(), dy = seg.dy();
        double t = ((worldPos.x() - seg.p1().x()) * dx + (worldPos.y() - seg.p1().y()) * dy)
                   / (dx*dx + dy*dy);
        t = std::clamp(t, 0.0, 1.0);
        QPointF proj = seg.p1() + QPointF(dx, dy) * t;
        double dist = QLineF(worldPos, proj).length();
        if (dist < bestDist) {
            bestDist = dist;
            best = i;
            paramOut = t;
        }
    }
    return best;
}

} // namespace

// ---------------------------------------------------------------------------

TrimExtendTool::TrimExtendTool(Document& doc, CommandStack& cs, Mode mode)
    : Tool(doc, cs)
    , m_mode(mode)
{
}

void TrimExtendTool::setMode(Mode mode)
{
    m_mode = mode;
    m_hoverIdx = -1;
}

QString TrimExtendTool::name() const
{
    return m_mode == Mode::Trim ? "Trim" : "Extend";
}

QString TrimExtendTool::instructions() const
{
    if (m_mode == Mode::Trim)
        return "Click the portion of a line to trim at intersections. Esc cancels.";
    return "Click near an endpoint of a line to extend it to the nearest intersection. Esc cancels.";
}

void TrimExtendTool::cancel()
{
    m_hoverIdx = -1;
}

// ---------------------------------------------------------------------------
// Mouse handling
// ---------------------------------------------------------------------------

void TrimExtendTool::mousePress(const QPointF& pos)
{
    if (m_mode == Mode::Trim) {
        if (m_hoverIdx < 0 || !m_hasLo || !m_hasHi) return;
        const Entity *e = document().entityAt(m_hoverIdx);
        if (!e || e->kind() != Entity::Kind::Line) return;
        const auto *line = static_cast<const LineEntity*>(e);

        // Build trimmed segment: keep [0, lo] and [hi, 1] — the clicked part [lo,hi] is removed.
        // We create only the surviving segment nearest to click.
        // Determine which sub-segment contains click param (m_hoverParam):
        // m_hoverParam is in (m_trimLo, m_trimHi) — we replace with the other sub-segment.
        // To keep it simple: if clicked segment is the whole line, do nothing.
        if (m_trimLo < kEps && m_trimHi > 1.0 - kEps) return;

        QPointF p1 = line->startPoint(), p2 = line->endPoint();
        QPointF cutLo = p1 + m_trimLo * (p2 - p1);
        QPointF cutHi = p1 + m_trimHi * (p2 - p1);

        // Replace this entity with two stubs (if both stubs have length > eps)
        std::vector<std::unique_ptr<Entity>> replacements;
        if (m_trimLo > kEps)
            replacements.push_back(std::make_unique<LineEntity>(p1, cutLo));
        if (m_trimHi < 1.0 - kEps)
            replacements.push_back(std::make_unique<LineEntity>(cutHi, p2));

        if (replacements.empty()) {
            // Delete the entity
            commandStack().execute(std::make_unique<DeleteEntityCommand>(m_hoverIdx));
        } else {
            // Replace entity with first stub; add second via AddEntityCommand
            commandStack().execute(std::make_unique<UpdateEntityCommand>(
                m_hoverIdx, line->clone(), std::move(replacements[0])));
            for (std::size_t i = 1; i < replacements.size(); ++i)
                commandStack().execute(std::make_unique<AddEntityCommand>(std::move(replacements[i])));
        }
        m_hoverIdx = -1;
    } else {
        // Extend
        if (m_hoverIdx < 0 || !m_canExtend) return;
        const Entity *e = document().entityAt(m_hoverIdx);
        if (!e || e->kind() != Entity::Kind::Line) return;
        const auto *line = static_cast<const LineEntity*>(e);
        QPointF p1 = line->startPoint(), p2 = line->endPoint();
        QPointF dir = p2 - p1;
        QPointF newPt = p1 + m_extendTarget * dir;
        std::unique_ptr<Entity> newLine;
        if (m_extendFromEnd)
            newLine = std::make_unique<LineEntity>(p1, newPt);
        else
            newLine = std::make_unique<LineEntity>(newPt, p2);
        commandStack().execute(std::make_unique<UpdateEntityCommand>(
            m_hoverIdx, line->clone(), std::move(newLine)));
        m_hoverIdx = -1;
    }
}

void TrimExtendTool::mouseMove(const QPointF& pos)
{
    updateHover(pos);
}

void TrimExtendTool::mouseRelease(const QPointF& pos)
{
    Q_UNUSED(pos)
}

// ---------------------------------------------------------------------------
// Hover computation
// ---------------------------------------------------------------------------

void TrimExtendTool::updateHover(const QPointF& worldPos)
{
    m_hasLo = m_hasHi = false;
    m_canExtend = false;

    double param = 0.0;
    m_hoverIdx = nearestLine(worldPos, document(), param);
    m_hoverParam = param;

    if (m_hoverIdx < 0) return;

    const Entity *e = document().entityAt(m_hoverIdx);
    const auto *line = static_cast<const LineEntity*>(e);

    auto cuts = cutsOnLine(line, document(), m_hoverIdx);

    if (m_mode == Mode::Trim) {
        // Find the two cut parameters bracketing m_hoverParam
        m_trimLo = 0.0;
        m_trimHi = 1.0;
        for (double t : cuts) {
            if (t < param) m_trimLo = t;
            else if (t > param && !m_hasHi) { m_trimHi = t; m_hasHi = true; }
        }
        m_hasLo = true;   // always have a lo boundary (may be 0)
        if (!m_hasHi) m_trimHi = 1.0;
    } else {
        // Extend: find nearest cut beyond endpoint
        bool nearEnd = (param > 0.5);
        double bestTarget = nearEnd ? std::numeric_limits<double>::max()
                                    : std::numeric_limits<double>::lowest();
        if (nearEnd) {
            // look for cuts > 1 (beyond end)
            for (double t : cutsOnLine(line, document(), m_hoverIdx)) {
                // t ∈ (0,1) won't help; we need to find cuttings outside using infinite-line intersections
                // stored params are clipped to (eps, 1-eps) — can't extend that way.
                // Instead compute on full infinite line:
                Q_UNUSED(t)
            }
            // Compute again without the (0,1) clamp:
            if (computeExtend(m_hoverIdx, worldPos)) m_canExtend = true;
        } else {
            if (computeExtend(m_hoverIdx, worldPos)) m_canExtend = true;
        }
    }
}

bool TrimExtendTool::computeExtend(int entityIdx, const QPointF& clickPos)
{
    const Entity *e = document().entityAt(entityIdx);
    const auto *line = static_cast<const LineEntity*>(e);
    QPointF p1 = line->startPoint(), p2 = line->endPoint();
    bool nearEnd = (QLineF(clickPos, p2).length() < QLineF(clickPos, p1).length());
    m_extendFromEnd = nearEnd;

    double best = nearEnd ? std::numeric_limits<double>::max()
                          : std::numeric_limits<double>::lowest();
    bool found = false;

    for (int i = 0; i < document().entityCount(); ++i) {
        if (i == entityIdx) continue;
        const Entity *other = document().entityAt(i);
        if (!other || !document().isEntityVisible(other)) continue;

        if (other->kind() == Entity::Kind::Line) {
            const auto *ol = static_cast<const LineEntity*>(other);
            double t, u;
            if (lineLineParam(p1, p2, ol->startPoint(), ol->endPoint(), t, u)) {
                if (nearEnd && t > 1.0 + kEps && t < best) { best = t; found = true; }
                else if (!nearEnd && t < -kEps && t > best) { best = t; found = true; }
            }
        } else if (other->kind() == Entity::Kind::Circle) {
            const auto *c = static_cast<const CircleEntity*>(other);
            // Compute line-circle on infinite line
            double dx = p2.x()-p1.x(), dy = p2.y()-p1.y();
            double fx = p1.x()-c->center().x(), fy = p1.y()-c->center().y();
            double a = dx*dx+dy*dy;
            if (a < kEps*kEps) continue;
            double b = 2.0*(fx*dx+fy*dy);
            double cc = fx*fx+fy*fy - c->radius()*c->radius();
            double disc = b*b-4.0*a*cc;
            if (disc < 0.0) continue;
            double sq = std::sqrt(disc);
            for (double sg : {-1.0, 1.0}) {
                double t = (-b + sg*sq) / (2.0*a);
                if (nearEnd && t > 1.0 + kEps && t < best) { best = t; found = true; }
                else if (!nearEnd && t < -kEps && t > best) { best = t; found = true; }
            }
        }
    }
    if (found) m_extendTarget = best;
    return found;
}

std::vector<double> TrimExtendTool::computeCuts(int entityIdx) const
{
    const Entity *e = document().entityAt(entityIdx);
    if (!e || e->kind() != Entity::Kind::Line) return {};
    return cutsOnLine(static_cast<const LineEntity*>(e), document(), entityIdx);
}

// ---------------------------------------------------------------------------
// Preview painting
// ---------------------------------------------------------------------------

void TrimExtendTool::paintPreview(QPainter& painter) const
{
    if (m_hoverIdx < 0) return;
    const Entity *e = document().entityAt(m_hoverIdx);
    if (!e || e->kind() != Entity::Kind::Line) return;
    const auto *line = static_cast<const LineEntity*>(e);
    QPointF p1 = line->startPoint(), p2 = line->endPoint();

    if (m_mode == Mode::Trim) {
        // Highlight the segment that would be removed in red dashes
        QPointF cutLo = p1 + m_trimLo * (p2 - p1);
        QPointF cutHi = p1 + m_trimHi * (p2 - p1);
        QPen pen(QColor(220, 50, 50), 2);
        pen.setStyle(Qt::DashLine);
        pen.setCosmetic(true);
        painter.save();
        painter.setPen(pen);
        painter.drawLine(cutLo, cutHi);
        painter.restore();
    } else if (m_canExtend) {
        // Show the extended portion in cyan dashes
        QPointF dir = p2 - p1;
        QPointF extPt = p1 + m_extendTarget * dir;
        QPointF anchor = m_extendFromEnd ? p2 : p1;
        QPen pen(QColor(0, 180, 220), 2);
        pen.setStyle(Qt::DashLine);
        pen.setCosmetic(true);
        painter.save();
        painter.setPen(pen);
        painter.drawLine(anchor, extPt);
        painter.restore();
    }
}
