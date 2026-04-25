#include "DesignCanvas.h"

#include "canvas/ToolManager.h"
#include "canvas/CoordinateTransform.h"
#include "canvas/CanvasDrawing.h"
#include "canvas/CanvasSnap.h"
#include "canvas/EntityClipboard.h"

#include "../grbl/GraphicsDesign/Tools/ChamferTool.h"
#include "../grbl/GraphicsDesign/Tools/FilletTool.h"
#include "../grbl/GraphicsDesign/Tools/MeasureTool.h"
#include "../grbl/GraphicsDesign/Tools/SplineTool.h"
#include "../grbl/GraphicsDesign/Tools/OffsetTool.h"
#include "../grbl/GraphicsDesign/Tools/PolylineTool.h"
#include "../grbl/GraphicsDesign/Tools/SelectTool.h"

#include <QMenu>
#include <QContextMenuEvent>
#include <QApplication>
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QShowEvent>
#include <algorithm>
#include <cmath>

namespace {
constexpr double kShortcutDuplicateOffset = 10.0;

QPointF shortcutOffsetForStep(int step) {
    const double offset = kShortcutDuplicateOffset * std::max(step, 1);
    return QPointF(offset, offset);
}
}

// ============================================================================
// Construction
// ============================================================================

DesignCanvas::DesignCanvas(QWidget *parent)
    : QWidget(parent)
    , m_commandStack(m_document)
    , m_toolManager(m_document, m_commandStack)
    , m_drawing(m_document, m_transforms)
    , m_snap(m_document, m_transforms, m_gridSize, kSnapDistancePx)
    , m_clipboard(m_document, m_commandStack)
{
    setMinimumSize(300, 300);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

    // Throttle repaints
    m_repaintTimer.setSingleShot(true);
    m_repaintTimer.setInterval(50);
    connect(&m_repaintTimer, &QTimer::timeout, this, [this]() {
        if (m_repaintPending) {
            m_repaintPending = false;
            update();
        }
    });

    // Connect document signals
    connect(&m_document, &Document::changed, this, [this]() {
        m_drawing.invalidateCache();
        scheduleUpdate();
        emitUndoRedoState();
        emit selectedEntityChanged();
        emit documentStateChanged();
    });

    connect(&m_document, &Document::selectionChanged, this, [this]() {
        m_drawing.invalidateCache();
        scheduleUpdate();
        emit selectedEntityChanged();
    });

    // Deferred crisp re-render after zoom settles
    m_zoomSettleTimer.setSingleShot(true);
    m_zoomSettleTimer.setInterval(150);
    connect(&m_zoomSettleTimer, &QTimer::timeout, this, [this]() {
        m_drawing.invalidateCache();
        update();
    });

    emitToolStatus();
    emitUndoRedoState();

    // Load canvas settings from singleton
    m_canvasSettings = SettingsManager::instance().canvasSettings();
    connect(&SettingsManager::instance(), &SettingsManager::canvasSettingsChanged,
            this, &DesignCanvas::setCanvasSettings);
}

// ============================================================================
// Tool type conversion
// ============================================================================

ToolManager::Type DesignCanvas::toToolManagerType(ToolType t)
{
    switch (t) {
        case ToolType::Select:    return ToolManager::Type::Select;
        case ToolType::Line:      return ToolManager::Type::Line;
        case ToolType::Polyline:  return ToolManager::Type::Polyline;
        case ToolType::Rectangle: return ToolManager::Type::Rectangle;
        case ToolType::Circle:    return ToolManager::Type::Circle;
        case ToolType::Arc:       return ToolManager::Type::Arc;
        case ToolType::Text:      return ToolManager::Type::Text;
        case ToolType::Dimension: return ToolManager::Type::Dimension;
        case ToolType::AngularDimension: return ToolManager::Type::AngularDimension;
        case ToolType::RadiusDimension:  return ToolManager::Type::RadiusDimension;
        case ToolType::Trim:      return ToolManager::Type::Trim;
        case ToolType::Extend:    return ToolManager::Type::Extend;
        case ToolType::Chamfer:   return ToolManager::Type::Chamfer;
        case ToolType::Fillet:    return ToolManager::Type::Fillet;
        case ToolType::Offset:    return ToolManager::Type::Offset;
        case ToolType::Measure:   return ToolManager::Type::Measure;
        case ToolType::Spline:    return ToolManager::Type::Spline;
    }
    return ToolManager::Type::Select;
}

