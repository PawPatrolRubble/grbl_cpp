#include "CanvasDrawing.h"
#include "../../grbl/GraphicsDesign/LineEntity.h"
#include "../../grbl/GraphicsDesign/CircleEntity.h"
#include "../../grbl/GraphicsDesign/ArcEntity.h"
#include "../../grbl/GraphicsDesign/RectangleEntity.h"
#include "../../grbl/GraphicsDesign/PolylineEntity.h"

#include <QFontMetrics>
#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

namespace {
constexpr double kPi = 3.14159265358979323846;

QPen entityPen(const QColor& layerColor) {
    QPen pen(layerColor);
    pen.setWidth(2);
    pen.setCosmetic(true);
    return pen;
}

QPen selectedPen() {
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

/// Project point onto line segment, return perpendicular point if within segment.
/// Returns nullopt if the perpendicular falls outside the segment bounds.
std::optional<QPointF> perpendicularPointOnLine(const QPointF& p, const QPointF& a, const QPointF& b) {
    const QPointF ab = b - a;
    const double len2 = ab.x() * ab.x() + ab.y() * ab.y();
    if (len2 < 1e-10) return std::nullopt; // Degenerate line
    
    const QPointF ap = p - a;
    const double t = (ap.x() * ab.x() + ap.y() * ab.y()) / len2;
    
    // Only snap if perpendicular falls within segment (not beyond endpoints)
    if (t >= 0.0 && t <= 1.0) {
        return a + t * ab;
    }
    return std::nullopt;
}

/// Distance from point p to line segment a-b.
double pointToSegmentDist(const QPointF& p, const QPointF& a, const QPointF& b) {
    const QPointF ab = b - a;
    const double len2 = ab.x() * ab.x() + ab.y() * ab.y();
    if (len2 < 1e-10) return std::hypot(p.x() - a.x(), p.y() - a.y());
    const QPointF ap = p - a;
    const double t = std::clamp((ap.x() * ab.x() + ap.y() * ab.y()) / len2, 0.0, 1.0);
    const QPointF proj = a + t * ab;
    return std::hypot(p.x() - proj.x(), p.y() - proj.y());
}

/// Approximate geometric distance from mouse to entity (world units).
double mouseToEntityDist(const QPointF& mouse, const Entity* entity) {
    const auto kind = entity->kind();
    if (kind == Entity::Kind::Line) {
        const auto* line = static_cast<const LineEntity*>(entity);
        return pointToSegmentDist(mouse, line->startPoint(), line->endPoint());
    }
    if (kind == Entity::Kind::Circle) {
        const auto* circle = static_cast<const CircleEntity*>(entity);
        const double d = std::hypot(mouse.x() - circle->center().x(), mouse.y() - circle->center().y());
        return std::abs(d - circle->radius());
    }
    if (kind == Entity::Kind::Arc) {
        const auto* arc = static_cast<const ArcEntity*>(entity);
        const double d = std::hypot(mouse.x() - arc->center().x(), mouse.y() - arc->center().y());
        // Distance to the arc's circle circumference regardless of sweep
        return std::abs(d - arc->radius());
    }
    if (kind == Entity::Kind::Rectangle) {
        const auto* rect = static_cast<const RectangleEntity*>(entity);
        const auto c = rect->corners();
        double minDist = std::numeric_limits<double>::max();
        for (int e = 0; e < 4; ++e)
            minDist = std::min(minDist, pointToSegmentDist(mouse, c[e], c[(e + 1) % 4]));
        return minDist;
    }
    if (kind == Entity::Kind::Polygon) {
        const auto* poly = static_cast<const PolylineEntity*>(entity);
        const auto& pts = poly->points();
        double minDist = std::numeric_limits<double>::max();
        for (int s = 0; s + 1 < pts.size(); ++s)
            minDist = std::min(minDist, pointToSegmentDist(mouse, pts[s], pts[s + 1]));
        return minDist;
    }
    // Fallback: distance to bounding rect center
    const QRectF b = entity->boundingRect();
    return std::hypot(mouse.x() - b.center().x(), mouse.y() - b.center().y());
}
/// Draw a distinct snap symbol at the given world point.
/// Each snap type uses a unique shape, matching common CAD conventions.
void drawSnapSymbol(QPainter& painter, const QString& label, const QPointF& pt,
                    bool isActive, double invZoom)
{
    const double s = (isActive ? 7.0 : 5.0) * invZoom; // half-size in world units
    const QColor activeColor(255, 220, 50);           // bright yellow when snapping
    const QColor inactiveColor(140, 140, 140, 200);   // gray for candidate

    const QColor color = isActive ? activeColor : inactiveColor;
    const double pw = 1.5 * invZoom; // pen width in world units

    QPen pen(color, pw);
    pen.setCosmetic(false);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);

    if (label == "Endpoint" || label == "Corner" || label == "Start" || label == "End") {
        // Square box
        painter.drawRect(QRectF(pt.x() - s, pt.y() - s, 2.0 * s, 2.0 * s));

    } else if (label == "Midpoint") {
        // Upward-pointing equilateral triangle
        const double h = s * 1.732; // sqrt(3) * s
        QPolygonF tri;
        tri << QPointF(pt.x(),       pt.y() + h * 0.667)
            << QPointF(pt.x() + s,   pt.y() - h * 0.333)
            << QPointF(pt.x() - s,   pt.y() - h * 0.333);
        painter.drawPolygon(tri);

    } else if (label == "Center") {
        // Circle
        painter.drawEllipse(pt, s, s);

    } else if (label == "Quadrant") {
        // Diamond (rotated square)
        QPolygonF diamond;
        diamond << QPointF(pt.x(),     pt.y() + s)
                << QPointF(pt.x() + s, pt.y())
                << QPointF(pt.x(),     pt.y() - s)
                << QPointF(pt.x() - s, pt.y());
        diamond << diamond[0]; // close
        painter.drawPolyline(diamond);

    } else if (label == "Intersection") {
        // X-cross (two diagonal lines)
        painter.drawLine(QPointF(pt.x() - s, pt.y() - s), QPointF(pt.x() + s, pt.y() + s));
        painter.drawLine(QPointF(pt.x() + s, pt.y() - s), QPointF(pt.x() - s, pt.y() + s));

    } else if (label == "Perpendicular") {
        // Right-angle mark: L-shape with a small square at corner
        const double ls = s * 1.2;
        painter.drawLine(QPointF(pt.x(),      pt.y() - ls), QPointF(pt.x(),      pt.y()));
        painter.drawLine(QPointF(pt.x(),      pt.y()),       QPointF(pt.x() + ls, pt.y()));
        // tiny corner square
        const double cs = ls * 0.3;
        painter.drawRect(QRectF(pt.x(), pt.y() - cs, cs, cs));

    } else if (label == "Nearest") {
        // Hourglass: two triangles (top and bottom)
        painter.drawLine(QPointF(pt.x() - s, pt.y() + s), QPointF(pt.x() + s, pt.y() - s));
        painter.drawLine(QPointF(pt.x() - s, pt.y() - s * 0.3), QPointF(pt.x() + s, pt.y() - s * 0.3));
        painter.drawLine(QPointF(pt.x() - s, pt.y() + s * 0.3), QPointF(pt.x() + s, pt.y() + s * 0.3));

    } else if (label == "Tangent") {
        // Circle with a tangent horizontal line through it
        painter.drawEllipse(pt, s * 0.7, s * 0.7);
        painter.drawLine(QPointF(pt.x() - s * 1.2, pt.y() + s), QPointF(pt.x() + s * 1.2, pt.y() + s));

    } else {
        // Fallback: small circle
        painter.drawEllipse(pt, s * 0.6, s * 0.6);
    }

    // When active, add a highlight ring
    if (isActive) {
        QPen ringPen(QColor(255, 220, 50, 120), pw * 0.8);
        ringPen.setCosmetic(false);
        painter.setPen(ringPen);
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(pt, s * 1.5, s * 1.5);
    }
}
} // namespace

