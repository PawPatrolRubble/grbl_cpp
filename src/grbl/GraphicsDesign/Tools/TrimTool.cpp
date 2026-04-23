#include "TrimTool.h"

#include "../Document.h"
#include "../Entity.h"
#include "../LineEntity.h"
#include "../ArcEntity.h"
#include "../CircleEntity.h"
#include "../PolylineEntity.h"
#include "../RectangleEntity.h"
#include "../CommandStack.h"

#include <QPainter>
#include <QLineF>
#include <QRectF>
#include <algorithm>
#include <cmath>

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kEps = 1e-9;
constexpr double kClickRadius = 15.0;  // world units for hover detection

inline double degToRad(double d) { return d * kPi / 180.0; }
inline double radToDeg(double r) { return r * 180.0 / kPi; }
inline double normAngle(double a) {
    a = std::fmod(a, 360.0);
    return a < 0.0 ? a + 360.0 : a;
}

// Line-line intersection parameters
bool lineLineParams(const QPointF& p1, const QPointF& p2,
                    const QPointF& q1, const QPointF& q2,
                    double& t, double& u)
{
    double dx = p2.x() - p1.x(), dy = p2.y() - p1.y();
    double ex = q2.x() - q1.x(), ey = q2.y() - q1.y();
    double denom = dx * ey - dy * ex;
    if (std::abs(denom) < kEps) return false;  // parallel
    double fx = q1.x() - p1.x(), fy = q1.y() - p1.y();
    t = (fx * ey - fy * ex) / denom;
    u = (fx * dy - fy * dx) / denom;
    return true;
}

// Line-circle intersection parameters on infinite line
std::vector<double> lineCircleParams(const QPointF& p1, const QPointF& p2,
                                     const QPointF& center, double r)
{
    double dx = p2.x() - p1.x(), dy = p2.y() - p1.y();
    double fx = p1.x() - center.x(), fy = p1.y() - center.y();
    double a = dx * dx + dy * dy;
    if (a < kEps * kEps) return {};
    double b = 2.0 * (fx * dx + fy * dy);
    double c = fx * fx + fy * fy - r * r;
    double disc = b * b - 4.0 * a * c;
    if (disc < 0.0) return {};
    double sq = std::sqrt(disc);
    std::vector<double> ts;
    for (double sign : {-1.0, 1.0}) {
        double t = (-b + sign * sq) / (2.0 * a);
        ts.push_back(t);
    }
    std::sort(ts.begin(), ts.end());
    return ts;
}

// Check if angle is within arc span
bool angleInArc(double angleDeg, double startDeg, double sweepDeg)
{
    double a = normAngle(angleDeg - startDeg);
    double s = std::abs(sweepDeg);
    return a <= s + kEps || a >= 360.0 - kEps;
}

// Compute angle of point relative to center
double pointAngle(const QPointF& pt, const QPointF& center)
{
    return normAngle(std::atan2(pt.y() - center.y(), pt.x() - center.x()) * 180.0 / kPi);
}

// Distance from point to line segment
double distanceToSegment(const QPointF& pt, const QPointF& p1, const QPointF& p2)
{
    double dx = p2.x() - p1.x(), dy = p2.y() - p1.y();
    double lenSq = dx * dx + dy * dy;
    if (lenSq < kEps) return QLineF(pt, p1).length();
    
    double t = ((pt.x() - p1.x()) * dx + (pt.y() - p1.y()) * dy) / lenSq;
    t = std::clamp(t, 0.0, 1.0);
    QPointF proj(p1.x() + t * dx, p1.y() + t * dy);
    return QLineF(pt, proj).length();
}

// Check if line segment crosses another line segment (proper intersection)
bool segmentsCross(const QPointF& a1, const QPointF& a2,
                   const QPointF& b1, const QPointF& b2)
{
    double t, u;
    if (!lineLineParams(a1, a2, b1, b2, t, u)) return false;
    return t > kEps && t < 1.0 - kEps && u > kEps && u < 1.0 - kEps;
}