DesignCanvas::ToolType DesignCanvas::fromToolManagerType(ToolManager::Type t)
{
    switch (t) {
        case ToolManager::Type::Select:    return ToolType::Select;
        case ToolManager::Type::Line:      return ToolType::Line;
        case ToolManager::Type::Polyline:  return ToolType::Polyline;
        case ToolManager::Type::Rectangle: return ToolType::Rectangle;
        case ToolManager::Type::Circle:    return ToolType::Circle;
        case ToolManager::Type::Arc:       return ToolType::Arc;
        case ToolManager::Type::Text:      return ToolType::Text;
        case ToolManager::Type::Dimension: return ToolType::Dimension;
        case ToolManager::Type::AngularDimension: return ToolType::AngularDimension;
        case ToolManager::Type::RadiusDimension:  return ToolType::RadiusDimension;
        case ToolManager::Type::Trim:      return ToolType::Trim;
        case ToolManager::Type::Extend:    return ToolType::Extend;
        case ToolManager::Type::Chamfer:   return ToolType::Chamfer;
        case ToolManager::Type::Fillet:    return ToolType::Fillet;
        case ToolManager::Type::Offset:    return ToolType::Offset;
        case ToolManager::Type::Measure:   return ToolType::Measure;
        case ToolManager::Type::Spline:    return ToolType::Spline;
        case ToolManager::Type::Count:     break;
    }
    return ToolType::Select;
}

// ============================================================================
// Active tool access
// ============================================================================

Tool& DesignCanvas::activeTool()
{
    return m_toolManager.active(m_toolManagerActiveType);
}

const Tool& DesignCanvas::activeTool() const
{
    return m_toolManager.active(m_toolManagerActiveType);
}

// ============================================================================
// Public API — tool / undo / document / view
// ============================================================================

QString DesignCanvas::activeToolName() const
{
    return activeTool().name();
}

QString DesignCanvas::activeToolInstructions() const
{
    return activeTool().instructions();
}

bool DesignCanvas::canUndo() const
{
    return m_commandStack.canUndo();
}

bool DesignCanvas::canRedo() const
{
    return m_commandStack.canRedo();
}

const Entity* DesignCanvas::selectedEntity() const
{
    return m_document.selectedEntity();
}

void DesignCanvas::setTool(ToolType toolType)
{
    ToolManager::Type mt = toToolManagerType(toolType);
    if (m_toolManagerActiveType == mt) return;

    activeTool().cancel();
    m_toolManagerActiveType = mt;
    m_snapResult = {};
    m_hoverHandleId.clear();
    unsetCursor();
    emitToolStatus();
    scheduleUpdate();
    emit toolChanged(toolType);
}

void DesignCanvas::clearDocument()
{
    activeTool().cancel();
    if (m_document.entityCount() == 0) return;
    m_commandStack.execute(std::make_unique<ClearDocumentCommand>());
    emitToolStatus();
    emitUndoRedoState();
}

void DesignCanvas::deleteSelectedEntity()
{
    deleteSelectedEntityInternal();
}

void DesignCanvas::cancelActiveTool()
{
    cancelActiveToolInternal();
}

void DesignCanvas::undo()
{
    cancelActiveToolInternal();
    m_commandStack.undo();
    emitToolStatus();
    emitUndoRedoState();
}

void DesignCanvas::redo()
{
    cancelActiveToolInternal();
    m_commandStack.redo();
    emitToolStatus();
    emitUndoRedoState();
}