CanvasDrawing::CanvasDrawing(Document& document, const CoordinateTransform& transforms)
    : m_document(document)
    , m_transforms(transforms)
{
}

bool CanvasDrawing::isCacheValid(double zoom, const QPointF& pan, const QSize& size) const
{
    return !m_entityCacheDirty
        && !m_entityCache.isNull()
        && m_entityCacheSize == size
        && m_entityCacheZoom == zoom
        && m_entityCachePanOffset == pan;
}

void CanvasDrawing::renderCache(double zoom, const QPointF& pan, const QSize& size)
{
    m_entityCache = QPixmap(size);
    m_entityCache.fill(Qt::transparent);
    {
        QPainter cp(&m_entityCache);
        cp.setRenderHint(QPainter::Antialiasing, true);
        cp.translate(pan);
        cp.scale(zoom, -zoom);
        drawWorktable(cp);
        drawAxisOrigin(cp);
        drawEntities(cp, size);
    }
    m_entityCacheZoom = zoom;
    m_entityCachePanOffset = pan;
    m_entityCacheSize = size;
    m_entityCacheDirty = false;
}

void CanvasDrawing::blitCache(QPainter& widgetPainter) const
{
    widgetPainter.drawPixmap(0, 0, m_entityCache);
}

void CanvasDrawing::blitCacheTransformed(QPainter& widgetPainter,
                                          double currentZoom, const QPointF& currentPan) const
{
    double relativeZoom = currentZoom / m_entityCacheZoom;
    QPointF panDelta = currentPan - m_entityCachePanOffset * relativeZoom;
    widgetPainter.save();
    widgetPainter.translate(panDelta);
    widgetPainter.scale(relativeZoom, relativeZoom);
    widgetPainter.drawPixmap(0, 0, m_entityCache);
    widgetPainter.restore();
}

