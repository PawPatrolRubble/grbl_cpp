#include "DesignCanvas.h"

#include <QMenu>
#include <QContextMenuEvent>
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QShowEvent>
#include <algorithm>
#include <cmath>

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kShortcutDuplicateOffset = 10.0;

QPen createEntityPen() {
    QPen pen(QColor(210, 210, 210));
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

QPointF shortcutOffsetForStep(int step) {
    const double offset = kShortcutDuplicateOffset * std::max(step, 1);
    return QPointF(offset, offset);
}
}

DesignCanvas::DesignCanvas(QWidget *parent)
    : QWidget(parent)
    , m_commandStack(m_document)
    , m_selectTool(m_document, m_commandStack)
    , m_lineTool(m_document, m_commandStack)
    , m_polylineTool(m_document, m_commandStack)
    , m_rectangleTool(m_document, m_commandStack)
    , m_circleTool(m_document, m_commandStack)
    , m_arcTool(m_document, m_commandStack)
    , m_textTool(m_document, m_commandStack)
    , m_dimensionTool(m_document, m_commandStack)
    , m_angularDimensionTool(m_document, m_commandStack)
    , m_radiusDimensionTool(m_document, m_commandStack)
    , m_trimTool(m_document, m_commandStack, TrimExtendTool::Mode::Trim)
    , m_extendTool(m_document, m_commandStack, TrimExtendTool::Mode::Extend)
    , m_chamferTool(m_document, m_commandStack)
    , m_filletTool(m_document, m_commandStack)
    , m_offsetTool(m_document, m_commandStack)
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
        m_entityCacheDirty = true;
        scheduleUpdate();
        emitUndoRedoState();
        emit selectedEntityChanged();
        emit documentStateChanged();
    });

    connect(&m_document, &Document::selectionChanged, this, [this]() {
        m_entityCacheDirty = true;
        scheduleUpdate();
        emit selectedEntityChanged();
    });

    // Deferred crisp re-render after zoom settles
    m_zoomSettleTimer.setSingleShot(true);
    m_zoomSettleTimer.setInterval(150);
    connect(&m_zoomSettleTimer, &QTimer::timeout, this, [this]() {
        m_entityCacheDirty = true;
        update();
    });

    emitToolStatus();
    emitUndoRedoState();

    // Load canvas settings from singleton and auto-update on change
    m_canvasSettings = SettingsManager::instance().canvasSettings();
    connect(&SettingsManager::instance(), &SettingsManager::canvasSettingsChanged,
            this, &DesignCanvas::setCanvasSettings);
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
    const QSet<QUuid> ids = m_document.selectedIds();
    if (ids.isEmpty()) return;

    // Collect indices; delete from high to low so indices don't shift
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

void DesignCanvas::setCanvasSettings(const SettingsManager::CanvasSettings &s)
{
    m_canvasSettings = s;
    m_entityCacheDirty = true;
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
    emit gridSizeChanged(size);
    scheduleUpdate();
}

double DesignCanvas::filletRadius() const { return m_filletTool.filletRadius(); }
void DesignCanvas::setFilletRadius(double r) { m_filletTool.setFilletRadius(r); }
double DesignCanvas::chamferDistance() const { return m_chamferTool.chamferDistance(); }
void DesignCanvas::setChamferDistance(double d) { m_chamferTool.setChamferDistance(d); }
double DesignCanvas::offsetDistance() const { return m_offsetTool.offsetDistance(); }
void DesignCanvas::setOffsetDistance(double d) { m_offsetTool.setOffsetDistance(d); }

void DesignCanvas::setGcodeOverlayVisible(bool visible)
{
    if (m_showGcodeOverlay == visible) return;
    m_showGcodeOverlay = visible;
    scheduleUpdate();
}

