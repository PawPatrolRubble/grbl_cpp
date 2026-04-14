#include "DesignCanvas.h"

#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <algorithm>
#include <cmath>

namespace {
constexpr double kPi = 3.14159265358979323846;

QPen createEntityPen() {
    QPen pen(QColor(25, 25, 25));
    pen.setWidth(2);
    pen.setCosmetic(true);
    return pen;
}

QPen createSelectedPen() {
    QPen pen(QColor(196, 65, 43));
    pen.setWidth(3);
    pen.setCosmetic(true);
    return pen;
}

QPen createPreviewPen() {
    QPen pen(QColor(23, 107, 135));
    pen.setWidth(2);
    pen.setStyle(Qt::DashLine);
    pen.setCosmetic(true);
    return pen;
}

bool isRotateRole(Entity::HandleRole role) {
    return role == Entity::HandleRole::Rotate;
}
}

DesignCanvas::DesignCanvas(QWidget *parent)
    : QWidget(parent)
    , m_commandStack(m_document)
    , m_selectTool(m_document, m_commandStack)
    , m_lineTool(m_document, m_commandStack)
    , m_rectangleTool(m_document, m_commandStack)
    , m_circleTool(m_document, m_commandStack)
    , m_arcTool(m_document, m_commandStack)
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
        scheduleUpdate();
        emitUndoRedoState();
        emit selectedEntityChanged();
        emit documentStateChanged();
    });

    connect(&m_document, &Document::selectionChanged, this, [this]() {
        scheduleUpdate();
        emit selectedEntityChanged();
    });

    emitToolStatus();
    emitUndoRedoState();
}

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
    if (m_activeToolType == toolType) {
        return;
    }

    activeTool().cancel();
    m_activeToolType = toolType;
    m_snapResult = {};
    m_hoverHandleId.clear();
    unsetCursor();
    emitToolStatus();
    scheduleUpdate();
}

void DesignCanvas::clearDocument()
{
    activeTool().cancel();
    if (m_document.entityCount() == 0) {
        return;
    }

    m_commandStack.execute(std::make_unique<ClearDocumentCommand>());
    emitToolStatus();
    emitUndoRedoState();
}

void DesignCanvas::deleteSelectedEntity()
{
    Entity* selected = m_document.selectedEntity();
    if (!selected) {
        return;
    }

    int index = m_document.indexOf(selected->id());
    if (index >= 0) {
        m_commandStack.execute(std::make_unique<DeleteEntityCommand>(index));
    }
    emitToolStatus();
    emitUndoRedoState();
}

void DesignCanvas::cancelActiveTool()
{
    m_hoverHandleId.clear();
    m_snapResult = {};
    activeTool().cancel();
    unsetCursor();
    emitToolStatus();
    scheduleUpdate();
}

void DesignCanvas::undo()
{
    cancelActiveTool();
    m_commandStack.undo();
    emitToolStatus();
    emitUndoRedoState();
}

void DesignCanvas::redo()
{
    cancelActiveTool();
    m_commandStack.redo();
    emitToolStatus();
    emitUndoRedoState();
}

void DesignCanvas::setWorktableSize(const QSizeF &size)
{
    m_worktableSize = size;
    autoFit();
}

void DesignCanvas::autoFit()
{
    double margin = 30.0;
    double tableW = m_worktableSize.width();
    double tableH = m_worktableSize.height();
    if (tableW < 1.0) tableW = 300.0;
    if (tableH < 1.0) tableH = 200.0;

    double scaleX = (width() - 2 * margin) / tableW;
    double scaleY = (height() - 2 * margin) / tableH;
    m_zoom = std::min(scaleX, scaleY);

    // Origin at bottom-left (machine coordinates, Y-up)
    m_panOffset = QPointF(margin, height() - margin);
    scheduleUpdate();
}

QPointF DesignCanvas::toWorld(const QPointF& screenPos) const
{
    return (screenPos - m_panOffset) / m_zoom;
}

