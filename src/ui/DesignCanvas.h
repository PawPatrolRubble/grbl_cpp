#pragma once

#include "../grbl/GraphicsDesign/Document.h"
#include "../grbl/GraphicsDesign/CommandStack.h"
#include "../grbl/GraphicsDesign/Tools/Tool.h"
#include "../grbl/GraphicsDesign/Tools/SelectTool.h"
#include "../grbl/GraphicsDesign/Tools/LineTool.h"
#include "../grbl/GraphicsDesign/Tools/PolylineTool.h"
#include "../grbl/GraphicsDesign/Tools/RectangleTool.h"
#include "../grbl/GraphicsDesign/Tools/CircleTool.h"
#include "../grbl/GraphicsDesign/Tools/ArcTool.h"
#include "../grbl/GraphicsDesign/Tools/TextTool.h"
#include "../grbl/GraphicsDesign/Tools/DimensionTool.h"
#include "../grbl/GraphicsDesign/Tools/AngularDimensionTool.h"
#include "../grbl/GraphicsDesign/Tools/RadiusDimensionTool.h"
#include "../grbl/GraphicsDesign/Tools/TrimExtendTool.h"
#include "../grbl/GraphicsDesign/Tools/ChamferTool.h"
#include "../grbl/GraphicsDesign/Tools/FilletTool.h"
#include "../grbl/GraphicsDesign/Tools/OffsetTool.h"

#include "SettingsManager.h"

#include <QWidget>
#include <QTimer>
#include <QPointF>
#include <memory>

/// Canvas widget for drawing and editing entities with pan/zoom/snap.
/// Uses Document/Entity/Tool system for professional CAD features.
class DesignCanvas : public QWidget
{
    Q_OBJECT

public:
    enum class ToolType {
        Select,
        Line,
        Polyline,
        Rectangle,
        Circle,
        Arc,
        Text,
        Dimension,
        AngularDimension,
        RadiusDimension,
        Trim,
        Extend,
        Chamfer,
        Fillet,
        Offset
    };

    struct SnapResult {
        bool active = false;
        QPointF worldPos;
        QString label;
    };

    explicit DesignCanvas(QWidget *parent = nullptr);

    // Tool access
    QString activeToolName() const;
    QString activeToolInstructions() const;
    ToolType currentToolType() const { return m_activeToolType; }

    // Undo/redo
    bool canUndo() const;
    bool canRedo() const;

    // Document access
    Document* document() { return &m_document; }
    const Document* document() const { return &m_document; }
    const Entity* selectedEntity() const;

    // View
    double zoom() const { return m_zoom; }
    void autoFit();
    void fitToSelection();
    void setWorktableSize(const QSizeF &size);
    void setCanvasSettings(const SettingsManager::CanvasSettings &s);
    bool snapEnabled() const { return m_snapEnabled; }
    void setSnapEnabled(bool enabled);
    double gridSize() const { return m_gridSize; }
    void setGridSize(double size);
    double filletRadius() const;
    void setFilletRadius(double r);
    double chamferDistance() const;
    void setChamferDistance(double d);
    double offsetDistance() const;
    void setOffsetDistance(double d);
    bool gcodeOverlayVisible() const { return m_showGcodeOverlay; }
    void setGcodeOverlayVisible(bool visible);

public slots:
    void setTool(ToolType toolType);
    void clearDocument();
    void deleteSelectedEntity();
    void cancelActiveTool();
    void undo();
    void redo();

signals:
    void toolStatusChanged(const QString& message);
    void undoRedoStateChanged(bool canUndo, bool canRedo);
    void selectedEntityChanged();
    void documentStateChanged();
    void toolChanged(ToolType toolType);
    void snapEnabledChanged(bool enabled);
    void gridSizeChanged(double size);
    void exportGcodeRequested();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    Tool& activeTool();
    const Tool& activeTool() const;

    void emitToolStatus();
    void emitUndoRedoState();
    void copySelectedEntity();
    void pasteCopiedEntity();
    void duplicateSelectedEntity();
    void addEntityFromShortcut(std::unique_ptr<Entity> entity);

    // Coordinate transforms (Y-up machine coordinates)
    QPointF toWorld(const QPointF& screenPos) const;
    QPointF toScreen(const QPointF& worldPos) const;
    QPointF machineToScreen(const QPointF& pt) const;
    QPointF screenToMachine(const QPointF& pt) const;

    void applyZoom(double factor, const QPointF& screenAnchor);
    void scheduleUpdate();

    // Drawing helpers
    void drawGridAndRulers(QPainter& painter);
    void drawRulerStrips(QPainter& painter);
    void drawGcodeOverlay(QPainter& painter);
    void drawWorktable(QPainter& painter);
    void drawAxisOrigin(QPainter& painter);
    void drawEntities(QPainter& painter);
    void drawHandles(QPainter& painter);
    void drawSnapIndicator(QPainter& painter) const;
    void drawCrosshair(QPainter& painter);
    void drawPreview(QPainter& painter);

    // Snap system
    SnapResult computeSnap(const QPointF& rawWorldPos, bool includeSelection) const;

    // Handle helpers
    std::vector<Entity::EditHandle> selectedHandles() const;
    std::optional<Entity::EditHandle> hitHandle(const QPointF& worldPos) const;
    void updateHoverCursor(const QPointF& worldPos);

    // Document and tools
    Document m_document;
    CommandStack m_commandStack;
    SelectTool m_selectTool;
    LineTool m_lineTool;
    PolylineTool m_polylineTool;
    RectangleTool m_rectangleTool;
    CircleTool m_circleTool;
    ArcTool m_arcTool;
    TextTool m_textTool;
    DimensionTool m_dimensionTool;
    AngularDimensionTool m_angularDimensionTool;
    RadiusDimensionTool m_radiusDimensionTool;
    TrimExtendTool m_trimTool;
    TrimExtendTool m_extendTool;
    ChamferTool    m_chamferTool;
    FilletTool     m_filletTool;
    OffsetTool     m_offsetTool;
    ToolType m_activeToolType = ToolType::Select;

    // View state
    double m_zoom = 1.0;
    QPointF m_panOffset;
    QSizeF m_worktableSize = QSizeF(300, 200);
    SettingsManager::CanvasSettings m_canvasSettings;
    bool m_panning = false;
    QPoint m_lastPanScreenPos;

    // Handle hover
    QString m_hoverHandleId;

    // Snap result
    SnapResult m_snapResult;
    bool m_snapEnabled = true;
    double m_gridSize = 10.0;  // machine units (mm)
    bool m_showGcodeOverlay = false;

    // Repaint throttle
    QTimer m_repaintTimer;
    bool m_repaintPending = false;
    bool m_initialFitDone = false;

    // Entity render cache — render entities to pixmap, blit on pan/zoom
    QPixmap m_entityCache;
    double m_entityCacheZoom = 0;
    QPointF m_entityCachePanOffset;
    QSize m_entityCacheSize;
    bool m_entityCacheDirty = true;

    // Shortcut clipboard for basic CAD edit operations.
    std::unique_ptr<Entity> m_copiedEntity;
    int m_pasteSerial = 0;

    // Deferred crisp render after zoom
    QTimer m_zoomSettleTimer;

    // Constants
    static constexpr double kHandleHalfSizePx = 6.0;
    static constexpr double kHandleHitHalfSizePx = 10.0;
    static constexpr double kGridStep = 25.0;
    static constexpr double kSnapDistancePx = 14.0;

    // Mouse crosshair tracking
    QPointF m_mouseScreenPos;
    bool m_mouseInWidget = false;
};