#pragma once

#include "CoordinateTransform.h"
#include "SettingsManager.h"

#include "../../grbl/GraphicsDesign/Document.h"

#include <QPainter>
#include <QPixmap>
#include <QSize>

/// All canvas rendering logic extracted from DesignCanvas.
///
/// Responsibilities:
///   - Entity pixmap caching (render once, blit on pan/zoom)
///   - Grid, rulers, worktable, axis, entities, handles, snap, G-code, crosshair
///
/// Per-frame state (mouse, snap, handles) is passed as parameters — CanvasDrawing
/// only owns the persistent entity pixmap cache and canvas settings.
class CanvasDrawing
{
public:
    CanvasDrawing(Document& document, const CoordinateTransform& transforms);

    // --- Cache management ---
    void invalidateCache() { m_entityCacheDirty = true; }
    void onResize(const QSize& size) { m_entityCacheSize = size; m_entityCacheDirty = true; }

    /// Full re-render of entities to the pixmap cache.
    void renderCache(double zoom, const QPointF& pan, const QSize& size);

    /// Check if cache is valid for the current view state.
    bool isCacheValid(double zoom, const QPointF& pan, const QSize& size) const;

    /// Blit the entity cache onto the widget painter.
    void blitCache(QPainter& widgetPainter) const;

    /// Blit with zoom/pan offset (cache outdated but close enough for fast zooming).
    void blitCacheTransformed(QPainter& widgetPainter,
                               double currentZoom, const QPointF& currentPan) const;

    // --- Per-frame drawing (called fresh each paintEvent) ---
    void drawGridAndRulers(QPainter& painter, const QSize& widgetSize) const;
    void drawRulerStrips(QPainter& painter, const QSize& widgetSize,
                         const QPointF& mouseScreenPos, bool mouseInWidget) const;
    void drawGcodeOverlay(QPainter& painter, const QSize& widgetSize) const;
    void drawWorktable(QPainter& painter) const;
    void drawAxisOrigin(QPainter& painter) const;
    void drawEntities(QPainter& painter, const QSize& widgetSize) const;
    void drawHandles(QPainter& painter,
                     const QString& hoverHandleId,
                     const std::vector<Entity::EditHandle>& handles) const;
    void drawSnapIndicator(QPainter& painter, QPointF snapWorldPos, bool snapActive) const;
    void drawEntitySnapMarkers(QPainter& painter, const QPointF& mouseWorldPos,
                                const QString& activeSnapLabel,
                                const QPointF& activeSnapPos,
                                bool snapActive) const;
    void drawCrosshair(QPainter& painter, const QPointF& mouseScreenPos,
                       bool mouseInWidget, const QSize& widgetSize) const;

    // --- Pen factories (used by DesignCanvas for preview pen) ---
    static QPen createEntityPen(const QColor& layerColor);
    static QPen createSelectedPen();
    static QPen createPreviewPen();

    // --- Settings ---
    void setCanvasSettings(const SettingsManager::CanvasSettings& s) { m_canvasSettings = s; invalidateCache(); }
    void setGridSize(double size) { m_gridSize = size; }
    void setWorktableSize(const QSizeF& size) { m_worktableSize = size; invalidateCache(); }

    QSizeF worktableSize() const { return m_worktableSize; }
    double gridSize() const { return m_gridSize; }

private:
    Document& m_document;
    const CoordinateTransform& m_transforms;

    QSizeF m_worktableSize = QSizeF(300, 200);
    SettingsManager::CanvasSettings m_canvasSettings;
    double m_gridSize = 10.0;

    // Entity render cache
    QPixmap m_entityCache;
    QSize m_entityCacheSize;
    double m_entityCacheZoom = 0;
    QPointF m_entityCachePanOffset;
    bool m_entityCacheDirty = true;
};