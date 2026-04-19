#include "DesignCanvas.h"

#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QShowEvent>
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
    , m_textTool(m_document, m_commandStack)
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
    emit toolChanged(toolType);
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
    m_document.setWorktableWidth(size.width());
    autoFit();
}

void DesignCanvas::setSnapEnabled(bool enabled)
{
    m_snapEnabled = enabled;
    scheduleUpdate();
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
    // World = Machine coordinates (Y-up)
    return QPointF((screenPos.x() - m_panOffset.x()) / m_zoom,
                   -(screenPos.y() - m_panOffset.y()) / m_zoom);
}

QPointF DesignCanvas::toScreen(const QPointF& worldPos) const
{
    // Screen coordinates (Y-down) from machine coordinates (Y-up)
    return QPointF(worldPos.x() * m_zoom + m_panOffset.x(),
                   -worldPos.y() * m_zoom + m_panOffset.y());
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
    double newZoom = std::clamp(m_zoom * factor, 0.2, 8.0);
    double actualFactor = newZoom / m_zoom;
    m_panOffset = screenAnchor - actualFactor * (screenAnchor - m_panOffset);
    m_zoom = newZoom;
    update();
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

void DesignCanvas::drawCrosshair(QPainter &painter)
{
    if (!m_mouseInWidget) return;

    painter.save();

    QPen crossPen(QColor(100, 100, 100, 150), 1.0, Qt::DashLine);
    painter.setPen(crossPen);
    painter.drawLine(QPointF(m_mouseScreenPos.x(), 0),
                     QPointF(m_mouseScreenPos.x(), height()));
    painter.drawLine(QPointF(0, m_mouseScreenPos.y()),
                     QPointF(width(), m_mouseScreenPos.y()));

    QPointF machPos = toWorld(m_mouseScreenPos);
    QString coordText = QString("X:%1  Y:%2")
                            .arg(machPos.x(), 0, 'f', 2)
                            .arg(machPos.y(), 0, 'f', 2);

    QFont font("Consolas", 9);
    painter.setFont(font);
    QFontMetrics fm(font);
    QRect textRect = fm.boundingRect(coordText);
    textRect.adjust(-4, -2, 4, 2);

    double labelX = m_mouseScreenPos.x() + 12;
    double labelY = m_mouseScreenPos.y() - 12;
    if (labelX + textRect.width() > width())
        labelX = m_mouseScreenPos.x() - textRect.width() - 12;
    if (labelY - textRect.height() < 0)
        labelY = m_mouseScreenPos.y() + textRect.height() + 4;

    QRectF bgRect(labelX, labelY - textRect.height(), textRect.width(), textRect.height());
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(255, 255, 255, 210));
    painter.drawRoundedRect(bgRect.adjusted(-2, -1, 2, 1), 3, 3);

    painter.setPen(QColor(50, 50, 50));
    painter.drawText(bgRect, Qt::AlignCenter, coordText);

    painter.restore();
}

void DesignCanvas::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.fillRect(rect(), QColor(248, 246, 240));
    painter.setRenderHint(QPainter::Antialiasing, true);

    // Rulers are drawn in screen coordinates
    drawGridAndRulers(painter);

    painter.save();
    painter.translate(m_panOffset);
    painter.scale(m_zoom, -m_zoom);  // Y-up for machine coordinates

    drawWorktable(painter);
    drawAxisOrigin(painter);
    drawEntities(painter);
    drawHandles(painter);

    painter.setPen(createPreviewPen());
    activeTool().paintPreview(painter);

    drawSnapIndicator(painter);
    painter.restore();

    drawCrosshair(painter);
}

void DesignCanvas::drawGridAndRulers(QPainter& painter)
{
    // Adaptive step like ToolpathCanvas
    double stepMachine = 10.0;
    while (stepMachine * m_zoom < 30.0) {
        stepMachine *= 2.0;
        if (stepMachine * m_zoom >= 30.0) break;
        stepMachine *= 2.5;
    }

    QPointF machineTL = screenToMachine(QPointF(0, 0));
    QPointF machineBR = screenToMachine(QPointF(width(), height()));

    double startX = std::floor(std::min(machineTL.x(), machineBR.x()) / stepMachine) * stepMachine;
    double endX   = std::ceil(std::max(machineTL.x(), machineBR.x()) / stepMachine) * stepMachine;
    double startY = std::floor(std::min(machineTL.y(), machineBR.y()) / stepMachine) * stepMachine;
    double endY   = std::ceil(std::max(machineTL.y(), machineBR.y()) / stepMachine) * stepMachine;

    // Grid lines
    painter.setPen(QPen(QColor("#d1d5db"), 1));
    for (double x = startX; x <= endX; x += stepMachine) {
        QPointF p1 = machineToScreen(QPointF(x, startY));
        QPointF p2 = machineToScreen(QPointF(x, endY));
        painter.drawLine(p1, p2);
    }
    for (double y = startY; y <= endY; y += stepMachine) {
        QPointF p1 = machineToScreen(QPointF(startX, y));
        QPointF p2 = machineToScreen(QPointF(endX, y));
        painter.drawLine(p1, p2);
    }

    // Ruler labels along X-axis (bottom) and Y-axis (left)
    QFont rulerFont("Sans Serif", 8);
    painter.setFont(rulerFont);
    painter.setPen(QColor("#6b7280"));

    for (double x = startX; x <= endX; x += stepMachine) {
        QPointF p = machineToScreen(QPointF(x, 0));
        painter.drawText(QPointF(p.x() + 3, height() - 4), QString::number(x, 'g', 6));
    }
    for (double y = startY; y <= endY; y += stepMachine) {
        QPointF p = machineToScreen(QPointF(0, y));
        painter.drawText(QPointF(4, p.y() - 3), QString::number(y, 'g', 6));
    }
}