QPointF DesignCanvas::toScreen(const QPointF& worldPos) const
{
    return worldPos * m_zoom + m_panOffset;
}

QPointF DesignCanvas::machineToScreen(const QPointF& pt) const
{
    // Machine coordinates: Y-up. Screen: Y-down.
    return QPointF(pt.x() * m_zoom + m_panOffset.x(),
                   -pt.y() * m_zoom + m_panOffset.y());
}

QPointF DesignCanvas::screenToMachine(const QPointF& pt) const
{
    return QPointF((pt.x() - m_panOffset.x()) / m_zoom,
                   -(pt.y() - m_panOffset.y()) / m_zoom);
}

void DesignCanvas::applyZoom(double factor, const QPointF& screenAnchor)
{
    const QPointF worldBefore = toWorld(screenAnchor);
    m_zoom = std::clamp(m_zoom * factor, 0.2, 8.0);
    m_panOffset = screenAnchor - worldBefore * m_zoom;
    scheduleUpdate();
}

void DesignCanvas::scheduleUpdate()
{
    m_repaintPending = true;
    if (!m_repaintTimer.isActive()) {
        m_repaintTimer.start();
    }
}

void DesignCanvas::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    autoFit();
}

void DesignCanvas::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.fillRect(rect(), QColor(248, 246, 240));
    painter.setRenderHint(QPainter::Antialiasing, true);

    painter.save();
    painter.translate(m_panOffset);
    painter.scale(m_zoom, m_zoom);

    drawGrid(painter);
    drawWorktable(painter);
    drawAxisOrigin(painter);
    drawEntities(painter);
    drawHandles(painter);

    painter.setPen(createPreviewPen());
    activeTool().paintPreview(painter);

    drawSnapIndicator(painter);
    painter.restore();
}

void DesignCanvas::drawGrid(QPainter& painter)
{
    painter.setPen(QColor(226, 221, 211));

    const QRectF worldBounds = QRectF(toWorld(QPointF(0.0, 0.0)),
                                       toWorld(QPointF(width(), height()))).normalized();

    const int startX = static_cast<int>(std::floor(worldBounds.left() / kGridStep)) * static_cast<int>(kGridStep);
    const int endX = static_cast<int>(std::ceil(worldBounds.right() / kGridStep)) * static_cast<int>(kGridStep);
    const int startY = static_cast<int>(std::floor(worldBounds.top() / kGridStep)) * static_cast<int>(kGridStep);
    const int endY = static_cast<int>(std::ceil(worldBounds.bottom() / kGridStep)) * static_cast<int>(kGridStep);

    for (int x = startX; x <= endX; x += static_cast<int>(kGridStep)) {
        painter.drawLine(QPointF(x, worldBounds.top()), QPointF(x, worldBounds.bottom()));
    }
    for (int y = startY; y <= endY; y += static_cast<int>(kGridStep)) {
        painter.drawLine(QPointF(worldBounds.left(), y), QPointF(worldBounds.right(), y));
    }
}

void DesignCanvas::drawWorktable(QPainter& painter)
{
    // Draw worktable boundary (machine coordinates)
    QPointF bl(0, 0);
    QPointF br(m_worktableSize.width(), 0);
    QPointF tr(m_worktableSize.width(), m_worktableSize.height());
    QPointF tl(0, m_worktableSize.height());

    painter.setPen(QPen(QColor("#d1d5db"), 1.5, Qt::DotLine));
    painter.drawLine(bl, br);
    painter.drawLine(br, tr);
    painter.drawLine(tr, tl);
    painter.drawLine(tl, bl);
}