void DesignCanvas::setWorktableSize(const QSizeF &size)
{
    m_worktableSize = size;
    m_document.setWorktableWidth(size.width());
    autoFit();
}

void DesignCanvas::setCanvasSettings(const SettingsManager::CanvasSettings &s)
{
    m_canvasSettings = s;
    m_drawing.setCanvasSettings(s);
    scheduleUpdate();
}

void DesignCanvas::setSnapEnabled(bool enabled)
{
    if (m_snapEnabled == enabled) return;
    m_snapEnabled = enabled;
    emit snapEnabledChanged(enabled);
    scheduleUpdate();
}

void DesignCanvas::setGridSize(double size)
{
    if (size <= 0.0 || qFuzzyCompare(size, m_gridSize)) return;
    m_gridSize = size;
    m_snap.setGridSize(size);
    m_drawing.setGridSize(size);
    emit gridSizeChanged(size);
    scheduleUpdate();
}

double DesignCanvas::filletRadius() const
{
    auto* t = m_toolManager.toolAs<FilletTool>(ToolManager::Type::Fillet);
    return t ? t->filletRadius() : 0.0;
}

void DesignCanvas::setFilletRadius(double r)
{
    if (auto* t = m_toolManager.toolAs<FilletTool>(ToolManager::Type::Fillet))
        t->setFilletRadius(r);
}

double DesignCanvas::chamferDistance() const
{
    auto* t = m_toolManager.toolAs<ChamferTool>(ToolManager::Type::Chamfer);
    return t ? t->chamferDistance() : 0.0;
}

void DesignCanvas::setChamferDistance(double d)
{
    if (auto* t = m_toolManager.toolAs<ChamferTool>(ToolManager::Type::Chamfer))
        t->setChamferDistance(d);
}

double DesignCanvas::offsetDistance() const
{
    auto* t = m_toolManager.toolAs<OffsetTool>(ToolManager::Type::Offset);
    return t ? t->offsetDistance() : 0.0;
}

void DesignCanvas::setOffsetDistance(double d)
{
    if (auto* t = m_toolManager.toolAs<OffsetTool>(ToolManager::Type::Offset))
        t->setOffsetDistance(d);
}

// --- Measure tool accessors ---

bool DesignCanvas::hasMeasurement() const
{
    auto* t = m_toolManager.toolAs<MeasureTool>(ToolManager::Type::Measure);
    return t && t->hasMeasurement();
}
double DesignCanvas::measureDistance() const
{
    auto* t = m_toolManager.toolAs<MeasureTool>(ToolManager::Type::Measure);
    return t ? t->distance() : 0.0;
}
double DesignCanvas::measureDeltaX() const
{
    auto* t = m_toolManager.toolAs<MeasureTool>(ToolManager::Type::Measure);
    return t ? t->deltaX() : 0.0;
}
double DesignCanvas::measureDeltaY() const
{
    auto* t = m_toolManager.toolAs<MeasureTool>(ToolManager::Type::Measure);
    return t ? t->deltaY() : 0.0;
}
double DesignCanvas::measureAngle() const
{
    auto* t = m_toolManager.toolAs<MeasureTool>(ToolManager::Type::Measure);
    return t ? t->angle() : 0.0;
}

void DesignCanvas::setGcodeOverlayVisible(bool visible)
{
    if (m_showGcodeOverlay == visible) return;
    m_showGcodeOverlay = visible;
    scheduleUpdate();
}

// ============================================================================
// View helpers
// ============================================================================

void DesignCanvas::autoFit()
{
    m_transforms.autoFit(size(), m_worktableSize, 30.0);
    scheduleUpdate();
}

void DesignCanvas::fitToSelection()
{
    const Entity* sel = selectedEntity();
    if (!sel) { autoFit(); return; }

    QRectF bb = sel->boundingRect();
    if (!bb.isValid() || bb.isEmpty()) { autoFit(); return; }

    m_transforms.fitToSelection(size(), bb, 40.0);
    scheduleUpdate();
}