// ============================================================================
// Grid and Rulers
// ============================================================================

void CanvasDrawing::drawGridAndRulers(QPainter& painter, const QSize& widgetSize) const
{
    double stepMachine = m_gridSize;
    while (stepMachine * m_transforms.zoom() < 20.0) {
        stepMachine *= 2.0;
        if (stepMachine * m_transforms.zoom() >= 20.0) break;
        stepMachine *= 2.5;
    }

    QPointF machineTL = m_transforms.screenToMachine(QPointF(0, 0));
    QPointF machineBR = m_transforms.screenToMachine(QPointF(widgetSize.width(), widgetSize.height()));

    double startX = std::floor(std::min(machineTL.x(), machineBR.x()) / stepMachine) * stepMachine;
    double endX   = std::ceil(std::max(machineTL.x(), machineBR.x()) / stepMachine) * stepMachine;
    double startY = std::floor(std::min(machineTL.y(), machineBR.y()) / stepMachine) * stepMachine;
    double endY   = std::ceil(std::max(machineTL.y(), machineBR.y()) / stepMachine) * stepMachine;

    QVector<QLineF> gridLines;
    int nx = static_cast<int>((endX - startX) / stepMachine) + 1;
    int ny = static_cast<int>((endY - startY) / stepMachine) + 1;
    gridLines.reserve(nx + ny);

    for (double x = startX; x <= endX; x += stepMachine) {
        gridLines.append(QLineF(m_transforms.machineToScreen(QPointF(x, startY)),
                                m_transforms.machineToScreen(QPointF(x, endY))));
    }
    for (double y = startY; y <= endY; y += stepMachine) {
        gridLines.append(QLineF(m_transforms.machineToScreen(QPointF(startX, y)),
                                m_transforms.machineToScreen(QPointF(endX, y))));
    }
    painter.setPen(QPen(QColor(210, 210, 210), 1));
    painter.drawLines(gridLines);
}

// ============================================================================
// Ruler Strips
// ============================================================================

