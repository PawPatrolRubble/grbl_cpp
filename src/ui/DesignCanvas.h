#pragma once

#include "canvas/ToolManager.h"
#include "canvas/CoordinateTransform.h"
#include "canvas/CanvasDrawing.h"
#include "canvas/CanvasSnap.h"
#include "canvas/EntityClipboard.h"
#include "SettingsManager.h"

#include <QWidget>
#include <QTimer>

/// Canvas widget for drawing and editing entities with pan/zoom/snap.
///
/// Refactored: delegates to focused subsystems.
///   - ToolManager      — 15 tools, replaces duplicated switch statements
///   - CoordinateTransform — screen/world/machine coordinate math
///   - CanvasDrawing    — all rendering (grid, rulers, entities, handles, etc.)
///   - CanvasSnap       — snap computation + handle hit-testing
///   - EntityClipboard  — copy/paste/duplicate entity operations
class DesignCanvas : public QWidget
{
    Q_OBJECT

public:
    /// Public tool type enum (mirrors ToolManager::Type for backward compatibility).
    enum class ToolType {
        Select, Line, Polyline, Rectangle, Circle, Arc, Text,
        Dimension, AngularDimension, RadiusDimension,
        Trim, Extend, Chamfer, Fillet, Offset, Measure, Spline
    };

    /// Snap result (forwarded from CanvasSnap).
    struct SnapResult {
        bool active = false;
        QPointF worldPos;
        QString label;
    };

    explicit DesignCanvas(QWidget *parent = nullptr);

    // --- Tool access ---
    QString activeToolName() const;
    QString activeToolInstructions() const;
    ToolType currentToolType() const { return fromToolManagerType(m_toolManagerActiveType); }

    // --- Undo/redo ---
    bool canUndo() const;
    bool canRedo() const;

    // --- Document access ---
    Document* document() { return &m_document; }
    const Document* document() const { return &m_document; }
    const Entity* selectedEntity() const;

    // --- View ---
    double zoom() const { return m_transforms.zoom(); }
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
    // Measure tool accessors (valid when Measure tool is active)
    bool   hasMeasurement()   const;
    double measureDistance()  const;
    double measureDeltaX()    const;
    double measureDeltaY()    const;
    double measureAngle()     const;
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
    void measurementChanged();  ///< Emitted on every mouse move when Measure tool is active.

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
    // --- Tool type conversion ---
    static ToolManager::Type toToolManagerType(ToolType t);
    static ToolType fromToolManagerType(ToolManager::Type t);

    Tool& activeTool();
    const Tool& activeTool() const;

    // --- Helpers ---
    void emitToolStatus();
    void emitUndoRedoState();
    void scheduleUpdate();
    void cancelActiveToolInternal();
    void deleteSelectedEntityInternal();
    QPointF toWorld(const QPointF& screenPos) const;
    QPointF toScreen(const QPointF& worldPos) const;
    std::vector<Entity::EditHandle> selectedHandles() const;
    void updateHoverCursor(const QPointF& worldPos);

    // --- Subsystems ---
    Document m_document;
    CommandStack m_commandStack;

    ToolManager m_toolManager;
    ToolManager::Type m_toolManagerActiveType = ToolManager::Type::Select;

    CoordinateTransform m_transforms;
    CanvasDrawing m_drawing;
    CanvasSnap m_snap;
    EntityClipboard m_clipboard;

    // --- State ---
    QSizeF m_worktableSize = QSizeF(300, 200);
    SettingsManager::CanvasSettings m_canvasSettings;
    bool m_snapEnabled = true;
    double m_gridSize = 10.0;
    bool m_showGcodeOverlay = false;
    bool m_panning = false;
    bool m_rightClickPanned = false;  ///< true if right-button was dragged (pan); suppresses context menu
    QPoint m_lastPanScreenPos;
    bool m_initialFitDone = false;
    bool m_mouseInWidget = false;
    QPointF m_mouseScreenPos;

    // --- Snap state ---
    CanvasSnap::SnapResult m_snapResult;
    QString m_hoverHandleId;

    // --- Timers ---
    QTimer m_repaintTimer;
    bool m_repaintPending = false;
    QTimer m_zoomSettleTimer;

    // --- Constants ---
    static constexpr double kSnapDistancePx = 14.0;
    static constexpr double kHandleHitHalfSizePx = 10.0;
};