// Check if line segment crosses circle arc
bool segmentCrossesArc(const QPointF& p1, const QPointF& p2,
                       const QPointF& center, double radius,
                       double startDeg, double sweepDeg)
{
    auto ts = lineCircleParams(p1, p2, center, radius);
    for (double t : ts) {
        if (t > kEps && t < 1.0 - kEps) {
            QPointF hit(p1.x() + t * (p2.x() - p1.x()),
                        p1.y() + t * (p2.y() - p1.y()));
            double ang = pointAngle(hit, center);
            if (angleInArc(ang, startDeg, sweepDeg)) return true;
        }
    }
    return false;
}

// Check if line segment crosses circle (full circle)
bool segmentCrossesCircle(const QPointF& p1, const QPointF& p2,
                          const QPointF& center, double radius)
{
    auto ts = lineCircleParams(p1, p2, center, radius);
    for (double t : ts) {
        if (t > kEps && t < 1.0 - kEps) return true;
    }
    return false;
}

// Find nearest entity to point (lines, arcs)
int nearestEntity(const QPointF& worldPos, const Document& doc, double& paramOut, int& segmentOut)
{
    int best = -1;
    double bestDist = kClickRadius;
    paramOut = 0.0;
    segmentOut = 0;
    
    for (int i = 0; i < doc.entityCount(); ++i) {
        const Entity* e = doc.entityAt(i);
        if (!e || !doc.isEntityVisible(e)) continue;
        
        if (e->kind() == Entity::Kind::Line) {
            const auto* line = static_cast<const LineEntity*>(e);
            double t;
            double dist = distanceToSegment(worldPos, line->startPoint(), line->endPoint());
            QLineF seg(line->startPoint(), line->endPoint());
            double dx = seg.dx(), dy = seg.dy();
            t = ((worldPos.x() - seg.p1().x()) * dx + (worldPos.y() - seg.p1().y()) * dy)
                / (dx * dx + dy * dy);
            t = std::clamp(t, 0.0, 1.0);
            if (dist < bestDist) {
                bestDist = dist;
                best = i;
                paramOut = t;
                segmentOut = 0;
            }
        } else if (e->kind() == Entity::Kind::Arc) {
            const auto* arc = static_cast<const ArcEntity*>(e);
            // Check distance to arc
            double ang = pointAngle(worldPos, arc->center());
            if (angleInArc(ang, arc->startAngleDegrees(), arc->sweepAngleDegrees())) {
                double rDist = std::abs(QLineF(worldPos, arc->center()).length() - arc->radius());
                if (rDist < bestDist) {
                    bestDist = rDist;
                    best = i;
                    // Compute parameter along arc
                    double start = arc->startAngleDegrees();
                    double sweep = arc->sweepAngleDegrees();
                    double relAng = normAngle(ang - start);
                    if (relAng > sweep) relAng = sweep;
                    paramOut = relAng / sweep;  // 0 to 1 along arc
                    segmentOut = 0;
                }
            }
        } else if (e->kind() == Entity::Kind::Polygon) {
            const auto* poly = static_cast<const PolylineEntity*>(e);
            const auto& pts = poly->points();
            for (int seg = 0; seg + 1 < (int)pts.size(); ++seg) {
                double dist = distanceToSegment(worldPos, pts[seg], pts[seg + 1]);
                if (dist < bestDist) {
                    QLineF segLine(pts[seg], pts[seg + 1]);
                    double dx = segLine.dx(), dy = segLine.dy();
                    double t = ((worldPos.x() - pts[seg].x()) * dx + 
                                (worldPos.y() - pts[seg].y()) * dy)
                               / (dx * dx + dy * dy);
                    t = std::clamp(t, 0.0, 1.0);
                    bestDist = dist;
                    best = i;
                    // Parameter is normalized position along polyline
                    double totalLen = 0.0, partialLen = 0.0;
                    for (int j = 0; j + 1 < (int)pts.size(); ++j) {
                        double len = QLineF(pts[j], pts[j + 1]).length();
                        totalLen += len;
                        if (j < seg) partialLen += len;
                        else if (j == seg) partialLen += t * len;
                    }
                    paramOut = (totalLen > kEps) ? partialLen / totalLen : 0.0;
                    segmentOut = seg;
                }
            }
        }
    }
    return best;
}

} // namespace