void CanvasDrawing::drawRulerStrips(QPainter& painter, const QSize& widgetSize,
                                     const QPointF& mouseScreenPos, bool mouseInWidget) const
{
    const int kRulerW = 20;
    const QColor bgColor(240, 240, 240);
    const QColor borderColor(190, 190, 190);
    const QColor tickColor(120, 120, 120);
    const QColor labelColor(100, 100, 100);

    double stepMachine = 10.0;
    while (stepMachine * m_transforms.zoom() < 30.0) {
        stepMachine *= 2.0;
        if (stepMachine * m_transforms.zoom() >= 30.0) break;
        stepMachine *= 2.5;
    }

    QPointF machineTL = m_transforms.screenToMachine(QPointF(0, 0));
    QPointF machineBR = m_transforms.screenToMachine(QPointF(widgetSize.width(), widgetSize.height()));
    double startX = std::floor(std::min(machineTL.x(), machineBR.x()) / stepMachine) * stepMachine;
    double endX   = std::ceil(std::max(machineTL.x(), machineBR.x()) / stepMachine) * stepMachine;
    double startY = std::floor(std::min(machineTL.y(), machineBR.y()) / stepMachine) * stepMachine;
    double endY   = std::ceil(std::max(machineTL.y(), machineBR.y()) / stepMachine) * stepMachine;

    QFont rulerFont("Sans Serif", 7);
    painter.setFont(rulerFont);
    QFontMetrics fm(rulerFont);

    // --- Horizontal ruler strip (top) ---
    painter.fillRect(QRect(kRulerW, 0, widgetSize.width() - kRulerW, kRulerW), bgColor);
    painter.setPen(QPen(borderColor, 1));
    painter.drawLine(kRulerW, kRulerW, widgetSize.width(), kRulerW);
    painter.drawLine(kRulerW, 0, widgetSize.width(), 0);

    for (double x = startX; x <= endX; x += stepMachine) {
        double sx = m_transforms.machineToScreen(QPointF(x, 0)).x();
        if (sx < kRulerW || sx > widgetSize.width()) continue;
        painter.setPen(QPen(tickColor, 1));
        painter.drawLine(QPointF(sx, kRulerW), QPointF(sx, kRulerW - 6));
        QString label = QString::number(x, 'g', 5);
        int lw = fm.horizontalAdvance(label);
        painter.setPen(labelColor);
        painter.drawText(QPointF(sx - lw / 2.0, kRulerW - 8), label);
    }

    // --- Vertical ruler strip (left) ---
    painter.fillRect(QRect(0, kRulerW, kRulerW, widgetSize.height() - kRulerW), bgColor);
    painter.setPen(QPen(borderColor, 1));
    painter.drawLine(kRulerW, kRulerW, kRulerW, widgetSize.height());
    painter.drawLine(0, kRulerW, 0, widgetSize.height());

    for (double y = startY; y <= endY; y += stepMachine) {
        double sy = m_transforms.machineToScreen(QPointF(0, y)).y();
        if (sy < kRulerW || sy > widgetSize.height()) continue;
        painter.setPen(QPen(tickColor, 1));
        painter.drawLine(QPointF(kRulerW, sy), QPointF(kRulerW - 6, sy));
        QString label = QString::number(y, 'g', 5);
        painter.save();
        painter.translate(kRulerW - 8, sy);
        painter.rotate(-90);
        int lw = fm.horizontalAdvance(label);
        painter.setPen(labelColor);
        painter.drawText(QPointF(-lw / 2.0, 0), label);
        painter.restore();
    }

    // --- Corner square ---
    painter.fillRect(QRect(0, 0, kRulerW, kRulerW), QColor(230, 230, 230));
    painter.setPen(QPen(borderColor, 1));
    painter.drawLine(kRulerW, 0, kRulerW, kRulerW);
    painter.drawLine(0, kRulerW, kRulerW, kRulerW);

    // --- Cursor position indicators ---
    if (mouseInWidget) {
        QPen cursorPen(QColor("#ef4444"), 1);
        painter.setPen(cursorPen);
        double cx = mouseScreenPos.x();
        if (cx >= kRulerW && cx <= widgetSize.width())
            painter.drawLine(QPointF(cx, 0), QPointF(cx, kRulerW));
        double cy = mouseScreenPos.y();
        if (cy >= kRulerW && cy <= widgetSize.height())
            painter.drawLine(QPointF(0, cy), QPointF(kRulerW, cy));
    }
}

// ============================================================================
// G-code Overlay
// ============================================================================