QPointF DesignCanvas::toWorld(const QPointF& screenPos) const
{
    return m_transforms.toWorld(screenPos);
}

QPointF DesignCanvas::toScreen(const QPointF& worldPos) const
{
    return m_transforms.toScreen(worldPos);
}

void DesignCanvas::scheduleUpdate()
{
    m_repaintPending = true;
    if (!m_repaintTimer.isActive()) {
        m_repaintTimer.start();
    }
}

void DesignCanvas::emitToolStatus()
{
    const QString snapText = m_snapResult.active
        ? QString("  |  Snap: %1").arg(m_snapResult.label)
        : QString("  |  Snap: grid/end/corner/center/quadrant");
    emit toolStatusChanged(QString("%1: %2%3")
        .arg(activeToolName(), activeToolInstructions(), snapText));
}

void DesignCanvas::emitUndoRedoState()
{
    emit undoRedoStateChanged(canUndo(), canRedo());
}

// ============================================================================
// Internal helpers
// ============================================================================

void DesignCanvas::cancelActiveToolInternal()
{
    m_hoverHandleId.clear();
    m_snapResult = {};
    activeTool().cancel();
    unsetCursor();
    emitToolStatus();
    scheduleUpdate();
}

void DesignCanvas::deleteSelectedEntityInternal()
{
    const QSet<QUuid> ids = m_document.selectedIds();
    if (ids.isEmpty()) return;

    std::vector<int> indices;
    indices.reserve(ids.size());
    for (const QUuid& id : ids) {
        int idx = m_document.indexOf(id);
        if (idx >= 0) indices.push_back(idx);
    }
    std::sort(indices.begin(), indices.end(), std::greater<int>());

    for (int idx : indices) {
        m_commandStack.execute(std::make_unique<DeleteEntityCommand>(idx));
    }
    emitToolStatus();
    emitUndoRedoState();
}

// ============================================================================
// Paint event
// ============================================================================

void DesignCanvas::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.fillRect(rect(), QColor(245, 245, 245));
    painter.setRenderHint(QPainter::Antialiasing, true);

    // Rulers in screen coordinates
    m_drawing.drawGridAndRulers(painter, size());

    // --- Entity layer via pixmap cache ---
    if (m_drawing.isCacheValid(m_transforms.zoom(), m_transforms.panOffset(), size())) {
        m_drawing.blitCache(painter);
    } else {
        m_drawing.renderCache(m_transforms.zoom(), m_transforms.panOffset(), size());
        m_drawing.blitCache(painter);
    }

    // Dynamic elements always drawn fresh
    painter.save();
    painter.translate(m_transforms.panOffset());
    painter.scale(m_transforms.zoom(), -m_transforms.zoom());

    m_drawing.drawHandles(painter, m_hoverHandleId,
        m_document.selectedEntity() ? m_document.selectedEntity()->editHandles()
                                     : std::vector<Entity::EditHandle>{});

    painter.setPen(CanvasDrawing::createPreviewPen());
    activeTool().paintPreview(painter);

    m_drawing.drawSnapIndicator(painter, m_snapResult.worldPos, m_snapResult.active);
    
    // Draw entity snap markers (AutoSnap preview)
    const QPointF mouseWorldPos = toWorld(m_mouseScreenPos);
    m_drawing.drawEntitySnapMarkers(painter, mouseWorldPos, 
                                     m_snapResult.label, m_snapResult.worldPos, 
                                     m_snapResult.active);
    
    painter.restore();

    m_drawing.drawCrosshair(painter, m_mouseScreenPos, m_mouseInWidget, size());
    if (m_showGcodeOverlay)
        m_drawing.drawGcodeOverlay(painter, size());
    m_drawing.drawRulerStrips(painter, size(), m_mouseScreenPos, m_mouseInWidget);
}

// ============================================================================
// Mouse events
// ============================================================================