// ---------------------------------------------------------------------------
// TrimTool implementation
// ---------------------------------------------------------------------------

TrimTool::TrimTool(Document& doc, CommandStack& cs)
    : Tool(doc, cs)
{
}

void TrimTool::setPhase(Phase phase)
{
    m_phase = phase;
    m_hoverEntityIdx = -1;
    m_fenceCrossedEntities.clear();
}

QString TrimTool::name() const
{
    return "Trim";
}

QString TrimTool::instructions() const
{
    if (m_phase == Phase::SelectCuttingEdges) {
        return "Select cutting edge entities. Enter or click empty space to finish.";
    }
    if (m_fenceMode) {
        return "Drag to draw fence crossing entities to trim. Esc to exit fence mode.";
    }
    return "Click portion of entity to trim. 'F' for fence mode. Esc to cancel.";
}

void TrimTool::cancel()
{
    if (m_phase == Phase::TrimObjects && m_fenceDrawing) {
        m_fenceDrawing = false;
        m_fenceCrossedEntities.clear();
    } else {
        m_cuttingEdges.clear();
        m_phase = Phase::SelectCuttingEdges;
        m_hoverEntityIdx = -1;
    }
}

bool TrimTool::isActive() const
{
    return m_fenceDrawing || m_phase == Phase::TrimObjects;
}

// ---------------------------------------------------------------------------
// Cutting edges management
// ---------------------------------------------------------------------------

void TrimTool::addCuttingEdge(const QUuid& id)
{
    m_cuttingEdges.insert(id);
}

void TrimTool::removeCuttingEdge(const QUuid& id)
{
    m_cuttingEdges.remove(id);
}

void TrimTool::clearCuttingEdges()
{
    m_cuttingEdges.clear();
}

void TrimTool::finishCuttingEdgeSelection()
{
    if (m_cuttingEdges.isEmpty()) {
        // If no cutting edges selected, use all entities as cutting edges
        for (int i = 0; i < document().entityCount(); ++i) {
            const Entity* e = document().entityAt(i);
            if (e) m_cuttingEdges.insert(e->id());
        }
    }
    m_phase = Phase::TrimObjects;
    m_hoverEntityIdx = -1;
}

// ---------------------------------------------------------------------------
// Mouse handling
// ---------------------------------------------------------------------------

void TrimTool::mousePress(const QPointF& pos)
{
    if (m_phase == Phase::SelectCuttingEdges) {
        // Find entity under cursor and add as cutting edge
        double param;
        int segment;
        int idx = nearestEntity(pos, document(), param, segment);
        if (idx >= 0) {
            const Entity* e = document().entityAt(idx);
            if (e) {
                if (m_cuttingEdges.contains(e->id())) {
                    removeCuttingEdge(e->id());  // Toggle off
                } else {
                    addCuttingEdge(e->id());      // Toggle on
                }
            }
        } else {
            // Click on empty space - finish cutting edge selection
            finishCuttingEdgeSelection();
        }
    } else if (m_phase == Phase::TrimObjects) {
        if (m_fenceMode) {
            // Start fence drawing
            m_fenceDrawing = true;
            m_fenceStart = pos;
            m_fenceEnd = pos;
            m_fenceCrossedEntities.clear();
        } else {
            // Single entity trim
            if (m_hoverEntityIdx >= 0 && m_hasTrimSegment) {
                trimEntity(m_hoverEntityIdx, m_hoverParam);
            }
        }
    }
}

void TrimTool::mouseMove(const QPointF& pos)
{
    if (m_phase == Phase::TrimObjects) {
        if (m_fenceDrawing) {
            updateFencePreview(pos);
        } else {
            updateHover(pos);
        }
    }
}

void TrimTool::mouseRelease(const QPointF& pos)
{
    if (m_fenceDrawing) {
        m_fenceEnd = pos;
        m_fenceDrawing = false;
        trimEntitiesByFence();
    }
}