void CanvasDrawing::drawGcodeOverlay(QPainter& painter, const QSize& widgetSize) const
{
    QPen rapidPen(QColor(6, 182, 212, 160), 1, Qt::DashLine);
    rapidPen.setCosmetic(true);
    QPen cutPen(QColor(249, 115, 22, 200), 1.5);
    cutPen.setCosmetic(true);

    const double zSafe = 5.0;
    const double zCut  = -1.0;
    const double feed  = 1000.0;

    painter.save();

    for (int i = 0; i < m_document.entityCount(); ++i) {
        const Entity *e = m_document.entityAt(i);
        if (!e || !e->isEnabled()) continue;
        if (!m_document.isEntityVisible(e)) continue;

        const auto moves = e->toGcodeMoves(zSafe, zCut, feed);
        for (const GcodeMove &mv : moves) {
            QPointF sp = m_transforms.machineToScreen(QPointF(mv.start.x(), mv.start.y()));
            QPointF ep = m_transforms.machineToScreen(QPointF(mv.end.x(), mv.end.y()));

            if (mv.type == GcodeMove::Type::Rapid) {
                painter.setPen(rapidPen);
                painter.drawLine(sp, ep);
            } else if (mv.type == GcodeMove::Type::Linear) {
                painter.setPen(cutPen);
                painter.drawLine(sp, ep);
            } else {
                painter.setPen(cutPen);
                double cx = mv.start.x() + mv.iOffset;
                double cy = mv.start.y() + mv.jOffset;
                double r  = std::hypot(mv.start.x() - cx, mv.start.y() - cy);
                if (r < 1e-6) { painter.drawLine(sp, ep); continue; }
                double a0 = std::atan2(mv.start.y() - cy, mv.start.x() - cx);
                double a1 = std::atan2(mv.end.y() - cy, mv.end.x() - cx);
                bool ccw = (mv.type == GcodeMove::Type::ArcCCW);
                if (ccw && a1 < a0) a1 += 2 * kPi;
                if (!ccw && a1 > a0) a1 -= 2 * kPi;
                int steps = std::max(8, (int)(std::abs(a1 - a0) * r * m_transforms.zoom() / 4.0));
                QPointF prev = sp;
                for (int s = 1; s <= steps; ++s) {
                    double t = a0 + (a1 - a0) * s / steps;
                    QPointF pt = m_transforms.machineToScreen(QPointF(cx + r * std::cos(t),
                                                                      cy + r * std::sin(t)));
                    painter.drawLine(prev, pt);
                    prev = pt;
                }
            }
        }
    }

    painter.restore();
}

// ============================================================================
// Worktable
// ============================================================================

void CanvasDrawing::drawWorktable(QPainter& painter) const
{
    QPointF bl(0, 0);
    QPointF br(m_worktableSize.width(), 0);
    QPointF tr(m_worktableSize.width(), m_worktableSize.height());
    QPointF tl(0, m_worktableSize.height());

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(255, 255, 255));
    painter.drawRect(QRectF(bl, tr));

    double tableW = m_worktableSize.width();
    double tableH = m_worktableSize.height();

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

    if (m_canvasSettings.showMarginLine) {
        QPen marginPen(m_canvasSettings.marginColor);
        marginPen.setWidthF(0.3);
        painter.setPen(marginPen);
        painter.drawLine(QPointF(m_canvasSettings.marginOffset, 0),
                         QPointF(m_canvasSettings.marginOffset, tableH));
    }

    painter.setPen(QPen(QColor(100, 140, 220), 0.5, Qt::DotLine));
    painter.drawLine(bl, br);
    painter.drawLine(br, tr);
    painter.drawLine(tr, tl);
    painter.drawLine(tl, bl);

    painter.save();
    double invZoom = 1.0 / m_transforms.zoom();
    QFont labelFont("Sans Serif", 8);
    painter.setFont(labelFont);
    painter.translate(tr);
    painter.scale(invZoom, -invZoom);
    painter.setPen(QColor(130, 160, 220));
    painter.drawText(QPointF(4, 12), QString("%1 x %2 mm")
                     .arg(m_worktableSize.width(), 0, 'f', 0)
                     .arg(m_worktableSize.height(), 0, 'f', 0));
    painter.restore();
}

// ============================================================================
// Axis Origin
// ============================================================================