void DesignCanvas::mousePressEvent(QMouseEvent *event)
{
    const QPointF rawWorldPos = toWorld(event->position());

    if (event->button() == Qt::MiddleButton || event->button() == Qt::RightButton) {
        // Right-click on an entity → select it (so context menu can act on it)
        if (event->button() == Qt::RightButton && m_document.selectedIds().isEmpty()) {
            m_document.selectNearest(rawWorldPos, 15.0);
        }
        m_panning = true;
        m_rightClickPanned = false;
        m_lastPanScreenPos = event->position().toPoint();
        setCursor(Qt::ClosedHandCursor);
        return;
    }

    if (event->button() == Qt::LeftButton) {
        if (m_snapEnabled) {
            m_snapResult = m_snap.computeSnap(rawWorldPos, true);
        } else {
            m_snapResult = {};
        }
        const QPointF snappedPos = m_snapResult.active ? m_snapResult.worldPos : rawWorldPos;

        // Pre-resolve handle hit in screen space for SelectTool
        if (m_toolManagerActiveType == ToolManager::Type::Select) {
            auto handle = m_snap.hitHandle(rawWorldPos, selectedHandles());
            m_hoverHandleId = handle ? handle->id : QString();
            // Pass hit handle ID to SelectTool before mousePress
            if (auto* selectTool = m_toolManager.toolAs<SelectTool>(ToolManager::Type::Select)) {
                selectTool->setHitHandleId(m_hoverHandleId);
            }
        }

        const int entityCountBefore = m_document.entityCount();
        activeTool().mousePress(snappedPos);
        const int entityCountAfter = m_document.entityCount();

        // Auto-switch to Select tool after creation
        if (entityCountAfter > entityCountBefore) {
            m_document.select(m_document.entityAt(entityCountAfter - 1)->id());
            setTool(ToolType::Select);
        }

        emitToolStatus();
        scheduleUpdate();
        return;
    }
}

void DesignCanvas::mouseMoveEvent(QMouseEvent *event)
{
    const QPointF rawWorldPos = toWorld(event->position());
    m_mouseScreenPos = event->position();
    m_mouseInWidget = true;

    if (m_panning) {
        const QPoint delta = event->position().toPoint() - m_lastPanScreenPos;
        if (delta.manhattanLength() > 3)
            m_rightClickPanned = true;
        m_transforms.panBy(QPointF(delta.x(), delta.y()));
        m_lastPanScreenPos = event->position().toPoint();
        m_zoomSettleTimer.start();
        update();
        return;
    }

    bool isDragging = (m_toolManagerActiveType == ToolManager::Type::Select && activeTool().isActive());

    if (m_toolManagerActiveType == ToolManager::Type::Select && !isDragging) {
        updateHoverCursor(rawWorldPos);
    }

    if (m_snapEnabled && !isDragging) {
        m_snapResult = m_snap.computeSnap(rawWorldPos, true);
    } else {
        m_snapResult = {};
    }
    const QPointF snappedPos = m_snapResult.active ? m_snapResult.worldPos : rawWorldPos;
    activeTool().mouseMove(snappedPos);

    if (m_toolManagerActiveType == ToolManager::Type::Measure)
        emit measurementChanged();

    emitToolStatus();
    scheduleUpdate();
}

void DesignCanvas::mouseReleaseEvent(QMouseEvent *event)
{
    const QPointF rawWorldPos = toWorld(event->position());

    if (event->button() == Qt::MiddleButton || event->button() == Qt::RightButton) {
        if (m_panning) {
            m_panning = false;
            unsetCursor();
            event->accept();
            // Right-click without drag → explicitly show context menu
            if (event->button() == Qt::RightButton && !m_rightClickPanned) {
                QContextMenuEvent ctxEvent(QContextMenuEvent::Mouse,
                                           event->position().toPoint(),
                                           event->globalPosition().toPoint(),
                                           event->modifiers());
                QApplication::sendEvent(this, &ctxEvent);
            }
            return;
        }
    }

    if (event->button() == Qt::LeftButton) {
        if (m_snapEnabled) {
            m_snapResult = m_snap.computeSnap(rawWorldPos, true);
        } else {
            m_snapResult = {};
        }
        const QPointF snappedPos = m_snapResult.active ? m_snapResult.worldPos : rawWorldPos;
        activeTool().mouseRelease(snappedPos);
        emitToolStatus();
        scheduleUpdate();
    }
}