void DesignCanvas::drawAxisOrigin(QPainter& painter)
{
    QPointF origin(0, 0);

    painter.setPen(QPen(QColor("#f38ba8"), 2));
    QPointF xEnd = origin + QPointF(25, 0);
    painter.drawLine(origin, xEnd);
    painter.drawLine(xEnd, xEnd + QPointF(-4, -4));
    painter.drawLine(xEnd, xEnd + QPointF(-4, 4));

    painter.setPen(QPen(QColor("#a6e3a1"), 2));
    QPointF yEnd = origin + QPointF(0, 25);
    painter.drawLine(origin, yEnd);
    painter.drawLine(yEnd, yEnd + QPointF(-4, -4));
    painter.drawLine(yEnd, yEnd + QPointF(4, -4));
}

void DesignCanvas::drawEntities(QPainter& painter)
{
    Entity* selected = m_document.selectedEntity();

    for (int i = 0; i < m_document.entityCount(); ++i) {
        Entity* entity = m_document.entityAt(i);
        bool isSelected = (selected && entity->id() == selected->id());
        painter.setPen(isSelected ? createSelectedPen() : createEntityPen());
        entity->draw(painter);
    }
}

void DesignCanvas::drawHandles(QPainter& painter)
{
    if (m_activeToolType != ToolType::Select) {
        return;
    }

    Entity* selected = m_document.selectedEntity();
    if (!selected) {
        return;
    }

    painter.save();
    const double halfSize = kHandleHalfSizePx / m_zoom;
    auto handles = selected->editHandles();

    for (const auto& handle : handles) {
        const bool rotate = isRotateRole(handle.role);
        const bool hovered = (m_hoverHandleId == handle.id);

        QPen pen(rotate ? QColor(110, 64, 170) : QColor(53, 90, 120), 1.5 / m_zoom);
        painter.setPen(pen);

        if (handle.hasLeader) {
            painter.drawLine(handle.leaderAnchor, handle.worldPos);
        }

        painter.save();
        painter.translate(handle.worldPos);
        painter.rotate(handle.angleDegrees);
        painter.setBrush(hovered
                         ? (rotate ? QColor(188, 162, 255) : QColor(173, 216, 230))
                         : (rotate ? QColor(231, 222, 255) : QColor(255, 255, 255)));
        painter.drawRect(QRectF(-halfSize, -halfSize, halfSize * 2.0, halfSize * 2.0));
        painter.restore();
    }
    painter.restore();
}

void DesignCanvas::drawSnapIndicator(QPainter& painter) const
{
    if (!m_snapResult.active) {
        return;
    }

    painter.save();
    QPen pen(QColor(22, 138, 173), 1.5 / m_zoom);
    pen.setCosmetic(true);
    painter.setPen(pen);
    painter.setBrush(QColor(186, 230, 253, 160));

    const double half = 7.0 / m_zoom;
    const QPointF p = m_snapResult.worldPos;
    painter.drawLine(QPointF(p.x() - half, p.y()), QPointF(p.x() + half, p.y()));
    painter.drawLine(QPointF(p.x(), p.y() - half), QPointF(p.x(), p.y() + half));
    painter.drawEllipse(p, 3.5 / m_zoom, 3.5 / m_zoom);
    painter.restore();
}

void DesignCanvas::drawPreview(QPainter& painter)
{
    // Preview is painted by the active tool via paintPreview()
}