void CanvasDrawing::drawAxisOrigin(QPainter& painter) const
{
    QPointF origin(0, 0);
    double invZoom = 1.0 / m_transforms.zoom();
    double arrowLen = 30.0 * invZoom;
    double headLen = 5.0 * invZoom;

    painter.setPen(QPen(QColor("#f38ba8"), 2));
    QPointF xEnd = origin + QPointF(arrowLen, 0);
    painter.drawLine(origin, xEnd);
    painter.drawLine(xEnd, xEnd + QPointF(-headLen, -headLen));
    painter.drawLine(xEnd, xEnd + QPointF(-headLen, headLen));

    painter.save();
    painter.translate(xEnd + QPointF(3.0 * invZoom, 0));
    painter.scale(invZoom, -invZoom);
    QFont labelFont("Sans Serif", 9);
    labelFont.setBold(true);
    painter.setFont(labelFont);
    painter.drawText(QPointF(0, 4), "X");
    painter.restore();

    painter.setPen(QPen(QColor("#a6e3a1"), 2));
    QPointF yEnd = origin + QPointF(0, arrowLen);
    painter.drawLine(origin, yEnd);
    painter.drawLine(yEnd, yEnd + QPointF(-headLen, -headLen));
    painter.drawLine(yEnd, yEnd + QPointF(headLen, -headLen));

    painter.save();
    painter.translate(yEnd + QPointF(0, 3.0 * invZoom));
    painter.scale(invZoom, -invZoom);
    painter.setPen(QColor("#a6e3a1"));
    painter.setFont(labelFont);
    painter.drawText(QPointF(-4, 0), "Y");
    painter.restore();
}

// ============================================================================
// Entities
// ============================================================================

void CanvasDrawing::drawEntities(QPainter& painter, const QSize& widgetSize) const
{
    const QSet<QUuid>& selectedIds = m_document.selectedIds();

    QPointF visTL = m_transforms.screenToMachine(QPointF(0, 0));
    QPointF visBR = m_transforms.screenToMachine(QPointF(widgetSize.width(), widgetSize.height()));
    QRectF visibleRect(QPointF(std::min(visTL.x(), visBR.x()), std::min(visTL.y(), visBR.y())),
                       QPointF(std::max(visTL.x(), visBR.x()), std::max(visTL.y(), visBR.y())));

    for (int i = 0; i < m_document.entityCount(); ++i) {
        Entity* entity = m_document.entityAt(i);
        if (!m_document.isEntityVisible(entity))
            continue;
        if (!visibleRect.intersects(entity->boundingRect()))
            continue;
        if (!entity->isEnabled()) {
            painter.setOpacity(0.3);
        }
        if (m_document.isEntityLocked(entity)) {
            painter.setOpacity(0.5);
        }
        bool isSelected = selectedIds.contains(entity->id());
        painter.setPen(isSelected ? selectedPen() : entityPen(m_document.entityLayerColor(entity)));
        entity->draw(painter);
        painter.setOpacity(1.0);
    }
}

// ============================================================================
// Handles
// ============================================================================