void DesignCanvas::drawGcodeOverlay(QPainter& painter)
{
    // Draw the G-code toolpath over the canvas.
    // Rapid (G0) moves are shown as thin dashed cyan lines.
    // Cut (G1/arc) moves are shown as solid orange lines.

    QPen rapidPen(QColor(6, 182, 212, 160), 1, Qt::DashLine);  // cyan dashed
    rapidPen.setCosmetic(true);
    QPen cutPen(QColor(249, 115, 22, 200), 1.5);                // orange solid
    cutPen.setCosmetic(true);

    const double zSafe  = 5.0;
    const double zCut   = -1.0;
    const double feed   = 1000.0;

    painter.save();

    for (int i = 0; i < m_document.entityCount(); ++i) {
        const Entity *e = m_document.entityAt(i);
        if (!e || !e->isEnabled()) continue;
        // Skip if on hidden layer
        if (!m_document.isEntityVisible(e)) continue;

        const auto moves = e->toGcodeMoves(zSafe, zCut, feed);
        for (const GcodeMove &mv : moves) {
            QPointF sp = machineToScreen(QPointF(mv.start.x(), mv.start.y()));
            QPointF ep = machineToScreen(QPointF(mv.end.x(),   mv.end.y()));

            if (mv.type == GcodeMove::Type::Rapid) {
                painter.setPen(rapidPen);
                painter.drawLine(sp, ep);
            } else if (mv.type == GcodeMove::Type::Linear) {
                painter.setPen(cutPen);
                painter.drawLine(sp, ep);
            } else {
                // Arc — approximate with polyline segments
                painter.setPen(cutPen);
                double cx = mv.start.x() + mv.iOffset;
                double cy = mv.start.y() + mv.jOffset;
                double r  = std::hypot(mv.start.x() - cx, mv.start.y() - cy);
                if (r < 1e-6) { painter.drawLine(sp, ep); continue; }
                double a0 = std::atan2(mv.start.y() - cy, mv.start.x() - cx);
                double a1 = std::atan2(mv.end.y()   - cy, mv.end.x()   - cx);
                bool ccw = (mv.type == GcodeMove::Type::ArcCCW);
                if (ccw && a1 < a0) a1 += 2 * kPi;
                if (!ccw && a1 > a0) a1 -= 2 * kPi;
                int steps = std::max(8, (int)(std::abs(a1 - a0) * r * m_zoom / 4.0));
                QPointF prev = sp;
                for (int s = 1; s <= steps; ++s) {
                    double t = a0 + (a1 - a0) * s / steps;
                    QPointF pt = machineToScreen(QPointF(cx + r * std::cos(t),
                                                         cy + r * std::sin(t)));
                    painter.drawLine(prev, pt);
                    prev = pt;
                }
            }
        }
    }

    painter.restore();
}