// ---------------------------------------------------------------------------
// Hover and preview
// ---------------------------------------------------------------------------

void TrimTool::updateHover(const QPointF& worldPos)
{
    m_hoverEntityIdx = -1;
    m_hasTrimSegment = false;

    double param;
    int segment;
    m_hoverEntityIdx = nearestEntity(worldPos, document(), param, segment);
    m_hoverParam = param;

    if (m_hoverEntityIdx < 0) return;

    // Check if this entity is a cutting edge - cannot trim cutting edges
    const Entity* e = document().entityAt(m_hoverEntityIdx);
    if (e && m_cuttingEdges.contains(e->id())) {
        m_hoverEntityIdx = -1;
        return;
    }

    // Compute cut points on this entity from cutting edges
    auto cuts = computeCutsOnEntity(m_hoverEntityIdx);
    if (cuts.empty()) return;

    // Find segment to trim based on hover position
    m_trimLo = 0.0;
    m_trimHi = 1.0;
    for (double t : cuts) {
        if (t < param) m_trimLo = t;
        else if (t > param && !m_hasTrimSegment) { m_trimHi = t; break; }
    }
    m_hasTrimSegment = true;
}

void TrimTool::updateFencePreview(const QPointF& worldPos)
{
    m_fenceEnd = worldPos;
    m_fenceCrossedEntities = findEntitiesCrossedByFence(m_fenceStart, m_fenceEnd);
}

// ---------------------------------------------------------------------------
// Cut computation
// ---------------------------------------------------------------------------

std::vector<double> TrimTool::computeCutsOnEntity(int entityIdx) const
{
    std::vector<double> result;
    const Entity* e = document().entityAt(entityIdx);
    if (!e) return result;

    // Get entity geometry based on kind
    QPointF p1, p2;
    bool isLine = false;
    bool isArc = false;
    QPointF arcCenter;
    double arcRadius = 0.0;
    double arcStart = 0.0, arcSweep = 0.0;

    if (e->kind() == Entity::Kind::Line) {
        const auto* line = static_cast<const LineEntity*>(e);
        p1 = line->startPoint();
        p2 = line->endPoint();
        isLine = true;
    } else if (e->kind() == Entity::Kind::Arc) {
        const auto* arc = static_cast<const ArcEntity*>(e);
        arcCenter = arc->center();
        arcRadius = arc->radius();
        arcStart = arc->startAngleDegrees();
        arcSweep = arc->sweepAngleDegrees();
        isArc = true;
        // Create virtual endpoints for parameterization
        double startRad = degToRad(arcStart);
        double endRad = degToRad(arcStart + arcSweep);
        p1 = QPointF(arcCenter.x() + arcRadius * std::cos(startRad),
                     arcCenter.y() + arcRadius * std::sin(startRad));
        p2 = QPointF(arcCenter.x() + arcRadius * std::cos(endRad),
                     arcCenter.y() + arcRadius * std::sin(endRad));
    } else {
        return result;  // Unsupported entity type
    }

    // Compute intersections with each cutting edge
    for (const QUuid& edgeId : m_cuttingEdges) {
        int edgeIdx = document().indexOf(edgeId);
        if (edgeIdx < 0 || edgeIdx == entityIdx) continue;
        const Entity* edge = document().entityAt(edgeIdx);
        if (!edge || !document().isEntityVisible(edge)) continue;

        if (edge->kind() == Entity::Kind::Line) {
            const auto* edgeLine = static_cast<const LineEntity*>(edge);
            double t, u;
            if (lineLineParams(p1, p2, edgeLine->startPoint(), edgeLine->endPoint(), t, u)) {
                if (t > kEps && t < 1.0 - kEps) {
                    if (isArc) {
                        // Verify intersection is on arc segment
                        QPointF hit(p1.x() + t * (p2.x() - p1.x()),
                                    p1.y() + t * (p2.y() - p1.y()));
                        double ang = pointAngle(hit, arcCenter);
                        if (angleInArc(ang, arcStart, arcSweep)) {
                            result.push_back(t);
                        }
                    } else {
                        result.push_back(t);
                    }
                }
            }
        } else if (edge->kind() == Entity::Kind::Arc) {
            const auto* edgeArc = static_cast<const ArcEntity*>(edge);
            auto ts = lineCircleParams(p1, p2, edgeArc->center(), edgeArc->radius());
            for (double t : ts) {
                if (t > kEps && t < 1.0 - kEps) {
                    QPointF hit(p1.x() + t * (p2.x() - p1.x()),
                                p1.y() + t * (p2.y() - p1.y()));
                    // Check if on entity arc
                    if (isArc) {
                        double ang = pointAngle(hit, arcCenter);
                        if (!angleInArc(ang, arcStart, arcSweep)) continue;
                    }
                    // Check if on edge arc
                    double edgeAng = pointAngle(hit, edgeArc->center());
                    if (angleInArc(edgeAng, edgeArc->startAngleDegrees(), edgeArc->sweepAngleDegrees())) {
                        result.push_back(t);
                    }
                }
            }
        } else if (edge->kind() == Entity::Kind::Circle) {
            const auto* edgeCircle = static_cast<const CircleEntity*>(edge);
            auto ts = lineCircleParams(p1, p2, edgeCircle->center(), edgeCircle->radius());
            for (double t : ts) {
                if (t > kEps && t < 1.0 - kEps) {
                    if (isArc) {
                        QPointF hit(p1.x() + t * (p2.x() - p1.x()),
                                    p1.y() + t * (p2.y() - p1.y()));
                        double ang = pointAngle(hit, arcCenter);
                        if (!angleInArc(ang, arcStart, arcSweep)) continue;
                    }
                    result.push_back(t);
                }
            }
        }
    }

    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end(),
                             [](double a, double b) { return std::abs(a - b) < kEps; }),
                 result.end());
    return result;
}

