#pragma once

#include "CoordinateTransform.h"

#include "../../grbl/GraphicsDesign/Document.h"

#include <QPointF>
#include <QString>
#include <optional>
#include <vector>

/// Snap computation and handle hit-testing extracted from DesignCanvas.
///
/// Stateless utility — no mutable state except configuration.
/// All computation goes through CoordinateTransform and Document references.
class CanvasSnap
{
public:
    struct SnapResult {
        bool active = false;
        QPointF worldPos;
        QString label;
    };

    CanvasSnap(const Document& document, const CoordinateTransform& transforms,
               double gridSize = 10.0, double snapDistancePx = 14.0);

    // --- Snap computation ---
    SnapResult computeSnap(const QPointF& rawWorldPos, bool includeSelection) const;

    // --- Handle hit-testing ---
    std::optional<Entity::EditHandle> hitHandle(
        const QPointF& worldPos,
        const std::vector<Entity::EditHandle>& handles) const;

    // --- Hover cursor helper ---
    /// Returns cursor shape hint: Qt::CrossCursor for rotate handles,
    /// Qt::SizeFDiagCursor for resize handles, 0 for no handle.
    int cursorForHandle(const Entity::EditHandle& handle) const;

    // --- Config ---
    void setGridSize(double size) { m_gridSize = size; }
    void setSnapDistancePx(double d) { m_snapDistancePx = d; }

    static constexpr double kHandleHitHalfSizePx = 10.0;

private:
    QPointF toScreen(const QPointF& worldPos) const;

    const Document& m_document;
    const CoordinateTransform& m_transforms;
    double m_gridSize = 10.0;
    double m_snapDistancePx = 14.0;
};
