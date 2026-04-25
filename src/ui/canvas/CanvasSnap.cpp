#include "CanvasSnap.h"
#include "CoordinateTransform.h"
#include "../../grbl/GraphicsDesign/Entity.h"
#include "../../grbl/GraphicsDesign/LineEntity.h"
#include "../../grbl/GraphicsDesign/ArcEntity.h"
#include "../../grbl/GraphicsDesign/CircleEntity.h"
#include "../../grbl/GraphicsDesign/RectangleEntity.h"
#include "../../grbl/GraphicsDesign/PolylineEntity.h"

#include <QLineF>
#include <QRectF>
#include <QPointF>
#include <algorithm>
#include <cmath>

namespace {
constexpr double kPi = 3.14159265358979323846;

bool isRotateRole(Entity::HandleRole role) {
    return role == Entity::HandleRole::Rotate;
}

/// Project point onto line segment, return perpendicular point if within segment.
/// Returns nullopt if the perpendicular falls outside the segment bounds.
std::optional<QPointF> perpendicularPointOnLine(const QPointF& p, const QPointF& a, const QPointF& b) {
    const QPointF ab = b - a;
    const double len2 = ab.x() * ab.x() + ab.y() * ab.y();
    if (len2 < 1e-10) return std::nullopt; // Degenerate line
    
    const QPointF ap = p - a;
    const double t = (ap.x() * ab.x() + ap.y() * ab.y()) / len2;
    
    // Only snap if perpendicular falls within segment (not beyond endpoints)
    if (t > 0.05 && t < 0.95) {
        return a + t * ab;
    }
    return std::nullopt;
}

/// Nearest point on circle from given point (along line from center to point).
std::optional<QPointF> nearestPointOnCircle(const QPointF& p, const QPointF& center, double radius) {
    const QPointF delta = p - center;
    const double dist = std::hypot(delta.x(), delta.y());
    if (dist < 1e-10) return std::nullopt; // Point at center
    
    return center + radius * (delta / dist);
}

/// Nearest point on arc from given point, if point lies within arc sweep.
std::optional<QPointF> nearestPointOnArc(const QPointF& p, const QPointF& center, double radius,
                                          double startAngle, double sweepAngle) {
    // First compute nearest point on full circle
    const QPointF delta = p - center;
    const double dist = std::hypot(delta.x(), delta.y());
    if (dist < 1e-10) return std::nullopt;
    
    const double angle = std::atan2(delta.y(), delta.x()) * 180.0 / kPi;
    
    // Normalize angle to [0, 360)
    double normalizedAngle = angle;
    while (normalizedAngle < 0) normalizedAngle += 360.0;
    while (normalizedAngle >= 360.0) normalizedAngle -= 360.0;
    
    // Normalize start angle
    double normalizedStart = startAngle;
    while (normalizedStart < 0) normalizedStart += 360.0;
    while (normalizedStart >= 360.0) normalizedStart -= 360.0;
    
    // Compute end angle
    double normalizedEnd = normalizedStart + sweepAngle;
    
    // Check if angle is within arc sweep
    bool withinArc = false;
    if (sweepAngle >= 0) {
        withinArc = (normalizedAngle >= normalizedStart - 0.5 && normalizedAngle <= normalizedEnd + 0.5);
    } else {
        withinArc = (normalizedAngle >= normalizedEnd - 0.5 && normalizedAngle <= normalizedStart + 0.5);
    }
    
    if (withinArc) {
        return center + radius * (delta / dist);
    }
    return std::nullopt;
}

/// Line-Line intersection (segment intersection).
/// Returns intersection point if two segments intersect, nullopt otherwise.
std::optional<QPointF> lineLineIntersection(const QPointF& p1, const QPointF& p2,
                                             const QPointF& p3, const QPointF& p4) {
    // Use cross product approach
    const QPointF d1 = p2 - p1;
    const QPointF d2 = p4 - p3;
    const QPointF d3 = p3 - p1;
    
    const double cross1 = d1.x() * d2.y() - d1.y() * d2.x();
    if (std::abs(cross1) < 1e-10) return std::nullopt; // Lines are parallel
    
    const double t = (d3.x() * d2.y() - d3.y() * d2.x()) / cross1;
    const double u = (d3.x() * d1.y() - d3.y() * d1.x()) / cross1;
    
    // Check if intersection is within both segments
    if (t >= 0.0 && t <= 1.0 && u >= 0.0 && u <= 1.0) {
        return p1 + t * d1;
    }
    return std::nullopt;
}

/// Line-Circle intersection.
/// Returns up to 2 intersection points where line segment intersects circle.
std::vector<QPointF> lineCircleIntersection(const QPointF& p1, const QPointF& p2,
                                             const QPointF& center, double radius) {
    std::vector<QPointF> results;
    
    const QPointF d = p2 - p1;
    const QPointF f = p1 - center;
    
    const double a = d.x() * d.x() + d.y() * d.y();
    const double b = 2.0 * (f.x() * d.x() + f.y() * d.y());
    const double c = f.x() * f.x() + f.y() * f.y() - radius * radius;
    
    double discriminant = b * b - 4.0 * a * c;
    
    if (discriminant < 0.0) return results; // No intersection
    
    discriminant = std::sqrt(discriminant);
    
    // First intersection point
    const double t1 = (-b - discriminant) / (2.0 * a);
    if (t1 >= 0.0 && t1 <= 1.0) {
        results.push_back(p1 + t1 * d);
    }
    
    // Second intersection point (if not same as first)
    const double t2 = (-b + discriminant) / (2.0 * a);
    if (t2 >= 0.0 && t2 <= 1.0 && t2 != t1) {
        results.push_back(p1 + t2 * d);
    }
    
    return results;
}

/// Circle-Circle intersection.
/// Returns up to 2 intersection points where two circles intersect.
std::vector<QPointF> circleCircleIntersection(const QPointF& c1, double r1,
                                               const QPointF& c2, double r2) {
    std::vector<QPointF> results;
    
    const double d = std::hypot(c2.x() - c1.x(), c2.y() - c1.y());
    
    // No intersection cases
    if (d > r1 + r2 + 1e-10) return results; // Circles are separate
    if (d < std::abs(r1 - r2) - 1e-10) return results; // One circle inside another
    if (d < 1e-10 && std::abs(r1 - r2) < 1e-10) return results; // Coincident circles
    
    // Calculate intersection points
    const double a = (r1 * r1 - r2 * r2 + d * d) / (2.0 * d);
    const double h = std::sqrt(std::max(0.0, r1 * r1 - a * a));
    
    const QPointF p2 = c1 + a * (c2 - c1) / d;
    
    // Offset from p2 along perpendicular direction
    const QPointF offset = h * QPointF(-(c2.y() - c1.y()) / d, (c2.x() - c1.x()) / d);
    
    results.push_back(p2 + offset);
    if (h > 1e-10) {
        results.push_back(p2 - offset);
    }
    
    return results;
}

/// Check if a point is on an arc segment (within arc sweep).
bool isPointOnArc(const QPointF& p, const QPointF& center, double radius,
                  double startAngle, double sweepAngle) {
    // Check radius first
    const double dist = std::hypot(p.x() - center.x(), p.y() - center.y());
    if (std::abs(dist - radius) > 1e-10) return false;
    
    // Check angle
    double angle = std::atan2(p.y() - center.y(), p.x() - center.x()) * 180.0 / kPi;
    
    // Normalize angles to [0, 360)
    while (angle < 0) angle += 360.0;
    while (angle >= 360.0) angle -= 360.0;
    
    double normalizedStart = startAngle;
    while (normalizedStart < 0) normalizedStart += 360.0;
    while (normalizedStart >= 360.0) normalizedStart -= 360.0;
    
    double normalizedEnd = normalizedStart + sweepAngle;
    
    bool withinArc = false;
    if (sweepAngle >= 0) {
        withinArc = (angle >= normalizedStart - 0.5 && angle <= normalizedEnd + 0.5);
    } else {
        withinArc = (angle >= normalizedEnd - 0.5 && angle <= normalizedStart + 0.5);
    }
    
    return withinArc;
}
}

