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