void DesignCanvas::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton || m_toolManagerActiveType != ToolManager::Type::Polyline) {
        QWidget::mouseDoubleClickEvent(event);
        return;
    }

    const int entityCountBefore = m_document.entityCount();
    if (m_snapEnabled) {
        m_snapResult = m_snap.computeSnap(toWorld(event->position()), true);
    } else {
        m_snapResult = {};
    }

    auto* polyTool = m_toolManager.toolAs<PolylineTool>(ToolManager::Type::Polyline);
    if (polyTool && polyTool->finish() && m_document.entityCount() > entityCountBefore) {
        m_document.clearSelection();
        if (Entity* created = m_document.entityAt(entityCountBefore)) {
            m_document.select(created->id());
        }
        setTool(ToolType::Select);
    }

    emitToolStatus();
    scheduleUpdate();
    event->accept();
}

void DesignCanvas::wheelEvent(QWheelEvent *event)
{
    const double factor = event->angleDelta().y() > 0 ? 1.15 : 1.0 / 1.15;
    m_transforms.applyZoom(factor, event->position());
    m_zoomSettleTimer.start();
    event->accept();
}

// ============================================================================
// Hover cursor update
// ============================================================================

void DesignCanvas::updateHoverCursor(const QPointF& worldPos)
{
    auto handle = m_snap.hitHandle(worldPos, selectedHandles());
    m_hoverHandleId = handle.has_value() ? handle->id : QString();

    if (handle.has_value()) {
        if (m_snap.cursorForHandle(*handle) == Qt::CrossCursor) {
            setCursor(Qt::CrossCursor);
        } else {
            setCursor(Qt::SizeFDiagCursor);
        }
        return;
    }
    unsetCursor();
}

std::vector<Entity::EditHandle> DesignCanvas::selectedHandles() const
{
    const Entity* entity = selectedEntity();
    if (!entity) return {};
    return entity->editHandles();
}

// ============================================================================
// Keyboard events
// ============================================================================

void DesignCanvas::keyPressEvent(QKeyEvent *event)
{
    if (m_toolManagerActiveType == ToolManager::Type::Polyline) {
        auto* polyTool = m_toolManager.toolAs<PolylineTool>(ToolManager::Type::Polyline);
        if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
            const int entityCountBefore = m_document.entityCount();
            if (polyTool && polyTool->finish() && m_document.entityCount() > entityCountBefore) {
                m_document.clearSelection();
                if (Entity* created = m_document.entityAt(entityCountBefore)) {
                    m_document.select(created->id());
                }
                setTool(ToolType::Select);
            }
            emitToolStatus();
            scheduleUpdate();
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_Backspace && polyTool && polyTool->removeLastVertex()) {
            emitToolStatus();
            scheduleUpdate();
            event->accept();
            return;
        }
    }

    if (m_toolManagerActiveType == ToolManager::Type::Spline) {
        auto* splineTool = m_toolManager.toolAs<SplineTool>(ToolManager::Type::Spline);
        if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
            const int entityCountBefore = m_document.entityCount();
            if (splineTool && splineTool->finish() && m_document.entityCount() > entityCountBefore) {
                m_document.clearSelection();
                if (Entity* created = m_document.entityAt(entityCountBefore))
                    m_document.select(created->id());
                setTool(ToolType::Select);
            }
            emitToolStatus();
            scheduleUpdate();
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_Backspace && splineTool && splineTool->removeLastPoint()) {
            emitToolStatus();
            scheduleUpdate();
            event->accept();
            return;
        }
    }

    if (event->matches(QKeySequence::Copy)) {
        m_clipboard.copy();
        event->accept();
        return;
    }
    if (event->matches(QKeySequence::Paste)) {
        m_clipboard.paste();
        emitToolStatus();
        emitUndoRedoState();
        scheduleUpdate();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_D && event->modifiers() == Qt::ControlModifier) {
        m_clipboard.duplicate();
        setTool(ToolType::Select);
        emitToolStatus();
        emitUndoRedoState();
        scheduleUpdate();
        event->accept();
        return;
    }
    if (event->matches(QKeySequence::Undo)) {
        undo();
        event->accept();
        return;
    }
    if (event->matches(QKeySequence::Redo)) {
        redo();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Escape) {
        cancelActiveToolInternal();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Delete) {
        deleteSelectedEntityInternal();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_S && event->modifiers() == Qt::NoModifier) {
        setSnapEnabled(!m_snapEnabled);
        emitToolStatus();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_A && event->modifiers() == Qt::ControlModifier) {
        m_document.selectAll();
        emitToolStatus();
        scheduleUpdate();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_E && event->modifiers() == Qt::ControlModifier) {
        emit exportGcodeRequested();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_M && event->modifiers() == Qt::NoModifier) {
        setTool(ToolType::Measure);
        emitToolStatus();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_B && event->modifiers() == Qt::NoModifier) {
        setTool(ToolType::Spline);
        emitToolStatus();
        event->accept();
        return;
    }

    QWidget::keyPressEvent(event);
}