DesignCanvas::SnapResult DesignCanvas::computeSnap(const QPointF& rawWorldPos, bool includeSelection) const
{
    SnapResult best;
    double bestScreenDistance = kSnapDistancePx;

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
    const QPointF gridPoint(std::round(rawWorldPos.x() / kGridStep) * kGridStep,
                            std::round(rawWorldPos.y() / kGridStep) * kGridStep);
    tryCandidate(gridPoint, "Grid");

    // Entity snap points
    Entity* selected = m_document.selectedEntity();

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

std::vector<Entity::EditHandle> DesignCanvas::selectedHandles() const
{
    const Entity* entity = selectedEntity();
    if (!entity) {
        return {};
    }
    return entity->editHandles();
}

std::optional<Entity::EditHandle> DesignCanvas::hitHandle(const QPointF& worldPos) const
{
    const QPointF screenPos = toScreen(worldPos);
    for (const auto& handle : selectedHandles()) {
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

void DesignCanvas::updateHoverCursor(const QPointF& worldPos)
{
    const auto handle = hitHandle(worldPos);
    m_hoverHandleId = handle.has_value() ? handle->id : QString();

    if (handle.has_value()) {
        if (isRotateRole(handle->role)) {
            setCursor(Qt::CrossCursor);
        } else {
            setCursor(Qt::SizeFDiagCursor);
        }
        return;
    }

    unsetCursor();
}

Tool& DesignCanvas::activeTool()
{
    switch (m_activeToolType) {
        case ToolType::Select:    return m_selectTool;
        case ToolType::Rectangle: return m_rectangleTool;
        case ToolType::Circle:    return m_circleTool;
        case ToolType::Arc:       return m_arcTool;
        case ToolType::Line:
        default:                  return m_lineTool;
    }
}

const Tool& DesignCanvas::activeTool() const
{
    switch (m_activeToolType) {
        case ToolType::Select:    return m_selectTool;
        case ToolType::Rectangle: return m_rectangleTool;
        case ToolType::Circle:    return m_circleTool;
        case ToolType::Arc:       return m_arcTool;
        case ToolType::Line:
        default:                  return m_lineTool;
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

void DesignCanvas::mousePressEvent(QMouseEvent *event)
{
    const QPointF rawWorldPos = toWorld(event->position());

    if (event->button() == Qt::MiddleButton) {
        m_panning = true;
        m_lastPanScreenPos = event->position().toPoint();
        setCursor(Qt::ClosedHandCursor);
        return;
    }

    if (event->button() == Qt::LeftButton) {
        m_snapResult = computeSnap(rawWorldPos, true);
        const QPointF snappedPos = m_snapResult.active ? m_snapResult.worldPos : rawWorldPos;

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

    if (event->button() == Qt::RightButton) {
        cancelActiveTool();
    }
}

void DesignCanvas::mouseMoveEvent(QMouseEvent *event)
{
    const QPointF rawWorldPos = toWorld(event->position());

    if (m_panning) {
        const QPoint delta = event->position().toPoint() - m_lastPanScreenPos;
        m_panOffset += QPointF(delta.x(), delta.y());
        m_lastPanScreenPos = event->position().toPoint();
        scheduleUpdate();
        return;
    }

    if (m_activeToolType == ToolType::Select) {
        updateHoverCursor(rawWorldPos);
    }

    m_snapResult = computeSnap(rawWorldPos, true);
    const QPointF snappedPos = m_snapResult.active ? m_snapResult.worldPos : rawWorldPos;
    activeTool().mouseMove(snappedPos);

    emitToolStatus();
    scheduleUpdate();
}

void DesignCanvas::mouseReleaseEvent(QMouseEvent *event)
{
    const QPointF rawWorldPos = toWorld(event->position());

    if (event->button() == Qt::MiddleButton) {
        m_panning = false;
        unsetCursor();
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton) {
        m_snapResult = computeSnap(rawWorldPos, true);
        const QPointF snappedPos = m_snapResult.active ? m_snapResult.worldPos : rawWorldPos;
        activeTool().mouseRelease(snappedPos);
        emitToolStatus();
        scheduleUpdate();
    }
}

void DesignCanvas::wheelEvent(QWheelEvent *event)
{
    const double factor = event->angleDelta().y() > 0 ? 1.15 : 1.0 / 1.15;
    applyZoom(factor, event->position());
    event->accept();
}

void DesignCanvas::keyPressEvent(QKeyEvent *event)
{
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
        cancelActiveTool();
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_Delete) {
        deleteSelectedEntity();
        event->accept();
        return;
    }

    QWidget::keyPressEvent(event);
}