void CanvasDrawing::drawHandles(QPainter& painter,
                                 const QString& hoverHandleId,
                                 const std::vector<Entity::EditHandle>& handles) const
{
    painter.save();
    const double halfSize = 6.0 / m_transforms.zoom();

    for (const auto& handle : handles) {
        const bool rotate = isRotateRole(handle.role);
        const bool hovered = (hoverHandleId == handle.id);

        QPen pen(rotate ? QColor(110, 64, 170) : QColor(53, 90, 120), 1.5 / m_transforms.zoom());
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

// ============================================================================
// Snap Indicator
// ============================================================================

void CanvasDrawing::drawSnapIndicator(QPainter& painter, QPointF snapWorldPos, bool snapActive,
                                      const QString& snapLabel) const
{
    if (!snapActive) return;

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);

    // Draw the type-specific snap symbol (active/highlighted state)
    drawSnapSymbol(painter, snapLabel.isEmpty() ? QString("Point") : snapLabel,
                   snapWorldPos, true, 1.0 / m_transforms.zoom());

    painter.restore();
}

// ============================================================================
// Entity Snap Markers (AutoSnap preview)
// ============================================================================

void CanvasDrawing::drawEntitySnapMarkers(QPainter& painter, const QPointF& mouseWorldPos,
                                           const QString& activeSnapLabel,
                                           const QPointF& activeSnapPos,
                                           bool snapActive,
                                           bool snapEnabled,
                                           bool endpointSnap,
                                           bool midpointSnap,
                                           bool centerSnap,
                                           bool perpendicularSnap,
                                           std::optional<QPointF> perpRef,
                                           const QSet<QUuid>& excludedIds) const
{
    // If snap is disabled globally, don't draw any markers
    if (!snapEnabled) return;
    
    // Threshold in screen pixels converted to world units
    const double screenThreshold = 60.0;  // pixels - how close mouse must be to entity to show markers
    const double worldThreshold = screenThreshold / m_transforms.zoom();

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);

    // Find the single closest entity within the world threshold.
    // Only that entity's snap markers are drawn, avoiding visual clutter.
    int closestIndex = -1;
    double closestDist = worldThreshold;

    for (int i = 0; i < m_document.entityCount(); ++i) {
        Entity* entity = m_document.entityAt(i);
        if (!entity || !entity->isEnabled() || excludedIds.contains(entity->id())) continue;

        // Quick bounding-rect early-exit with generous padding
        const QRectF bounds = entity->boundingRect();
        const QRectF padded(bounds.x() - worldThreshold, bounds.y() - worldThreshold,
                            bounds.width() + 2.0 * worldThreshold,
                            bounds.height() + 2.0 * worldThreshold);
        if (!padded.contains(mouseWorldPos)) continue;

        const double dist = mouseToEntityDist(mouseWorldPos, entity);
        if (dist < closestDist) {
            closestDist = dist;
            closestIndex = i;
        }
    }

    if (closestIndex < 0) {
        painter.restore();
        return;
    }

    // Draw snap markers for the closest entity only
    {
        Entity* entity = m_document.entityAt(closestIndex);
        const auto snapPoints = entity->snapPoints();
        const auto kind = entity->kind();

        for (std::size_t idx = 0; idx < snapPoints.size(); ++idx) {
            const QPointF& pt = snapPoints[idx];
            
            // Determine snap type label for this point
            QString label;
            if (kind == Entity::Kind::Line) {
                label = idx < 2 ? "Endpoint" : "Midpoint";
            } else if (kind == Entity::Kind::Rectangle) {
                // Indices: 0-3 corners, 4 center, 5-8 edge midpoints
                if (idx < 4) label = "Corner";
                else if (idx == 4) label = "Center";
                else label = "Midpoint";
            } else if (kind == Entity::Kind::Circle) {
                label = idx == 0 ? "Center" : "Quadrant";
            } else if (kind == Entity::Kind::Arc) {
                // Indices: 0 center, 1 start, 2 end, 3 midpoint
                if (idx == 0) label = "Center";
                else if (idx == 1) label = "Start";
                else if (idx == 2) label = "End";
                else label = "Midpoint";
            } else if (kind == Entity::Kind::Polygon) {
                // Polyline: vertices first, then segment midpoints
                const std::size_t total = snapPoints.size();
                const std::size_t vertexCount = (total + 1) / 2;
                label = idx < vertexCount ? "Endpoint" : "Midpoint";
            } else {
                label = "Point";
            }

            // Skip marker if this snap type is disabled
            if ((label == "Endpoint" || label == "Start" || label == "End" || label == "Corner") && !endpointSnap) continue;
            if (label == "Midpoint" && !midpointSnap) continue;
            if ((label == "Center" || label == "Quadrant") && !centerSnap) continue;

            // Determine if this is the active snap point
            // Use distance threshold for robust matching (floating point equality can fail)
            const double distToActiveSnap = QLineF(pt, activeSnapPos).length();
            const bool isActive = snapActive && distToActiveSnap < 0.1;

            drawSnapSymbol(painter, label, pt, isActive, 1.0 / m_transforms.zoom());
        }
        
        // --- Perpendicular snap points (reference-based, only when first point is placed) ---
        // Only compute if perpendicular snap is enabled and a reference point exists
        if (perpendicularSnap && perpRef && snapActive && activeSnapLabel == "Perpendicular") {
        const QPointF& ref = *perpRef;
        // Helper lambda to draw a perpendicular marker (skips degenerate zero-length candidates)
        auto drawPerpMarker = [&](const std::optional<QPointF>& perp) {
            if (!perp) return;
            // Filter: skip if perpendicular point coincides with the reference (zero-length line)
            const double screenDist = QLineF(m_transforms.toScreen(ref), m_transforms.toScreen(*perp)).length();
            if (screenDist < 2.0) return;
            const double distToActiveSnap = QLineF(*perp, activeSnapPos).length();
            const bool isActive = snapActive && distToActiveSnap < 0.1 && activeSnapLabel == "Perpendicular";
            drawSnapSymbol(painter, "Perpendicular", *perp, isActive, 1.0 / m_transforms.zoom());
        };
        
        // LineEntity: perpendicular to line segment
        if (kind == Entity::Kind::Line) {
            auto* line = static_cast<LineEntity*>(entity);
            drawPerpMarker(perpendicularPointOnLine(ref, line->startPoint(), line->endPoint()));
        }
        
        // RectangleEntity: perpendicular to each edge
        if (kind == Entity::Kind::Rectangle) {
            auto* rect = static_cast<RectangleEntity*>(entity);
            const auto c = rect->corners();
            // Edges: c[0]-c[1], c[1]-c[2], c[2]-c[3], c[3]-c[0]
            for (int edge = 0; edge < 4; ++edge)
                drawPerpMarker(perpendicularPointOnLine(ref, c[edge], c[(edge + 1) % 4]));
        }
        
        // PolylineEntity: perpendicular to each segment
        if (kind == Entity::Kind::Polygon) {
            auto* poly = static_cast<PolylineEntity*>(entity);
            const auto& pts = poly->points();
            for (int seg = 0; seg + 1 < pts.size(); ++seg)
                drawPerpMarker(perpendicularPointOnLine(ref, pts[seg], pts[seg + 1]));
        }
        } // end if (perpendicularSnap && perpRef)
    } // end closest entity block

    painter.restore();
}

