#pragma once

#include "../grbl/GraphicsDesign/Document.h"
#include "../grbl/GraphicsDesign/CommandStack.h"
#include "../grbl/GraphicsDesign/Tools/Tool.h"
#include "../grbl/GraphicsDesign/Tools/SelectTool.h"
#include "../grbl/GraphicsDesign/Tools/LineTool.h"
#include "../grbl/GraphicsDesign/Tools/RectangleTool.h"
#include "../grbl/GraphicsDesign/Tools/CircleTool.h"
#include "../grbl/GraphicsDesign/Tools/ArcTool.h"
#include "../grbl/GraphicsDesign/Tools/TextTool.h"

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
        Rectangle,
        Circle,
        Arc,
        Text
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
    void setWorktableSize(const QSizeF &size);
    bool snapEnabled() const { return m_snapEnabled; }
    void setSnapEnabled(bool enabled);

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

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    Tool& activeTool();
    const Tool& activeTool() const;

    void emitToolStatus();
    void emitUndoRedoState();

    // Coordinate transforms (Y-up machine coordinates)
    QPointF toWorld(const QPointF& screenPos) const;
    QPointF toScreen(const QPointF& worldPos) const;
    QPointF machineToScreen(const QPointF& pt) const;
    QPointF screenToMachine(const QPointF& pt) const;

    void applyZoom(double factor, const QPointF& screenAnchor);
    void scheduleUpdate();

    // Drawing helpers
    void drawGridAndRulers(QPainter& painter);
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
    RectangleTool m_rectangleTool;
    CircleTool m_circleTool;
    ArcTool m_arcTool;
    TextTool m_textTool;
    ToolType m_activeToolType = ToolType::Select;

    // View state
    double m_zoom = 1.0;
    QPointF m_panOffset;
    QSizeF m_worktableSize = QSizeF(300, 200);
    bool m_panning = false;
    QPoint m_lastPanScreenPos;

    // Handle hover
    QString m_hoverHandleId;

    // Snap result
    SnapResult m_snapResult;
    bool m_snapEnabled = false;

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