void DesignCanvas::fitToSelection()
{
    const Entity* sel = selectedEntity();
    if (!sel) { autoFit(); return; }

    QRectF bb = sel->boundingRect();
    if (!bb.isValid() || bb.isEmpty()) { autoFit(); return; }

    // Inflate bounding box a bit
    double margin = 40.0;
    double bbW = bb.width();  if (bbW < 1.0) bbW = 1.0;
    double bbH = bb.height(); if (bbH < 1.0) bbH = 1.0;

    double scaleX = (width()  - 2 * margin) / bbW;
    double scaleY = (height() - 2 * margin) / bbH;
    m_zoom = std::min(scaleX, scaleY);

    // Center the bounding box
    // machineToScreen(x) = x * zoom + panX  =>  panX = screenX - x * zoom
    double centerX = (bb.left() + bb.right())  / 2.0;
    double centerY = (bb.top()  + bb.bottom()) / 2.0;
    m_panOffset = QPointF(width()  / 2.0 - centerX * m_zoom,
                          height() / 2.0 + centerY * m_zoom); // Y-flip: screen = -y*z + pan
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
    m_zoomSettleTimer.start();  // defer crisp re-render
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
    m_entityCacheDirty = true;
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

void DesignCanvas::contextMenuEvent(QContextMenuEvent *event)
{
    if (m_document.selectedIds().isEmpty()) {
        event->ignore();
        return;
    }

    QMenu menu(this);

    // Move to Layer submenu
    if (m_document.layerCount() > 0) {
        QMenu *moveMenu = menu.addMenu("Move to Layer");
        for (int i = 0; i < m_document.layerCount(); ++i) {
            const Layer &layer = m_document.layerAt(i);
            QAction *act = moveMenu->addAction(layer.name);
            // Show color swatch icon
            QPixmap px(12, 12);
            px.fill(layer.color);
            act->setIcon(QIcon(px));
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

    // Delete selected
    menu.addSeparator();
    QAction *delAct = menu.addAction("Delete Selected");
    connect(delAct, &QAction::triggered, this, &DesignCanvas::deleteSelectedEntity);

    // Export G-code
    menu.addSeparator();
    QAction *expAct = menu.addAction("Export G-code...");
    connect(expAct, &QAction::triggered, this, [this]() { emit exportGcodeRequested(); });

    menu.exec(event->globalPos());
}

void DesignCanvas::drawCrosshair(QPainter &painter)
{
    if (!m_mouseInWidget) return;

    painter.save();

    QPen crossPen(QColor(180, 180, 180, 100), 1.0, Qt::DashLine);
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
    painter.setBrush(QColor(30, 30, 30, 220));
    painter.drawRoundedRect(bgRect.adjusted(-2, -1, 2, 1), 3, 3);

    painter.setPen(QColor(210, 210, 210));
    painter.drawText(bgRect, Qt::AlignCenter, coordText);

    painter.restore();
}

void DesignCanvas::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.fillRect(rect(), QColor(30, 30, 30));
    painter.setRenderHint(QPainter::Antialiasing, true);

    // Rulers are drawn in screen coordinates
    drawGridAndRulers(painter);

    // --- Entity layer via pixmap cache ---
    bool cacheUsable = !m_entityCache.isNull()
                    && m_entityCacheSize == size();

    if (m_entityCacheDirty || !cacheUsable) {

        // Full re-render to cache
        m_entityCache = QPixmap(size());
        m_entityCache.fill(Qt::transparent);
        {
            QPainter cp(&m_entityCache);
            cp.setRenderHint(QPainter::Antialiasing, true);
            cp.translate(m_panOffset);
            cp.scale(m_zoom, -m_zoom);
            drawWorktable(cp);
            drawAxisOrigin(cp);
            drawEntities(cp);
        }
        m_entityCacheZoom = m_zoom;
        m_entityCachePanOffset = m_panOffset;
        m_entityCacheSize = size();
        m_entityCacheDirty = false;
    }

    if (m_entityCacheZoom == m_zoom && m_entityCachePanOffset == m_panOffset) {
        // Same view — blit directly
        painter.drawPixmap(0, 0, m_entityCache);
    } else {
        // Zoom/pan changed — scale/translate the cached pixmap (fast but approximate)
        double relativeZoom = m_zoom / m_entityCacheZoom;
        QPointF panDelta = m_panOffset - m_entityCachePanOffset * relativeZoom;
        painter.save();
        painter.translate(panDelta);
        painter.scale(relativeZoom, relativeZoom);
        painter.drawPixmap(0, 0, m_entityCache);
        painter.restore();
    }

    // Dynamic elements always drawn fresh (handles, preview, snap)
    painter.save();
    painter.translate(m_panOffset);
    painter.scale(m_zoom, -m_zoom);

    drawHandles(painter);

    painter.setPen(createPreviewPen());
    activeTool().paintPreview(painter);

    drawSnapIndicator(painter);
    painter.restore();

    drawCrosshair(painter);
    if (m_showGcodeOverlay)
        drawGcodeOverlay(painter);
    drawRulerStrips(painter);
}

void DesignCanvas::drawGridAndRulers(QPainter& painter)
{
    // Adaptive step: start from the configured grid size, scale up until visible
    double stepMachine = m_gridSize;
    while (stepMachine * m_zoom < 20.0) {
        stepMachine *= 2.0;
        if (stepMachine * m_zoom >= 20.0) break;
        stepMachine *= 2.5;
    }

    QPointF machineTL = screenToMachine(QPointF(0, 0));
    QPointF machineBR = screenToMachine(QPointF(width(), height()));

    double startX = std::floor(std::min(machineTL.x(), machineBR.x()) / stepMachine) * stepMachine;
    double endX   = std::ceil(std::max(machineTL.x(), machineBR.x()) / stepMachine) * stepMachine;
    double startY = std::floor(std::min(machineTL.y(), machineBR.y()) / stepMachine) * stepMachine;
    double endY   = std::ceil(std::max(machineTL.y(), machineBR.y()) / stepMachine) * stepMachine;

    // Grid lines — batched for performance
    QVector<QLineF> gridLines;
    int nx = static_cast<int>((endX - startX) / stepMachine) + 1;
    int ny = static_cast<int>((endY - startY) / stepMachine) + 1;
    gridLines.reserve(nx + ny);

    for (double x = startX; x <= endX; x += stepMachine) {
        gridLines.append(QLineF(machineToScreen(QPointF(x, startY)),
                                machineToScreen(QPointF(x, endY))));
    }
    for (double y = startY; y <= endY; y += stepMachine) {
        gridLines.append(QLineF(machineToScreen(QPointF(startX, y)),
                                machineToScreen(QPointF(endX, y))));
    }
    painter.setPen(QPen(QColor(70, 70, 70), 1));
    painter.drawLines(gridLines);
}

void DesignCanvas::drawRulerStrips(QPainter& painter)
{
    // Ruler strip dimensions
    const int kRulerW = 20;  // pixels for the ruler strip width
    const QColor bgColor(40, 40, 40);
    const QColor borderColor(75, 75, 75);
    const QColor tickColor(180, 180, 180);
    const QColor labelColor(180, 180, 180);

    // Adaptive step: same logic as drawGridAndRulers
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

    QFont rulerFont("Sans Serif", 7);
    painter.setFont(rulerFont);
    QFontMetrics fm(rulerFont);

    // --- Horizontal ruler strip (top) ---
    painter.fillRect(QRect(kRulerW, 0, width() - kRulerW, kRulerW), bgColor);
    painter.setPen(QPen(borderColor, 1));
    painter.drawLine(kRulerW, kRulerW, width(), kRulerW);          // bottom border
    painter.drawLine(kRulerW, 0, width(), 0);                      // top border

    for (double x = startX; x <= endX; x += stepMachine) {
        double sx = machineToScreen(QPointF(x, 0)).x();
        if (sx < kRulerW || sx > width()) continue;
        painter.setPen(QPen(tickColor, 1));
        painter.drawLine(QPointF(sx, kRulerW), QPointF(sx, kRulerW - 6));
        QString label = QString::number(x, 'g', 5);
        int lw = fm.horizontalAdvance(label);
        painter.setPen(labelColor);
        painter.drawText(QPointF(sx - lw / 2.0, kRulerW - 8), label);
    }

    // --- Vertical ruler strip (left) ---
    painter.fillRect(QRect(0, kRulerW, kRulerW, height() - kRulerW), bgColor);
    painter.setPen(QPen(borderColor, 1));
    painter.drawLine(kRulerW, kRulerW, kRulerW, height());         // right border
    painter.drawLine(0, kRulerW, 0, height());                     // left border

    for (double y = startY; y <= endY; y += stepMachine) {
        double sy = machineToScreen(QPointF(0, y)).y();
        if (sy < kRulerW || sy > height()) continue;
        painter.setPen(QPen(tickColor, 1));
        painter.drawLine(QPointF(kRulerW, sy), QPointF(kRulerW - 6, sy));
        // Draw rotated label
        QString label = QString::number(y, 'g', 5);
        painter.save();
        painter.translate(kRulerW - 8, sy);
        painter.rotate(-90);
        int lw = fm.horizontalAdvance(label);
        painter.setPen(labelColor);
        painter.drawText(QPointF(-lw / 2.0, 0), label);
        painter.restore();
    }

    // --- Corner square (covers intersection) ---
    painter.fillRect(QRect(0, 0, kRulerW, kRulerW), QColor(35, 35, 35));
    painter.setPen(QPen(borderColor, 1));
    painter.drawLine(kRulerW, 0, kRulerW, kRulerW);
    painter.drawLine(0, kRulerW, kRulerW, kRulerW);

    // --- Cursor position indicators ---
    if (m_mouseInWidget) {
        QPen cursorPen(QColor("#ef4444"), 1);
        painter.setPen(cursorPen);
        // Vertical line on horizontal ruler
        double cx = m_mouseScreenPos.x();
        if (cx >= kRulerW && cx <= width())
            painter.drawLine(QPointF(cx, 0), QPointF(cx, kRulerW));
        // Horizontal line on vertical ruler
        double cy = m_mouseScreenPos.y();
        if (cy >= kRulerW && cy <= height())
            painter.drawLine(QPointF(0, cy), QPointF(kRulerW, cy));
    }
}

void DesignCanvas::drawWorktable(QPainter& painter)
{
    // Draw worktable boundary in machine coordinates (Y-up)
    QPointF bl(0, 0);
    QPointF br(m_worktableSize.width(), 0);
    QPointF tr(m_worktableSize.width(), m_worktableSize.height());
    QPointF tl(0, m_worktableSize.height());

    // Fill worktable with paper background
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(38, 38, 38));
    painter.drawRect(QRectF(bl, tr));

    double tableW = m_worktableSize.width();
    double tableH = m_worktableSize.height();

    // Ruled horizontal lines (like lined paper)
    if (m_canvasSettings.showRuledLines && m_canvasSettings.lineSpacing > 0) {
        double spacing = m_canvasSettings.lineSpacing;
        int lineCount = static_cast<int>(tableH / spacing);

        QVector<QLineF> ruledLines;
        ruledLines.reserve(lineCount);
        for (int i = 1; i <= lineCount; ++i) {
            double y = i * spacing;
            if (y >= tableH) break;
            ruledLines.append(QLineF(QPointF(0, y), QPointF(tableW, y)));
        }
        QPen ruledPen(m_canvasSettings.lineColor);
        ruledPen.setWidthF(0.3);
        painter.setPen(ruledPen);
        painter.setBrush(Qt::NoBrush);
        painter.drawLines(ruledLines);
    }

    // Left margin line
    if (m_canvasSettings.showMarginLine) {
        QPen marginPen(m_canvasSettings.marginColor);
        marginPen.setWidthF(0.3);
        painter.setPen(marginPen);
        painter.drawLine(QPointF(m_canvasSettings.marginOffset, 0),
                         QPointF(m_canvasSettings.marginOffset, tableH));
    }

    // Worktable border
    painter.setPen(QPen(QColor(100, 140, 220), 0.5, Qt::DotLine));
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
    painter.setPen(QColor(130, 160, 220));
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
    const QSet<QUuid>& selectedIds = m_document.selectedIds();

    // Compute visible rect in machine coordinates for culling
    QPointF visTL = screenToMachine(QPointF(0, 0));
    QPointF visBR = screenToMachine(QPointF(width(), height()));
    QRectF visibleRect(QPointF(std::min(visTL.x(), visBR.x()), std::min(visTL.y(), visBR.y())),
                       QPointF(std::max(visTL.x(), visBR.x()), std::max(visTL.y(), visBR.y())));

    for (int i = 0; i < m_document.entityCount(); ++i) {
        Entity* entity = m_document.entityAt(i);
        // Skip entities on hidden layers
        if (!m_document.isEntityVisible(entity))
            continue;
        // Frustum culling: skip entities outside visible area
        if (!visibleRect.intersects(entity->boundingRect()))
            continue;
        bool isSelected = selectedIds.contains(entity->id());
        if (!entity->isEnabled()) {
            painter.setOpacity(0.3);
        }
        // Indicate locked layer entities with reduced opacity
        if (m_document.isEntityLocked(entity)) {
            painter.setOpacity(0.5);
        }
        painter.setPen(isSelected ? createSelectedPen() : [&]() {
            QPen p = createEntityPen();
            p.setColor(m_document.entityLayerColor(entity));
            return p;
        }());
        entity->draw(painter);
        painter.setOpacity(1.0);
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
    const QPointF gridPoint(std::round(rawWorldPos.x() / m_gridSize) * m_gridSize,
                            std::round(rawWorldPos.y() / m_gridSize) * m_gridSize);
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
        case ToolType::Polyline:  return m_polylineTool;
        case ToolType::Rectangle: return m_rectangleTool;
        case ToolType::Circle:    return m_circleTool;
        case ToolType::Arc:       return m_arcTool;
        case ToolType::Text:      return m_textTool;
        case ToolType::Dimension:         return m_dimensionTool;
        case ToolType::AngularDimension:   return m_angularDimensionTool;
        case ToolType::RadiusDimension:    return m_radiusDimensionTool;
        case ToolType::Trim:               return m_trimTool;
        case ToolType::Extend:             return m_extendTool;
        case ToolType::Chamfer:            return m_chamferTool;
        case ToolType::Fillet:             return m_filletTool;
        case ToolType::Offset:             return m_offsetTool;
        case ToolType::Line:
        default:                  return m_lineTool;
    }
}

const Tool& DesignCanvas::activeTool() const
{
    switch (m_activeToolType) {
        case ToolType::Select:    return m_selectTool;
        case ToolType::Polyline:  return m_polylineTool;
        case ToolType::Rectangle: return m_rectangleTool;
        case ToolType::Circle:    return m_circleTool;
        case ToolType::Arc:       return m_arcTool;
        case ToolType::Text:      return m_textTool;
        case ToolType::Dimension:         return m_dimensionTool;
        case ToolType::AngularDimension:  return m_angularDimensionTool;
        case ToolType::RadiusDimension:   return m_radiusDimensionTool;
        case ToolType::Trim:              return m_trimTool;
        case ToolType::Extend:            return m_extendTool;
        case ToolType::Chamfer:           return m_chamferTool;
        case ToolType::Fillet:            return m_filletTool;
        case ToolType::Offset:            return m_offsetTool;
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

void DesignCanvas::copySelectedEntity()
{
    const Entity* selected = selectedEntity();
    if (!selected) {
        return;
    }

    m_copiedEntity = selected->clone();
    m_pasteSerial = 0;
}

void DesignCanvas::pasteCopiedEntity()
{
    if (!m_copiedEntity) {
        return;
    }

    auto entity = m_copiedEntity->clone();
    ++m_pasteSerial;
    entity->translate(shortcutOffsetForStep(m_pasteSerial));
    addEntityFromShortcut(std::move(entity));
}

void DesignCanvas::duplicateSelectedEntity()
{
    const Entity* selected = selectedEntity();
    if (!selected) {
        return;
    }

    auto entity = selected->clone();
    entity->translate(shortcutOffsetForStep(1));
    addEntityFromShortcut(std::move(entity));

    m_copiedEntity = selected->clone();
    m_pasteSerial = 1;
}

void DesignCanvas::addEntityFromShortcut(std::unique_ptr<Entity> entity)
{
    if (!entity) {
        return;
    }

    const int entityCountBefore = m_document.entityCount();
    m_commandStack.execute(std::make_unique<AddEntityCommand>(std::move(entity)));

    if (m_document.entityCount() > entityCountBefore) {
        m_document.clearSelection();
        if (Entity* created = m_document.entityAt(entityCountBefore)) {
            m_document.select(created->id());
        }
    }

    setTool(ToolType::Select);
    emitToolStatus();
    emitUndoRedoState();
    scheduleUpdate();
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

        // Pre-resolve handle hit in screen space for SelectTool
        if (m_activeToolType == ToolType::Select) {
            auto handle = hitHandle(rawWorldPos);
            m_selectTool.setHitHandleId(handle ? handle->id : QString());
            m_selectTool.setCtrlPressed(event->modifiers() & Qt::ControlModifier);
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
        m_panOffset += QPointF(delta.x(), delta.y());
        m_lastPanScreenPos = event->position().toPoint();
        m_zoomSettleTimer.start();  // defer crisp re-render
        update();
        return;
    }

    // Skip snap and hover during entity drag for performance
    bool isDragging = (m_activeToolType == ToolType::Select && m_selectTool.isActive());

    if (m_activeToolType == ToolType::Select && !isDragging) {
        updateHoverCursor(rawWorldPos);
    }

    if (m_snapEnabled && !isDragging) {
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

void DesignCanvas::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton || m_activeToolType != ToolType::Polyline) {
        QWidget::mouseDoubleClickEvent(event);
        return;
    }

    const int entityCountBefore = m_document.entityCount();
    if (m_snapEnabled) {
        m_snapResult = computeSnap(toWorld(event->position()), true);
    } else {
        m_snapResult = {};
    }

    if (m_polylineTool.finish() && m_document.entityCount() > entityCountBefore) {
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
    applyZoom(factor, event->position());
    event->accept();
}

void DesignCanvas::keyPressEvent(QKeyEvent *event)
{
    if (m_activeToolType == ToolType::Polyline) {
        if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
            const int entityCountBefore = m_document.entityCount();
            if (m_polylineTool.finish() && m_document.entityCount() > entityCountBefore) {
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

        if (event->key() == Qt::Key_Backspace && m_polylineTool.removeLastVertex()) {
            emitToolStatus();
            scheduleUpdate();
            event->accept();
            return;
        }
    }

    if (event->matches(QKeySequence::Copy)) {
        copySelectedEntity();
        event->accept();
        return;
    }

    if (event->matches(QKeySequence::Paste)) {
        pasteCopiedEntity();
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_D && event->modifiers() == Qt::ControlModifier) {
        duplicateSelectedEntity();
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

    QWidget::keyPressEvent(event);
}