std::vector<int> TrimTool::findEntitiesCrossedByFence(const QPointF& p1, const QPointF& p2) const
{
    std::vector<int> result;

    for (int i = 0; i < document().entityCount(); ++i) {
        const Entity* e = document().entityAt(i);
        if (!e || !document().isEntityVisible(e)) continue;
        if (m_cuttingEdges.contains(e->id())) continue;  // Skip cutting edges

        bool crosses = false;

        if (e->kind() == Entity::Kind::Line) {
            const auto* line = static_cast<const LineEntity*>(e);
            crosses = segmentsCross(p1, p2, line->startPoint(), line->endPoint());
        } else if (e->kind() == Entity::Kind::Arc) {
            const auto* arc = static_cast<const ArcEntity*>(e);
            crosses = segmentCrossesArc(p1, p2, arc->center(), arc->radius(),
                                        arc->startAngleDegrees(), arc->sweepAngleDegrees());
        } else if (e->kind() == Entity::Kind::Circle) {
            const auto* circle = static_cast<const CircleEntity*>(e);
            crosses = segmentCrossesCircle(p1, p2, circle->center(), circle->radius());
        } else if (e->kind() == Entity::Kind::Polygon) {
            const auto* poly = static_cast<const PolylineEntity*>(e);
            const auto& pts = poly->points();
            for (int seg = 0; seg + 1 < (int)pts.size(); ++seg) {
                if (segmentsCross(p1, p2, pts[seg], pts[seg + 1])) {
                    crosses = true;
                    break;
                }
            }
        } else if (e->kind() == Entity::Kind::Rectangle) {
            const auto* rect = static_cast<const RectangleEntity*>(e);
            auto snapPts = rect->snapPoints();  // Returns 4 corners + center
            for (int i = 0; i < 4; ++i) {
                if (segmentsCross(p1, p2, snapPts[i], snapPts[(i + 1) % 4])) {
                    crosses = true;
                    break;
                }
            }
        }

        if (crosses) {
            result.push_back(i);
        }
    }

    return result;
}

// ---------------------------------------------------------------------------
// Trim operations
// ---------------------------------------------------------------------------