void CanvasDrawing::drawInferenceGuide(QPainter& painter,
                                      const QPointF& point,
                                      const QString& label,
                                      std::optional<QPointF> referencePoint) const
{
    if (label.isEmpty())
        return;

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);

    QPen guidePen(QColor(59, 130, 246, 190), 1.5 / m_transforms.zoom(), Qt::DashLine);
    painter.setPen(guidePen);

    if (referencePoint && (label == "Horizontal" || label == "Vertical")) {
        painter.drawLine(*referencePoint, point);
        painter.setBrush(QColor(59, 130, 246, 90));
        painter.drawEllipse(*referencePoint, 3.0 / m_transforms.zoom(), 3.0 / m_transforms.zoom());
    }

    painter.restore();
}

// ============================================================================
// Crosshair
// ============================================================================

void CanvasDrawing::drawCrosshair(QPainter& painter, const QPointF& mouseScreenPos,
                                   bool mouseInWidget, const QSize& widgetSize) const
{
    if (!mouseInWidget) return;

    painter.save();

    QPen crossPen(QColor(160, 160, 160, 150), 1.0, Qt::DashLine);
    painter.setPen(crossPen);
    painter.drawLine(QPointF(mouseScreenPos.x(), 0),
                     QPointF(mouseScreenPos.x(), widgetSize.height()));
    painter.drawLine(QPointF(0, mouseScreenPos.y()),
                     QPointF(widgetSize.width(), mouseScreenPos.y()));

    QPointF machPos = m_transforms.toWorld(mouseScreenPos);
    QString coordText = QString("X:%1  Y:%2")
                            .arg(machPos.x(), 0, 'f', 2)
                            .arg(machPos.y(), 0, 'f', 2);

    QFont font("Consolas", 9);
    painter.setFont(font);
    QFontMetrics fm(font);
    QRect textRect = fm.boundingRect(coordText);
    textRect.adjust(-4, -2, 4, 2);

    double labelX = mouseScreenPos.x() + 12;
    double labelY = mouseScreenPos.y() - 12;
    if (labelX + textRect.width() > widgetSize.width())
        labelX = mouseScreenPos.x() - textRect.width() - 12;
    if (labelY - textRect.height() < 0)
        labelY = mouseScreenPos.y() + textRect.height() + 4;

    QRectF bgRect(labelX, labelY - textRect.height(), textRect.width(), textRect.height());
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(255, 255, 255, 230));
    painter.drawRoundedRect(bgRect.adjusted(-2, -1, 2, 1), 3, 3);

    painter.setPen(QColor(50, 50, 50));
    painter.drawText(bgRect, Qt::AlignCenter, coordText);

    painter.restore();
}

// ============================================================================
// Pen Factories (static)
// ============================================================================

QPen CanvasDrawing::createEntityPen(const QColor& layerColor) {
    return ::entityPen(layerColor);
}

QPen CanvasDrawing::createSelectedPen() {
    return ::selectedPen();
}

QPen CanvasDrawing::createPreviewPen() {
    QPen pen(QColor(23, 107, 135));
    pen.setWidth(2);
    pen.setStyle(Qt::DashLine);
    pen.setCosmetic(true);
    return pen;
}
