#include "CanvasSnap.h"
#include "CoordinateTransform.h"
#include "../../grbl/GraphicsDesign/Entity.h"

#include <QLineF>
#include <QRectF>
#include <QPointF>
#include <algorithm>
#include <cmath>

namespace {
bool isRotateRole(Entity::HandleRole role) {
    return role == Entity::HandleRole::Rotate;
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
                label = pointIndex < 4 ? "Corner" : "Center";
            } else if (kind == Entity::Kind::Circle) {
                label = pointIndex == 0 ? "Center" : "Quadrant";
            } else if (kind == Entity::Kind::Arc) {
                label = pointIndex == 0 ? "Center" : (pointIndex == 1 ? "Start" : "End");
            }
            tryCandidate(points[pointIndex], label);
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