void TrimTool::trimEntity(int entityIdx, double clickParam)
{
    const Entity* e = document().entityAt(entityIdx);
    if (!e) return;

    auto cuts = computeCutsOnEntity(entityIdx);
    if (cuts.empty()) return;

    // Determine trim segment based on click position
    double trimLo = 0.0, trimHi = 1.0;
    for (double t : cuts) {
        if (t < clickParam) trimLo = t;
        else if (t > clickParam) { trimHi = t; break; }
    }

    // Check if nothing to trim
    if (trimLo < kEps && trimHi > 1.0 - kEps) return;

    if (e->kind() == Entity::Kind::Line) {
        const auto* line = static_cast<const LineEntity*>(e);
        QPointF p1 = line->startPoint();
        QPointF p2 = line->endPoint();
        QPointF cutLoPt = p1 + trimLo * (p2 - p1);
        QPointF cutHiPt = p1 + trimHi * (p2 - p1);

        // Create remaining segments
        std::vector<std::unique_ptr<Entity>> replacements;
        if (trimLo > kEps) {
            replacements.push_back(std::make_unique<LineEntity>(p1, cutLoPt));
        }
        if (trimHi < 1.0 - kEps) {
            replacements.push_back(std::make_unique<LineEntity>(cutHiPt, p2));
        }

        if (replacements.empty()) {
            commandStack().execute(std::make_unique<DeleteEntityCommand>(entityIdx));
        } else {
            commandStack().execute(std::make_unique<UpdateEntityCommand>(
                entityIdx, line->clone(), std::move(replacements[0])));
            for (size_t i = 1; i < replacements.size(); ++i) {
                commandStack().execute(std::make_unique<AddEntityCommand>(std::move(replacements[i])));
            }
        }
    } else if (e->kind() == Entity::Kind::Arc) {
        const auto* arc = static_cast<const ArcEntity*>(e);
        // Compute new start/end angles based on trim parameters
        double startDeg = arc->startAngleDegrees();
        double sweepDeg = arc->sweepAngleDegrees();
        
        double newStart1 = startDeg;
        double newSweep1 = trimLo * sweepDeg;  // First segment
        
        double newStart2 = startDeg + trimHi * sweepDeg;
        double newSweep2 = (1.0 - trimHi) * sweepDeg;  // Second segment

        std::vector<std::unique_ptr<Entity>> replacements;
        if (trimLo > kEps && std::abs(newSweep1) > 0.5) {
            replacements.push_back(std::make_unique<ArcEntity>(
                arc->center(), arc->radius(), newStart1, newSweep1));
        }
        if (trimHi < 1.0 - kEps && std::abs(newSweep2) > 0.5) {
            replacements.push_back(std::make_unique<ArcEntity>(
                arc->center(), arc->radius(), newStart2, newSweep2));
        }

        if (replacements.empty()) {
            commandStack().execute(std::make_unique<DeleteEntityCommand>(entityIdx));
        } else {
            commandStack().execute(std::make_unique<UpdateEntityCommand>(
                entityIdx, arc->clone(), std::move(replacements[0])));
            for (size_t i = 1; i < replacements.size(); ++i) {
                commandStack().execute(std::make_unique<AddEntityCommand>(std::move(replacements[i])));
            }
        }
    }

    m_hoverEntityIdx = -1;
}