void DesignCanvas::drawWorktable(QPainter& painter)
{
    // Draw worktable boundary in machine coordinates (Y-up)
    QPointF bl(0, 0);
    QPointF br(m_worktableSize.width(), 0);
    QPointF tr(m_worktableSize.width(), m_worktableSize.height());
    QPointF tl(0, m_worktableSize.height());

    painter.setPen(QPen(QColor("#9ca3af"), 0.5, Qt::DotLine));
    painter.drawLine(bl, br);
    painter.drawLine(br, tr);
    painter.drawLine(tr, tl);
    painter.drawLine(tl, bl);

    // Worktable size label (un-flip Y for text)
    painter.save();
    double invZoom = 1.0 / m_zoom;
    QFont labelFont("Sans Serif", 8);
    painter.setFont(labelFont);
    painter.translate(tr);
    painter.scale(invZoom, -invZoom);  // Un-flip Y for text
    painter.setPen(QColor("#9ca3af"));
    painter.drawText(QPointF(4, 12), QString("%1 x %2 mm")
                     .arg(m_worktableSize.width(), 0, 'f', 0)
                     .arg(m_worktableSize.height(), 0, 'f', 0));
    painter.restore();
}

void DesignCanvas::drawAxisOrigin(QPainter& painter)
{
    QPointF origin(0, 0);
    double invZoom = 1.0 / m_zoom;
    double arrowLen = 30.0 * invZoom;
    double headLen = 5.0 * invZoom;

    // X-axis (red, pointing right)
    painter.setPen(QPen(QColor("#f38ba8"), 2));
    QPointF xEnd = origin + QPointF(arrowLen, 0);
    painter.drawLine(origin, xEnd);
    painter.drawLine(xEnd, xEnd + QPointF(-headLen, -headLen));
    painter.drawLine(xEnd, xEnd + QPointF(-headLen, headLen));

    // X label (un-flip Y for text)
    painter.save();
    painter.translate(xEnd + QPointF(3.0 * invZoom, 0));
    painter.scale(invZoom, -invZoom);
    QFont labelFont("Sans Serif", 9);
    labelFont.setBold(true);
    painter.setFont(labelFont);
    painter.drawText(QPointF(0, 4), "X");
    painter.restore();

    // Y-axis (green, pointing up)
    painter.setPen(QPen(QColor("#a6e3a1"), 2));
    QPointF yEnd = origin + QPointF(0, arrowLen);
    painter.drawLine(origin, yEnd);
    painter.drawLine(yEnd, yEnd + QPointF(-headLen, -headLen));
    painter.drawLine(yEnd, yEnd + QPointF(headLen, -headLen));

    // Y label (un-flip Y for text)
    painter.save();
    painter.translate(yEnd + QPointF(0, 3.0 * invZoom));
    painter.scale(invZoom, -invZoom);
    painter.setPen(QColor("#a6e3a1"));
    painter.setFont(labelFont);
    painter.drawText(QPointF(-4, 0), "Y");
    painter.restore();
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
        case ToolType::Text:      return m_textTool;
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
        case ToolType::Text:      return m_textTool;
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

    if (event->button() == Qt::MiddleButton || event->button() == Qt::RightButton) {
        m_panning = true;
        m_lastPanScreenPos = event->position().toPoint();
        setCursor(Qt::ClosedHandCursor);
        return;
    }

    if (event->button() == Qt::LeftButton) {
        if (m_snapEnabled) {
            m_snapResult = computeSnap(rawWorldPos, true);
        } else {
            m_snapResult = {};
        }
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
}

void DesignCanvas::mouseMoveEvent(QMouseEvent *event)
{
    const QPointF rawWorldPos = toWorld(event->position());
    m_mouseScreenPos = event->position();
    m_mouseInWidget = true;

    if (m_panning) {
        const QPoint delta = event->position().toPoint() - m_lastPanScreenPos;
        m_panOffset += QPointF(delta.x(), delta.y());
        m_lastPanScreenPos = event->position().toPoint();
        update();
        return;
    }

    if (m_activeToolType == ToolType::Select) {
        updateHoverCursor(rawWorldPos);
    }

    if (m_snapEnabled) {
        m_snapResult = computeSnap(rawWorldPos, true);
    } else {
        m_snapResult = {};
    }
    const QPointF snappedPos = m_snapResult.active ? m_snapResult.worldPos : rawWorldPos;
    activeTool().mouseMove(snappedPos);

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
            return;
        }
    }

    if (event->button() == Qt::LeftButton) {
        if (m_snapEnabled) {
            m_snapResult = computeSnap(rawWorldPos, true);
        } else {
            m_snapResult = {};
        }
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

    if (event->key() == Qt::Key_S && event->modifiers() == Qt::NoModifier) {
        setSnapEnabled(!m_snapEnabled);
        emitToolStatus();
        event->accept();
        return;
    }

    QWidget::keyPressEvent(event);
}