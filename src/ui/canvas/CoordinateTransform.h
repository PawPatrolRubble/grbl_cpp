#pragma once

#include <QPointF>
#include <QSize>
#include <QRectF>
#include <cmath>
#include <algorithm>

/// Handles all coordinate conversions between screen, world (machine),
/// and manages pan/zoom state for the canvas.
///
/// Screen: Y-down, pixel coordinates (Qt widget space)
/// Machine: Y-up, real-world coordinates (mm)
class CoordinateTransform
{
public:
    CoordinateTransform() = default;

    // --- Screen ↔ Machine (Y-up) ---

    QPointF toWorld(const QPointF& screenPos) const {
        return QPointF(
            (screenPos.x() - m_panOffset.x()) / m_zoom,
            -(screenPos.y() - m_panOffset.y()) / m_zoom
        );
    }

    QPointF toScreen(const QPointF& worldPos) const {
        return QPointF(
            worldPos.x() * m_zoom + m_panOffset.x(),
            -worldPos.y() * m_zoom + m_panOffset.y()
        );
    }

    QPointF machineToScreen(const QPointF& pt) const {
        return QPointF(
            pt.x() * m_zoom + m_panOffset.x(),
            -pt.y() * m_zoom + m_panOffset.y()
        );
    }

    QPointF screenToMachine(const QPointF& pt) const {
        return QPointF(
            (pt.x() - m_panOffset.x()) / m_zoom,
            -(pt.y() - m_panOffset.y()) / m_zoom
        );
    }

    // --- Zoom ---

    void applyZoom(double factor, const QPointF& screenAnchor) {
        double newZoom = std::clamp(m_zoom * factor, 0.2, 8.0);
        double actualFactor = newZoom / m_zoom;
        m_panOffset = screenAnchor - actualFactor * (screenAnchor - m_panOffset);
        m_zoom = newZoom;
    }

    // --- Pan ---

    void panBy(const QPointF& delta) {
        m_panOffset += delta;
    }

    // --- View fitting ---

    void autoFit(const QSize& widgetSize, const QSizeF& worktableSize, double margin = 30.0) {
        double tableW = worktableSize.width();
        double tableH = worktableSize.height();
        if (tableW < 1.0) tableW = 300.0;
        if (tableH < 1.0) tableH = 200.0;

        double scaleX = (widgetSize.width() - 2 * margin) / tableW;
        double scaleY = (widgetSize.height() - 2 * margin) / tableH;
        m_zoom = std::min(scaleX, scaleY);

        // Origin at bottom-left (machine coordinates, Y-up)
        m_panOffset = QPointF(margin, widgetSize.height() - margin);
    }

    void fitToSelection(const QSize& widgetSize, const QRectF& entityBBox, double margin = 40.0) {
        double bbW = entityBBox.width();  if (bbW < 1.0) bbW = 1.0;
        double bbH = entityBBox.height(); if (bbH < 1.0) bbH = 1.0;

        double scaleX = (widgetSize.width() - 2 * margin) / bbW;
        double scaleY = (widgetSize.height() - 2 * margin) / bbH;
        m_zoom = std::min(scaleX, scaleY);

        double centerX = (entityBBox.left() + entityBBox.right()) / 2.0;
        double centerY = (entityBBox.top() + entityBBox.bottom()) / 2.0;
        m_panOffset = QPointF(
            widgetSize.width() / 2.0 - centerX * m_zoom,
            widgetSize.height() / 2.0 + centerY * m_zoom
        );
    }

    // --- Accessors ---

    double zoom() const { return m_zoom; }
    QPointF panOffset() const { return m_panOffset; }

    void setZoom(double z) { m_zoom = z; }
    void setPanOffset(const QPointF& p) { m_panOffset = p; }

    // Zoom change detection (for cache invalidation)
    void resetZoomChangeTracking() {
        m_lastCachedZoom = m_zoom;
        m_lastCachedPan = m_panOffset;
    }
    bool zoomOrPanChangedSinceCache() const {
        return m_zoom != m_lastCachedZoom || m_panOffset != m_lastCachedPan;
    }

private:
    double m_zoom = 1.0;
    QPointF m_panOffset;

    // Cache tracking
    double m_lastCachedZoom = 0;
    QPointF m_lastCachedPan;
};