// ============================================================================
// Resize / show / leave
// ============================================================================

void DesignCanvas::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    m_drawing.onResize(size());
    update();
}

void DesignCanvas::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (!m_initialFitDone) {
        m_initialFitDone = true;
        autoFit();
    }
}

void DesignCanvas::leaveEvent(QEvent *event)
{
    QWidget::leaveEvent(event);
    m_mouseInWidget = false;
    update();
}

// ============================================================================
// Context menu
// ============================================================================

void DesignCanvas::contextMenuEvent(QContextMenuEvent *event)
{
    if (m_document.selectedIds().isEmpty()) {
        event->ignore();
        return;
    }

    QMenu menu(this);

    // Move to Layer submenu
    if (m_document.layerCount() > 0) {
        // Determine current layer of the (single) selected entity, if any
        QUuid currentLayerId;
        if (m_document.selectedIds().size() == 1) {
            const QUuid id = *m_document.selectedIds().begin();
            int idx = m_document.indexOf(id);
            if (idx >= 0 && m_document.entityAt(idx))
                currentLayerId = m_document.entityAt(idx)->layerId();
        }

        QMenu *moveMenu = menu.addMenu("Move to Layer");
        for (int i = 0; i < m_document.layerCount(); ++i) {
            const Layer &layer = m_document.layerAt(i);
            QAction *act = moveMenu->addAction(layer.name);
            QPixmap px(12, 12);
            px.fill(layer.color);
            act->setIcon(QIcon(px));
            act->setCheckable(true);
            act->setChecked(layer.id == currentLayerId);
            const QUuid targetId = layer.id;
            connect(act, &QAction::triggered, this, [this, targetId]() {
                const QSet<QUuid> ids = m_document.selectedIds();
                for (const QUuid &id : ids) {
                    int idx = m_document.indexOf(id);
                    if (idx < 0) continue;
                    const Entity *e = m_document.entityAt(idx);
                    if (!e) continue;
                    auto before = e->clone();
                    auto after  = e->clone();
                    after->setLayerId(targetId);
                    m_commandStack.execute(
                        std::make_unique<UpdateEntityCommand>(idx, std::move(before), std::move(after)));
                }
                emitUndoRedoState();
                scheduleUpdate();
            });
        }
    }

    menu.addSeparator();
    QAction *delAct = menu.addAction("Delete Selected");
    connect(delAct, &QAction::triggered, this, &DesignCanvas::deleteSelectedEntityInternal);

    menu.addSeparator();
    QAction *expAct = menu.addAction("Export G-code...");
    connect(expAct, &QAction::triggered, this, [this]() { emit exportGcodeRequested(); });

    menu.exec(event->globalPos());
}