CanvasSnap::CanvasSnap(const Document& document, const CoordinateTransform& transforms,
                       double gridSize, double snapDistancePx)
    : m_document(document)
    , m_transforms(transforms)
    , m_gridSize(gridSize)
    , m_snapDistancePx(snapDistancePx)
{
}

QPointF CanvasSnap::toScreen(const QPointF& worldPos) const
{
    return QPointF(
        worldPos.x() * m_transforms.zoom() + m_transforms.panOffset().x(),
        -worldPos.y() * m_transforms.zoom() + m_transforms.panOffset().y()
    );
}

// ============================================================================
// Snap computation
// ============================================================================

CanvasSnap::SnapResult CanvasSnap::computeSnap(const QPointF& rawWorldPos, bool includeSelection) const
{
    SnapResult best;
    double bestScreenDistance = m_snapDistancePx;

    auto tryCandidate = [&](const QPointF& candidate, const QString& label) {
        const double screenDistance = QLineF(toScreen(rawWorldPos), toScreen(candidate)).length();
        if (screenDistance <= bestScreenDistance) {
            best.active = true;
            best.worldPos = candidate;
            best.label = label;
            bestScreenDistance = screenDistance;
        }
    };

    // Grid snap
    const QPointF gridPoint(std::round(rawWorldPos.x() / m_gridSize) * m_gridSize,
                            std::round(rawWorldPos.y() / m_gridSize) * m_gridSize);
    tryCandidate(gridPoint, "Grid");

    // Entity snap points
    const Entity* selected = m_document.selectedEntity();

    for (int i = 0; i < m_document.entityCount(); ++i) {
        Entity* entity = m_document.entityAt(i);
        if (!includeSelection && selected && entity->id() == selected->id()) {
            continue;
        }

        const auto kind = entity->kind();
        const auto points = entity->snapPoints();

        for (std::size_t pointIndex = 0; pointIndex < points.size(); ++pointIndex) {
            QString label = "Point";
            if (kind == Entity::Kind::Line) {
                label = pointIndex < 2 ? "Endpoint" : "Midpoint";
            } else if (kind == Entity::Kind::Rectangle) {
                // Indices: 0-3 corners, 4 center, 5-8 edge midpoints
                if (pointIndex < 4) label = "Corner";
                else if (pointIndex == 4) label = "Center";
                else label = "Midpoint";
            } else if (kind == Entity::Kind::Circle) {
                label = pointIndex == 0 ? "Center" : "Quadrant";
            } else if (kind == Entity::Kind::Arc) {
                // Indices: 0 center, 1 start, 2 end, 3 midpoint
                if (pointIndex == 0) label = "Center";
                else if (pointIndex == 1) label = "Start";
                else if (pointIndex == 2) label = "End";
                else label = "Midpoint";
            } else if (kind == Entity::Kind::Polygon) {
                // Polyline: vertices first, then segment midpoints
                // Total size S = vertices V + (V-1) = 2V - 1, so V = (S+1)/2
                const std::size_t total = points.size();
                const std::size_t vertexCount = (total + 1) / 2;
                label = pointIndex < vertexCount ? "Endpoint" : "Midpoint";
            }
            tryCandidate(points[pointIndex], label);
        }
    }

    // Perpendicular/nearest snap points
    for (int i = 0; i < m_document.entityCount(); ++i) {
        Entity* entity = m_document.entityAt(i);
        if (!includeSelection && selected && entity->id() == selected->id()) {
            continue;
        }

        const auto kind = entity->kind();

        // LineEntity: perpendicular to line segment
        if (kind == Entity::Kind::Line) {
            auto* line = static_cast<LineEntity*>(entity);
            auto perp = perpendicularPointOnLine(rawWorldPos, line->startPoint(), line->endPoint());
            if (perp) tryCandidate(*perp, "Perpendicular");
        }

        // ArcEntity: nearest point on arc (if within arc sweep)
        if (kind == Entity::Kind::Arc) {
            auto* arc = static_cast<ArcEntity*>(entity);
            auto nearest = nearestPointOnArc(rawWorldPos, arc->center(), arc->radius(),
                                             arc->startAngleDegrees(), arc->sweepAngleDegrees());
            if (nearest) tryCandidate(*nearest, "Nearest");
        }

        // CircleEntity: nearest point on circle
        if (kind == Entity::Kind::Circle) {
            auto* circle = static_cast<CircleEntity*>(entity);
            auto nearest = nearestPointOnCircle(rawWorldPos, circle->center(), circle->radius());
            if (nearest) tryCandidate(*nearest, "Nearest");
        }

        // RectangleEntity: perpendicular to each edge
        if (kind == Entity::Kind::Rectangle) {
            auto* rect = static_cast<RectangleEntity*>(entity);
            const auto c = rect->corners();
            // Edges: c[0]-c[1], c[1]-c[2], c[2]-c[3], c[3]-c[0]
            auto perp0 = perpendicularPointOnLine(rawWorldPos, c[0], c[1]);
            if (perp0) tryCandidate(*perp0, "Perpendicular");
            auto perp1 = perpendicularPointOnLine(rawWorldPos, c[1], c[2]);
            if (perp1) tryCandidate(*perp1, "Perpendicular");
            auto perp2 = perpendicularPointOnLine(rawWorldPos, c[2], c[3]);
            if (perp2) tryCandidate(*perp2, "Perpendicular");
            auto perp3 = perpendicularPointOnLine(rawWorldPos, c[3], c[0]);
            if (perp3) tryCandidate(*perp3, "Perpendicular");
        }

        // PolylineEntity: perpendicular to each segment
        if (kind == Entity::Kind::Polygon) {
            auto* poly = static_cast<PolylineEntity*>(entity);
            const auto& pts = poly->points();
            for (int seg = 0; seg + 1 < pts.size(); ++seg) {
                auto perp = perpendicularPointOnLine(rawWorldPos, pts[seg], pts[seg + 1]);
                if (perp) tryCandidate(*perp, "Perpendicular");
            }
        }
    }

    // Intersection snap points (between pairs of entities)
    for (int i = 0; i < m_document.entityCount(); ++i) {
        Entity* entity1 = m_document.entityAt(i);
        if (!includeSelection && selected && entity1->id() == selected->id()) {
            continue;
        }
        const auto kind1 = entity1->kind();

        for (int j = i + 1; j < m_document.entityCount(); ++j) {
            Entity* entity2 = m_document.entityAt(j);
            if (!includeSelection && selected && entity2->id() == selected->id()) {
                continue;
            }
            const auto kind2 = entity2->kind();

            // Line-Line intersection
            if (kind1 == Entity::Kind::Line && kind2 == Entity::Kind::Line) {
                auto* line1 = static_cast<LineEntity*>(entity1);
                auto* line2 = static_cast<LineEntity*>(entity2);
                auto inter = lineLineIntersection(line1->startPoint(), line1->endPoint(),
                                                  line2->startPoint(), line2->endPoint());
                if (inter) tryCandidate(*inter, "Intersection");
            }

            // Line-Circle intersection
            if (kind1 == Entity::Kind::Line && kind2 == Entity::Kind::Circle) {
                auto* line = static_cast<LineEntity*>(entity1);
                auto* circle = static_cast<CircleEntity*>(entity2);
                auto inters = lineCircleIntersection(line->startPoint(), line->endPoint(),
                                                     circle->center(), circle->radius());
                for (const auto& pt : inters) tryCandidate(pt, "Intersection");
            }
            if (kind2 == Entity::Kind::Line && kind1 == Entity::Kind::Circle) {
                auto* line = static_cast<LineEntity*>(entity2);
                auto* circle = static_cast<CircleEntity*>(entity1);
                auto inters = lineCircleIntersection(line->startPoint(), line->endPoint(),
                                                     circle->center(), circle->radius());
                for (const auto& pt : inters) tryCandidate(pt, "Intersection");
            }

            // Line-Arc intersection
            if (kind1 == Entity::Kind::Line && kind2 == Entity::Kind::Arc) {
                auto* line = static_cast<LineEntity*>(entity1);
                auto* arc = static_cast<ArcEntity*>(entity2);
                auto inters = lineCircleIntersection(line->startPoint(), line->endPoint(),
                                                     arc->center(), arc->radius());
                for (const auto& pt : inters) {
                    if (isPointOnArc(pt, arc->center(), arc->radius(),
                                     arc->startAngleDegrees(), arc->sweepAngleDegrees())) {
                        tryCandidate(pt, "Intersection");
                    }
                }
            }
            if (kind2 == Entity::Kind::Line && kind1 == Entity::Kind::Arc) {
                auto* line = static_cast<LineEntity*>(entity2);
                auto* arc = static_cast<ArcEntity*>(entity1);
                auto inters = lineCircleIntersection(line->startPoint(), line->endPoint(),
                                                     arc->center(), arc->radius());
                for (const auto& pt : inters) {
                    if (isPointOnArc(pt, arc->center(), arc->radius(),
                                     arc->startAngleDegrees(), arc->sweepAngleDegrees())) {
                        tryCandidate(pt, "Intersection");
                    }
                }
            }

            // Circle-Circle intersection
            if (kind1 == Entity::Kind::Circle && kind2 == Entity::Kind::Circle) {
                auto* circle1 = static_cast<CircleEntity*>(entity1);
                auto* circle2 = static_cast<CircleEntity*>(entity2);
                auto inters = circleCircleIntersection(circle1->center(), circle1->radius(),
                                                       circle2->center(), circle2->radius());
                for (const auto& pt : inters) tryCandidate(pt, "Intersection");
            }

            // Circle-Arc intersection
            if (kind1 == Entity::Kind::Circle && kind2 == Entity::Kind::Arc) {
                auto* circle = static_cast<CircleEntity*>(entity1);
                auto* arc = static_cast<ArcEntity*>(entity2);
                auto inters = circleCircleIntersection(circle->center(), circle->radius(),
                                                       arc->center(), arc->radius());
                for (const auto& pt : inters) {
                    if (isPointOnArc(pt, arc->center(), arc->radius(),
                                     arc->startAngleDegrees(), arc->sweepAngleDegrees())) {
                        tryCandidate(pt, "Intersection");
                    }
                }
            }
            if (kind2 == Entity::Kind::Circle && kind1 == Entity::Kind::Arc) {
                auto* circle = static_cast<CircleEntity*>(entity2);
                auto* arc = static_cast<ArcEntity*>(entity1);
                auto inters = circleCircleIntersection(circle->center(), circle->radius(),
                                                       arc->center(), arc->radius());
                for (const auto& pt : inters) {
                    if (isPointOnArc(pt, arc->center(), arc->radius(),
                                     arc->startAngleDegrees(), arc->sweepAngleDegrees())) {
                        tryCandidate(pt, "Intersection");
                    }
                }
            }

            // Arc-Arc intersection
            if (kind1 == Entity::Kind::Arc && kind2 == Entity::Kind::Arc) {
                auto* arc1 = static_cast<ArcEntity*>(entity1);
                auto* arc2 = static_cast<ArcEntity*>(entity2);
                auto inters = circleCircleIntersection(arc1->center(), arc1->radius(),
                                                       arc2->center(), arc2->radius());
                for (const auto& pt : inters) {
                    if (isPointOnArc(pt, arc1->center(), arc1->radius(),
                                     arc1->startAngleDegrees(), arc1->sweepAngleDegrees()) &&
                        isPointOnArc(pt, arc2->center(), arc2->radius(),
                                     arc2->startAngleDegrees(), arc2->sweepAngleDegrees())) {
                        tryCandidate(pt, "Intersection");
                    }
                }
            }

            // Line-Rectangle edges intersection
            if (kind1 == Entity::Kind::Line && kind2 == Entity::Kind::Rectangle) {
                auto* line = static_cast<LineEntity*>(entity1);
                auto* rect = static_cast<RectangleEntity*>(entity2);
                const auto c = rect->corners();
                // Edges: c[0]-c[1], c[1]-c[2], c[2]-c[3], c[3]-c[0]
                for (int edge = 0; edge < 4; ++edge) {
                    auto inter = lineLineIntersection(line->startPoint(), line->endPoint(),
                                                      c[edge], c[(edge + 1) % 4]);
                    if (inter) tryCandidate(*inter, "Intersection");
                }
            }
            if (kind2 == Entity::Kind::Line && kind1 == Entity::Kind::Rectangle) {
                auto* line = static_cast<LineEntity*>(entity2);
                auto* rect = static_cast<RectangleEntity*>(entity1);
                const auto c = rect->corners();
                for (int edge = 0; edge < 4; ++edge) {
                    auto inter = lineLineIntersection(line->startPoint(), line->endPoint(),
                                                      c[edge], c[(edge + 1) % 4]);
                    if (inter) tryCandidate(*inter, "Intersection");
                }
            }

            // Line-Polyline segments intersection
            if (kind1 == Entity::Kind::Line && kind2 == Entity::Kind::Polygon) {
                auto* line = static_cast<LineEntity*>(entity1);
                auto* poly = static_cast<PolylineEntity*>(entity2);
                const auto& pts = poly->points();
                for (int seg = 0; seg + 1 < pts.size(); ++seg) {
                    auto inter = lineLineIntersection(line->startPoint(), line->endPoint(),
                                                      pts[seg], pts[seg + 1]);
                    if (inter) tryCandidate(*inter, "Intersection");
                }
            }
            if (kind2 == Entity::Kind::Line && kind1 == Entity::Kind::Polygon) {
                auto* line = static_cast<LineEntity*>(entity2);
                auto* poly = static_cast<PolylineEntity*>(entity1);
                const auto& pts = poly->points();
                for (int seg = 0; seg + 1 < pts.size(); ++seg) {
                    auto inter = lineLineIntersection(line->startPoint(), line->endPoint(),
                                                      pts[seg], pts[seg + 1]);
                    if (inter) tryCandidate(*inter, "Intersection");
                }
            }
        }
    }

    return best;
}

// ============================================================================
// Handle hit-testing
// ============================================================================

std::optional<Entity::EditHandle> CanvasSnap::hitHandle(
    const QPointF& worldPos,
    const std::vector<Entity::EditHandle>& handles) const
{
    const QPointF screenPos = toScreen(worldPos);
    for (const auto& handle : handles) {
        const QPointF handleScreen = toScreen(handle.worldPos);
        QRectF hitRect(handleScreen.x() - kHandleHitHalfSizePx,
                       handleScreen.y() - kHandleHitHalfSizePx,
                       kHandleHitHalfSizePx * 2.0,
                       kHandleHitHalfSizePx * 2.0);
        if (hitRect.contains(screenPos)) {
            return handle;
        }
    }
    return std::nullopt;
}

int CanvasSnap::cursorForHandle(const Entity::EditHandle& handle) const
{
    if (isRotateRole(handle.role)) {
        return Qt::CrossCursor;
    }
    return Qt::SizeFDiagCursor;
}