void TrimTool::trimEntitiesByFence()
{
    // For each entity crossed by fence, find where fence crosses it
    // and trim at that intersection point
    for (int entityIdx : m_fenceCrossedEntities) {
        const Entity* e = document().entityAt(entityIdx);
        if (!e) continue;

        // Find intersection of fence line with this entity
        double fenceCrossParam = 0.5;  // Default to middle
        QPointF entityP1, entityP2;
        bool isArc = false;
        QPointF arcCenter;
        double arcStart = 0.0, arcSweep = 0.0;

        if (e->kind() == Entity::Kind::Line) {
            const auto* line = static_cast<const LineEntity*>(e);
            entityP1 = line->startPoint();
            entityP2 = line->endPoint();
            double t, u;
            if (lineLineParams(m_fenceStart, m_fenceEnd, entityP1, entityP2, t, u)) {
                fenceCrossParam = u;
            }
        } else if (e->kind() == Entity::Kind::Arc) {
            const auto* arc = static_cast<const ArcEntity*>(e);
            arcCenter = arc->center();
            arcStart = arc->startAngleDegrees();
            arcSweep = arc->sweepAngleDegrees();
            isArc = true;
            // Find intersection parameter
            auto ts = lineCircleParams(m_fenceStart, m_fenceEnd, arcCenter, arc->radius());
            for (double t : ts) {
                QPointF hit(m_fenceStart.x() + t * (m_fenceEnd.x() - m_fenceStart.x()),
                            m_fenceStart.y() + t * (m_fenceEnd.y() - m_fenceStart.y()));
                double ang = pointAngle(hit, arcCenter);
                if (angleInArc(ang, arcStart, arcSweep)) {
                    fenceCrossParam = normAngle(ang - arcStart) / arcSweep;
                    break;
                }
            }
        } else {
            continue;  // Unsupported
        }

        trimEntity(entityIdx, fenceCrossParam);
    }

    m_fenceCrossedEntities.clear();
}

// ---------------------------------------------------------------------------
// Preview painting
// ---------------------------------------------------------------------------

void TrimTool::paintPreview(QPainter& painter) const
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);

    // Phase 1: Highlight cutting edges
    if (m_phase == Phase::SelectCuttingEdges) {
        QPen cuttingEdgePen(QColor(255, 165, 0), 3);  // Orange for cutting edges
        cuttingEdgePen.setCosmetic(true);
        painter.setPen(cuttingEdgePen);
        for (const QUuid& id : m_cuttingEdges) {
            int idx = document().indexOf(id);
            if (idx < 0) continue;
            const Entity* e = document().entityAt(idx);
            if (e) e->draw(painter);
        }
    }

    // Phase 2: Trim preview
    if (m_phase == Phase::TrimObjects) {
        // Show cutting edges in different color
        QPen cuttingEdgePen(QColor(255, 165, 0, 150), 2);
        cuttingEdgePen.setCosmetic(true);
        painter.setPen(cuttingEdgePen);
        for (const QUuid& id : m_cuttingEdges) {
            int idx = document().indexOf(id);
            if (idx < 0) continue;
            const Entity* e = document().entityAt(idx);
            if (e) e->draw(painter);
        }

        // Highlight segment to be trimmed
        if (!m_fenceDrawing && m_hoverEntityIdx >= 0 && m_hasTrimSegment) {
            const Entity* e = document().entityAt(m_hoverEntityIdx);
            if (e && e->kind() == Entity::Kind::Line) {
                const auto* line = static_cast<const LineEntity*>(e);
                QPointF p1 = line->startPoint();
                QPointF p2 = line->endPoint();
                QPointF cutLoPt = p1 + m_trimLo * (p2 - p1);
                QPointF cutHiPt = p1 + m_trimHi * (p2 - p1);

                QPen trimPen(QColor(220, 50, 50), 2);
                trimPen.setStyle(Qt::DashLine);
                trimPen.setCosmetic(true);
                painter.setPen(trimPen);
                painter.drawLine(cutLoPt, cutHiPt);
            }
        }

        // Fence mode preview
        if (m_fenceDrawing) {
            // Draw fence line
            QPen fencePen(QColor(255, 100, 100), 2);
            fencePen.setStyle(Qt::DashLine);
            fencePen.setCosmetic(true);
            painter.setPen(fencePen);
            painter.drawLine(m_fenceStart, m_fenceEnd);

            // Highlight entities crossed by fence
            QPen crossedPen(QColor(220, 50, 50), 2);
            crossedPen.setCosmetic(true);
            painter.setPen(crossedPen);
            for (int idx : m_fenceCrossedEntities) {
                const Entity* e = document().entityAt(idx);
                if (e) e->draw(painter);
            }
        }
    }

    painter.restore();
}