#include "ToolpathCanvas.h"

#include <QPainter>
#include <QPen>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QResizeEvent>
#include <QtMath>
#include <algorithm>
#include <cmath>

static constexpr double HANDLE_SIZE = 7.0;  // half-size in screen pixels
static constexpr double ROTATION_HANDLE_DIST = 30.0;

ToolpathCanvas::ToolpathCanvas(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(200, 200);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setStyleSheet("background-color: #f5f5f7;");

    // Throttle repaints to max ~20 FPS during streaming
    m_repaintTimer.setSingleShot(true);
    m_repaintTimer.setInterval(50);
    connect(&m_repaintTimer, &QTimer::timeout, this, [this]() {
        if (m_repaintPending) {
            m_repaintPending = false;
            update();
        }
    });
}

// ===========================================================================
// Public API
// ===========================================================================

void ToolpathCanvas::setMoves(const QList<GcodeMove> &moves)
{
    m_moves = moves;
    m_bboxDirty = true;
    m_pathsDirty = true;
    m_executingLine = -1;
    m_highlightedLine = -1;
    m_pastBuiltLine = -1;

    // Build O(1) lineNumber → move index lookup
    m_lineToIndex.clear();
    m_lineToIndex.reserve(moves.size());
    for (int i = 0; i < moves.size(); ++i)
        m_lineToIndex.insert(moves[i].lineNumber, i);

    autoFit();
    update();
}

void ToolpathCanvas::setMovesNoFit(const QList<GcodeMove> &moves)
{
    m_moves = moves;
    m_bboxDirty = true;
    m_pathsDirty = true;
    m_pastBuiltLine = -1;

    // Build O(1) lineNumber → move index lookup
    m_lineToIndex.clear();
    m_lineToIndex.reserve(moves.size());
    for (int i = 0; i < moves.size(); ++i)
        m_lineToIndex.insert(moves[i].lineNumber, i);

    update();
}

void ToolpathCanvas::setExecutingLine(int lineNumber)
{
    m_executingLine = lineNumber;
    scheduleUpdate();
}

void ToolpathCanvas::setStreamRange(int startLine, int endLine)
{
    m_rangeStart = startLine;
    m_rangeEnd = endLine;
    m_pathsDirty = true;
    update();
}

void ToolpathCanvas::setCutterPosition(const QVector3D &pos)
{
    m_cutterPos = pos;
    scheduleUpdate();
}

void ToolpathCanvas::highlightSegment(int lineNumber)
{
    m_highlightedLine = lineNumber;
    update();
}

void ToolpathCanvas::scheduleUpdate()
{
    m_repaintPending = true;
    if (!m_repaintTimer.isActive()) {
        m_repaintTimer.start();
    }
}

void ToolpathCanvas::setWorktableSize(const QSizeF &size)
{
    m_worktableSize = size;
    autoFit();
    update();
}

// ===========================================================================
// Coordinate conversion
// ===========================================================================

void ToolpathCanvas::autoFit()
{
    if (m_moves.isEmpty()) {
        m_scale = 1.0;
        // Set origin at bottom-left corner with some margin
        m_offset = QPointF(50, height() - 50);
        m_pathsDirty = true;
        return;
    }

    double minX = 0, maxX = m_worktableSize.width();
    double minY = 0, maxY = m_worktableSize.height();
    for (const auto &mv : m_moves) {
        minX = std::min(minX, (double)mv.start.x());
        maxX = std::max(maxX, (double)mv.start.x());
        minX = std::min(minX, (double)mv.end.x());
        maxX = std::max(maxX, (double)mv.end.x());
        minY = std::min(minY, (double)mv.start.y());
        maxY = std::max(maxY, (double)mv.start.y());
        minY = std::min(minY, (double)mv.end.y());
        maxY = std::max(maxY, (double)mv.end.y());

        // Include arc center points for accurate bounds
        if (mv.type == GcodeMove::Type::ArcCW || mv.type == GcodeMove::Type::ArcCCW) {
            double cx = mv.start.x() + mv.iOffset;
            double cy = mv.start.y() + mv.jOffset;
            double r = std::hypot(mv.start.x() - cx, mv.start.y() - cy);
            minX = std::min(minX, cx - r);
            maxX = std::max(maxX, cx + r);
            minY = std::min(minY, cy - r);
            maxY = std::max(maxY, cy + r);
        }
    }

    double rangeX = maxX - minX;
    double rangeY = maxY - minY;
    if (rangeX < 0.001) rangeX = 1.0;
    if (rangeY < 0.001) rangeY = 1.0;

    double margin = 50.0;
    double scaleX = (width() - 2 * margin) / rangeX;
    double scaleY = (height() - 2 * margin) / rangeY;
    m_scale = std::min(scaleX, scaleY);

    double cx = (minX + maxX) / 2.0;
    double cy = (minY + maxY) / 2.0;
    m_offset = QPointF(width() / 2.0 - cx * m_scale,
                       height() / 2.0 + cy * m_scale);
    m_pathsDirty = true;
}

QPointF ToolpathCanvas::machineToScreen(const QPointF &pt) const
{
    return QPointF(pt.x() * m_scale + m_offset.x(),
                   -pt.y() * m_scale + m_offset.y());
}

QPointF ToolpathCanvas::screenToMachine(const QPointF &pt) const
{
    return QPointF((pt.x() - m_offset.x()) / m_scale,
                   -(pt.y() - m_offset.y()) / m_scale);
}

// ===========================================================================
// Arc approximation
// ===========================================================================

QVector<QPointF> ToolpathCanvas::approximateArc(const GcodeMove &move) const
{
    QVector<QPointF> pts;
    QPointF start(move.start.x(), move.start.y());
    QPointF end(move.end.x(), move.end.y());

    double cx = move.start.x() + move.iOffset;
    double cy = move.start.y() + move.jOffset;
    double r = std::hypot(move.start.x() - cx, move.start.y() - cy);
    if (r < 0.0001) {
        pts << start << end;
        return pts;
    }

    double startAngle = std::atan2(move.start.y() - cy, move.start.x() - cx);
    double endAngle   = std::atan2(move.end.y() - cy, move.end.x() - cx);

    bool clockwise = (move.type == GcodeMove::Type::ArcCW);
    double sweep;
    if (clockwise) {
        sweep = startAngle - endAngle;
        if (sweep <= 0) sweep += 2.0 * M_PI;
    } else {
        sweep = endAngle - startAngle;
        if (sweep <= 0) sweep += 2.0 * M_PI;
    }

    int segments = std::max(8, static_cast<int>(std::abs(sweep) / (M_PI / 8.0)));
    pts.reserve(segments + 1);

    for (int i = 0; i <= segments; ++i) {
        double t = static_cast<double>(i) / segments;
        double angle = clockwise ? (startAngle - sweep * t) : (startAngle + sweep * t);
        pts << QPointF(cx + r * std::cos(angle), cy + r * std::sin(angle));
    }
    return pts;
}

// ===========================================================================
// Bounding box computation & handle positions
// ===========================================================================

QRectF ToolpathCanvas::computeBoundingBox() const
{
    if (!m_bboxDirty)
        return m_cachedBBox;

    if (m_moves.isEmpty()) {
        m_cachedBBox = QRectF();
        m_bboxDirty = false;
        return m_cachedBBox;
    }

    double minX = 1e9, maxX = -1e9, minY = 1e9, maxY = -1e9;
    for (const auto &mv : m_moves) {
        minX = std::min(minX, (double)mv.start.x());
        maxX = std::max(maxX, (double)mv.start.x());
        minX = std::min(minX, (double)mv.end.x());
        maxX = std::max(maxX, (double)mv.end.x());
        minY = std::min(minY, (double)mv.start.y());
        maxY = std::max(maxY, (double)mv.start.y());
        minY = std::min(minY, (double)mv.end.y());
        maxY = std::max(maxY, (double)mv.end.y());
    }
    m_cachedBBox = QRectF(QPointF(minX, minY), QPointF(maxX, maxY));
    m_bboxDirty = false;
    return m_cachedBBox;
}

QPointF ToolpathCanvas::handleScreenPos(HandleId id, const QRectF &bbox) const
{
    double l = bbox.left(), r = bbox.right(), t = bbox.top(), b = bbox.bottom();
    double cx = (l + r) / 2.0, cy = (t + b) / 2.0;

    QPointF machPt;
    switch (id) {
    case HandleId::TopLeft:      machPt = QPointF(l, b); break; // machine Y-up: top = maxY
    case HandleId::TopCenter:    machPt = QPointF(cx, b); break;
    case HandleId::TopRight:     machPt = QPointF(r, b); break;
    case HandleId::MiddleLeft:   machPt = QPointF(l, cy); break;
    case HandleId::MiddleRight:  machPt = QPointF(r, cy); break;
    case HandleId::BottomLeft:   machPt = QPointF(l, t); break;
    case HandleId::BottomCenter: machPt = QPointF(cx, t); break;
    case HandleId::BottomRight:  machPt = QPointF(r, t); break;
    case HandleId::Rotation: {
        QPointF topCenter = machineToScreen(QPointF(cx, b));
        return QPointF(topCenter.x(), topCenter.y() - ROTATION_HANDLE_DIST);
    }
    default: return QPointF();
    }
    return machineToScreen(machPt);
}

ToolpathCanvas::HandleId ToolpathCanvas::hitTestHandles(const QPointF &screenPos) const
{
    if (m_moves.isEmpty()) return HandleId::None;

    QRectF bbox = computeBoundingBox();
    static const HandleId handles[] = {
        HandleId::Rotation,
        HandleId::TopLeft, HandleId::TopCenter, HandleId::TopRight,
        HandleId::MiddleLeft, HandleId::MiddleRight,
        HandleId::BottomLeft, HandleId::BottomCenter, HandleId::BottomRight
    };

    for (auto h : handles) {
        QPointF hp = handleScreenPos(h, bbox);
        if (std::hypot(screenPos.x() - hp.x(), screenPos.y() - hp.y()) <= HANDLE_SIZE + 3)
            return h;
    }
    return HandleId::None;
}

bool ToolpathCanvas::isInsideBBox(const QPointF &screenPos) const
{
    if (m_moves.isEmpty()) return false;
    QRectF bbox = computeBoundingBox();
    QPointF tl = machineToScreen(QPointF(bbox.left(), bbox.bottom()));
    QPointF br = machineToScreen(QPointF(bbox.right(), bbox.top()));
    QRectF screenRect(tl, br);
    return screenRect.normalized().contains(screenPos);
}

Qt::CursorShape ToolpathCanvas::cursorForHandle(HandleId id) const
{
    switch (id) {
    case HandleId::TopLeft:
    case HandleId::BottomRight:  return Qt::SizeFDiagCursor;
    case HandleId::TopRight:
    case HandleId::BottomLeft:   return Qt::SizeBDiagCursor;
    case HandleId::TopCenter:
    case HandleId::BottomCenter: return Qt::SizeVerCursor;
    case HandleId::MiddleLeft:
    case HandleId::MiddleRight:  return Qt::SizeHorCursor;
    case HandleId::Rotation:     return Qt::CrossCursor;
    default: return Qt::ArrowCursor;
    }
}

// ===========================================================================
// Painting
// ===========================================================================

void ToolpathCanvas::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), QColor("#f5f5f7"));

    drawGridAndRulers(painter);
    drawWorktable(painter);
    drawAxisOrigin(painter);
    drawMoves(painter);
    drawBoundingBox(painter);
    drawCutter(painter);
    drawCrosshair(painter);
}

void ToolpathCanvas::rebuildPaths()
{
    m_rapidLines.clear();
    m_linearLines.clear();
    m_arcLines.clear();
    m_dimmedLines.clear();

    // Pre-allocate reasonable sizes
    m_rapidLines.reserve(m_moves.size() / 3);
    m_linearLines.reserve(m_moves.size() / 3);

    bool hasRange = (m_rangeStart >= 0 || m_rangeEnd >= 0);
    int rStart = m_rangeStart >= 0 ? m_rangeStart : 0;
    int rEnd = m_rangeEnd >= 0 ? m_rangeEnd : INT_MAX;

    for (int i = 0; i < m_moves.size(); ++i) {
        const auto &mv = m_moves[i];
        bool inRange = !hasRange || (mv.lineNumber >= rStart && mv.lineNumber <= rEnd);

        if (mv.type == GcodeMove::Type::ArcCW || mv.type == GcodeMove::Type::ArcCCW) {
            auto pts = approximateArc(mv);
            for (int j = 0; j + 1 < pts.size(); ++j) {
                QLineF seg(machineToScreen(pts[j]), machineToScreen(pts[j + 1]));
                if (inRange)
                    m_arcLines << seg;
                else
                    m_dimmedLines << seg;
            }
        } else if (mv.type == GcodeMove::Type::Rapid) {
            QPointF a = machineToScreen(QPointF(mv.start.x(), mv.start.y()));
            QPointF b = machineToScreen(QPointF(mv.end.x(), mv.end.y()));
            QLineF seg(a, b);
            if (inRange)
                m_rapidLines << seg;
            else
                m_dimmedLines << seg;
        } else {
            QPointF a = machineToScreen(QPointF(mv.start.x(), mv.start.y()));
            QPointF b = machineToScreen(QPointF(mv.end.x(), mv.end.y()));
            QLineF seg(a, b);
            if (inRange)
                m_linearLines << seg;
            else
                m_dimmedLines << seg;
        }
    }

    m_pathScale = m_scale;
    m_pathOffset = m_offset;
    m_pathsDirty = false;
}

void ToolpathCanvas::drawMoves(QPainter &painter)
{
    if (m_moves.isEmpty())
        return;

    bool scaleChanged = (m_scale != m_pathScale || m_offset != m_pathOffset);
    if (m_pathsDirty || scaleChanged)
        rebuildPaths();

    painter.setPen(QPen(QColor("#ff8800"), 1.8, Qt::DashLine));
    painter.drawLines(m_rapidLines);

    painter.setPen(QPen(QColor("#2266aa"), 1.8));
    painter.drawLines(m_linearLines);

    painter.setPen(QPen(QColor("#22aa66"), 1.8));
    painter.drawLines(m_arcLines);

    // Out-of-range segments drawn dimmed
    if (!m_dimmedLines.isEmpty()) {
        painter.setPen(QPen(QColor("#444444"), 1.0));
        painter.drawLines(m_dimmedLines);
    }

    // Overlay highlights — O(1) via hash lookup instead of O(N) scan
    auto drawMoveHighlight = [&](int moveIdx, const QColor &color) {
        if (moveIdx < 0 || moveIdx >= m_moves.size()) return;
        const auto &mv = m_moves[moveIdx];

        painter.setPen(QPen(color, 2.2));

        if (mv.type == GcodeMove::Type::ArcCW || mv.type == GcodeMove::Type::ArcCCW) {
            auto pts = approximateArc(mv);
            for (int j = 0; j + 1 < pts.size(); ++j)
                painter.drawLine(machineToScreen(pts[j]), machineToScreen(pts[j + 1]));
        } else {
            painter.drawLine(machineToScreen(QPointF(mv.start.x(), mv.start.y())),
                             machineToScreen(QPointF(mv.end.x(), mv.end.y())));
        }
    };

    // Highlight specific line (user clicked)
    if (m_highlightedLine >= 0) {
        auto it = m_lineToIndex.find(m_highlightedLine);
        if (it != m_lineToIndex.end())
            drawMoveHighlight(it.value(), QColor("#ffdd44"));
    }

    // Executing line (currently streaming)
    if (m_executingLine >= 0) {
        auto it = m_lineToIndex.find(m_executingLine);
        if (it != m_lineToIndex.end())
            drawMoveHighlight(it.value(), QColor("#ffdd44"));

        // Past moves — dimmed.  Rebuild the batch cache only when the
        // executing line changes (or when paths/view changed).
        if (m_pastBuiltLine != m_executingLine || scaleChanged) {
            m_pastSegments.clear();
            for (auto mapIt = m_lineToIndex.begin(); mapIt != m_lineToIndex.end(); ++mapIt) {
                if (mapIt.key() >= m_executingLine || mapIt.key() == m_highlightedLine)
                    continue;
                int idx = mapIt.value();
                if (idx < 0 || idx >= m_moves.size()) continue;
                const auto &mv = m_moves[idx];
                if (mv.type == GcodeMove::Type::ArcCW || mv.type == GcodeMove::Type::ArcCCW) {
                    auto pts = approximateArc(mv);
                    for (int j = 0; j + 1 < pts.size(); ++j)
                        m_pastSegments << QLineF(machineToScreen(pts[j]), machineToScreen(pts[j + 1]));
                } else {
                    m_pastSegments << QLineF(
                        machineToScreen(QPointF(mv.start.x(), mv.start.y())),
                        machineToScreen(QPointF(mv.end.x(), mv.end.y())));
                }
            }
            m_pastBuiltLine = m_executingLine;
        }
        if (!m_pastSegments.isEmpty()) {
            painter.setPen(QPen(QColor("#666666"), 2.2));
            painter.drawLines(m_pastSegments);
        }
    }
}

void ToolpathCanvas::drawCutter(QPainter &painter)
{
    QPointF cPos = machineToScreen(QPointF(m_cutterPos.x(), m_cutterPos.y()));

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor("#cc3333"));
    painter.drawEllipse(cPos, 6, 6);

    if (m_executingLine >= 0) {
        // Use the lineToIndex hash for O(1) lookup instead of O(N) scan.
        auto it = m_lineToIndex.find(m_executingLine);
        if (it != m_lineToIndex.end()) {
            int idx = it.value();
            if (idx >= 0 && idx < m_moves.size()) {
                const auto &mv = m_moves[idx];
                QPointF target = machineToScreen(QPointF(mv.end.x(), mv.end.y()));
                QPointF dir = target - cPos;
                double len = std::hypot(dir.x(), dir.y());
                if (len > 1.0) {
                    dir /= len;
                    QPointF tip = cPos + dir * 14.0;
                    painter.setPen(QPen(QColor("#cc3333"), 2));
                    painter.drawLine(cPos, tip);
                    QPointF perp(-dir.y(), dir.x());
                    painter.drawLine(tip, tip - dir * 5 + perp * 3);
                    painter.drawLine(tip, tip - dir * 5 - perp * 3);
                }
            }
        }
    }
}

void ToolpathCanvas::drawBoundingBox(QPainter &painter)
{
    if (m_moves.isEmpty()) return;

    QRectF bbox = computeBoundingBox();

    // BBox rectangle (machine coords: left/bottom = min, right/top = max)
    QPointF sTL = machineToScreen(QPointF(bbox.left(), bbox.bottom()));
    QPointF sBR = machineToScreen(QPointF(bbox.right(), bbox.top()));
    QRectF screenRect = QRectF(sTL, sBR).normalized();

    painter.setPen(QPen(QColor("#89b4fa"), 1.2, Qt::DashLine));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(screenRect);

    // Draw 8 resize handles
    static const HandleId resizeHandles[] = {
        HandleId::TopLeft, HandleId::TopCenter, HandleId::TopRight,
        HandleId::MiddleLeft, HandleId::MiddleRight,
        HandleId::BottomLeft, HandleId::BottomCenter, HandleId::BottomRight
    };

    painter.setPen(QPen(QColor("#89b4fa"), 1));
    painter.setBrush(QColor("#cdd6f4"));
    for (auto h : resizeHandles) {
        QPointF hp = handleScreenPos(h, bbox);
        painter.drawRect(QRectF(hp.x() - HANDLE_SIZE, hp.y() - HANDLE_SIZE,
                                HANDLE_SIZE * 2, HANDLE_SIZE * 2));
    }

    // Rotation handle
    QPointF topCenter = handleScreenPos(HandleId::TopCenter, bbox);
    QPointF rotHandle = handleScreenPos(HandleId::Rotation, bbox);

    painter.setPen(QPen(QColor("#89b4fa"), 1.2));
    painter.drawLine(topCenter, rotHandle);

    painter.setPen(QPen(QColor("#f9e2af"), 1.5));
    painter.setBrush(QColor("#f9e2af"));
    painter.drawEllipse(rotHandle, HANDLE_SIZE, HANDLE_SIZE);
}

void ToolpathCanvas::drawGridAndRulers(QPainter &painter)
{
    double stepMachine = 10.0;
    while (stepMachine * m_scale < 30.0) {
        stepMachine *= 2.0;
        if (stepMachine * m_scale >= 30.0) break;
        stepMachine *= 2.5;
    }

    QPointF machineTL = screenToMachine(QPointF(0, 0));
    QPointF machineBR = screenToMachine(QPointF(width(), height()));

    double startX = std::floor(machineTL.x() / stepMachine) * stepMachine;
    double endX   = std::ceil(machineBR.x() / stepMachine) * stepMachine;
    double startY = std::floor(machineBR.y() / stepMachine) * stepMachine;
    double endY   = std::ceil(machineTL.y() / stepMachine) * stepMachine;

    painter.setPen(QPen(QColor("#d1d5db"), 1));
    for (double x = startX; x <= endX; x += stepMachine) {
        QPointF p1 = machineToScreen(QPointF(x, startY));
        QPointF p2 = machineToScreen(QPointF(x, endY));
        painter.drawLine(p1, p2);

        painter.setPen(QColor("#6b7280"));
        painter.drawText(QPointF(p1.x() + 4, height() - 4), QString::number(x));
        painter.setPen(QPen(QColor("#d1d5db"), 1));
    }

    for (double y = startY; y <= endY; y += stepMachine) {
        QPointF p1 = machineToScreen(QPointF(startX, y));
        QPointF p2 = machineToScreen(QPointF(endX, y));
        painter.drawLine(p1, p2);

        painter.setPen(QColor("#6b7280"));
        painter.drawText(QPointF(4, p1.y() - 4), QString::number(y));
        painter.setPen(QPen(QColor("#d1d5db"), 1));
    }
}

void ToolpathCanvas::drawWorktable(QPainter &painter)
{
    QPointF bl = machineToScreen(QPointF(0, 0));
    QPointF br = machineToScreen(QPointF(m_worktableSize.width(), 0));
    QPointF tl = machineToScreen(QPointF(0, m_worktableSize.height()));
    QPointF tr = machineToScreen(QPointF(m_worktableSize.width(), m_worktableSize.height()));

    painter.setPen(QPen(QColor("#9ca3af"), 1.5, Qt::DotLine));
    painter.drawLine(bl, br);
    painter.drawLine(br, tr);
    painter.drawLine(tr, tl);
    painter.drawLine(tl, bl);
}

void ToolpathCanvas::drawAxisOrigin(QPainter &painter)
{
    QPointF origin = machineToScreen(QPointF(0, 0));

    painter.setPen(QPen(QColor("#f38ba8"), 2));
    QPointF xEnd = origin + QPointF(40, 0);
    painter.drawLine(origin, xEnd);
    painter.drawLine(xEnd, xEnd + QPointF(-5, -5));
    painter.drawLine(xEnd, xEnd + QPointF(-5, 5));
    painter.drawText(xEnd + QPointF(5, 5), "X");

    painter.setPen(QPen(QColor("#a6e3a1"), 2));
    QPointF yEnd = origin + QPointF(0, -40);
    painter.drawLine(origin, yEnd);
    painter.drawLine(yEnd, yEnd + QPointF(-5, 5));
    painter.drawLine(yEnd, yEnd + QPointF(5, 5));
    painter.drawText(yEnd + QPointF(-5, -5), "Y");
}

// ===========================================================================
// Click / segment detection
// ===========================================================================

double ToolpathCanvas::distanceToSegment(const QPointF &p, const QPointF &a, const QPointF &b) const
{
    QPointF ab = b - a;
    double len2 = ab.x() * ab.x() + ab.y() * ab.y();
    if (len2 < 1e-12)
        return std::hypot(p.x() - a.x(), p.y() - a.y());
    double t = std::clamp(((p.x() - a.x()) * ab.x() + (p.y() - a.y()) * ab.y()) / len2, 0.0, 1.0);
    QPointF closest(a.x() + t * ab.x(), a.y() + t * ab.y());
    return std::hypot(p.x() - closest.x(), p.y() - closest.y());
}

int ToolpathCanvas::findClickedSegment(const QPointF &screenPos) const
{
    constexpr double threshold = 10.0;
    double bestDist = threshold;
    int bestLine = -1;

    for (const auto &mv : m_moves) {
        if (mv.type == GcodeMove::Type::ArcCW || mv.type == GcodeMove::Type::ArcCCW) {
            auto pts = approximateArc(mv);
            for (int j = 0; j + 1 < pts.size(); ++j) {
                double d = distanceToSegment(screenPos, machineToScreen(pts[j]), machineToScreen(pts[j + 1]));
                if (d < bestDist) { bestDist = d; bestLine = mv.lineNumber; }
            }
        } else {
            QPointF s = machineToScreen(QPointF(mv.start.x(), mv.start.y()));
            QPointF e = machineToScreen(QPointF(mv.end.x(), mv.end.y()));
            double d = distanceToSegment(screenPos, s, e);
            if (d < bestDist) { bestDist = d; bestLine = mv.lineNumber; }
        }
    }
    return bestLine;
}

// ===========================================================================
// Mouse interaction
// ===========================================================================

void ToolpathCanvas::mousePressEvent(QMouseEvent *event)
{
    QPointF pos = event->position();

    if (event->button() == Qt::LeftButton) {
        // 1. Check handle hit
        HandleId handle = hitTestHandles(pos);
        if (handle != HandleId::None) {
            m_activeHandle = handle;
            m_preDragMoves = m_moves;
            m_dragStart = pos;
            m_dragStartMachine = screenToMachine(pos);
            m_dragStartBBox = computeBoundingBox();

            if (handle == HandleId::Rotation) {
                m_dragMode = DragMode::Rotate;
                QPointF bboxCenter((m_dragStartBBox.left() + m_dragStartBBox.right()) / 2.0,
                                   (m_dragStartBBox.top() + m_dragStartBBox.bottom()) / 2.0);
                QPointF centerScreen = machineToScreen(bboxCenter);
                m_dragStartAngle = std::atan2(pos.y() - centerScreen.y(),
                                              pos.x() - centerScreen.x());
            } else {
                m_dragMode = DragMode::Scale;
            }
            setCursor(cursorForHandle(handle));
            return;
        }

        // 2. Segment click (check before bbox so clicks near toolpath select the line)
        int line = findClickedSegment(pos);
        if (line >= 0) {
            m_highlightedLine = line;
            emit segmentClicked(line);
            update();
            return;
        }

        // 3. Check inside bbox → translate
        if (isInsideBBox(pos)) {
            m_dragMode = DragMode::Translate;
            m_preDragMoves = m_moves;
            m_dragStart = pos;
            m_dragStartMachine = screenToMachine(pos);
            m_dragStartBBox = computeBoundingBox();
            setCursor(Qt::SizeAllCursor);
            return;
        }
    }

    // Middle/Right → pan
    if (event->button() == Qt::MiddleButton || event->button() == Qt::RightButton) {
        m_dragMode = DragMode::Pan;
        m_dragStart = pos;
        setCursor(Qt::ClosedHandCursor);
    }
}

void ToolpathCanvas::mouseMoveEvent(QMouseEvent *event)
{
    QPointF pos = event->position();
    m_mouseScreenPos = pos;
    m_mouseInWidget = true;

    switch (m_dragMode) {
    case DragMode::Pan: {
        QPointF delta = pos - m_dragStart;
        m_offset += delta;
        m_dragStart = pos;
        m_pathsDirty = true;
        update();
        return;
    }
    case DragMode::Translate: {
        QPointF nowMachine = screenToMachine(pos);
        double dx = nowMachine.x() - m_dragStartMachine.x();
        double dy = nowMachine.y() - m_dragStartMachine.y();

        m_moves = GcodeParser::transformMoves(m_preDragMoves, 1.0, 1.0, dx, dy, 0.0);
        m_bboxDirty = true;
        update();
        return;
    }
    case DragMode::Scale: {
        QRectF bb = m_dragStartBBox;
        QPointF centerMach((bb.left() + bb.right()) / 2.0,
                           (bb.top() + bb.bottom()) / 2.0);
        double bbW = bb.width();
        double bbH = bb.height();
        if (bbW < 0.001) bbW = 1.0;
        if (bbH < 0.001) bbH = 1.0;

        QPointF nowMach = screenToMachine(pos);
        double scaleX = 1.0, scaleY = 1.0;

        // Compute scale based on which handle is being dragged
        switch (m_activeHandle) {
        case HandleId::TopRight:
        case HandleId::BottomRight:
        case HandleId::TopLeft:
        case HandleId::BottomLeft: {
            // Corner → proportional based on distance from center
            double origDist = std::hypot(m_dragStartMachine.x() - centerMach.x(),
                                         m_dragStartMachine.y() - centerMach.y());
            double newDist = std::hypot(nowMach.x() - centerMach.x(),
                                        nowMach.y() - centerMach.y());
            if (origDist > 0.001) {
                double s = newDist / origDist;
                scaleX = s;
                scaleY = s;
            }
            break;
        }
        case HandleId::MiddleRight:
        case HandleId::MiddleLeft: {
            double origDx = std::abs(m_dragStartMachine.x() - centerMach.x());
            double newDx = std::abs(nowMach.x() - centerMach.x());
            if (origDx > 0.001) scaleX = newDx / origDx;
            break;
        }
        case HandleId::TopCenter:
        case HandleId::BottomCenter: {
            double origDy = std::abs(m_dragStartMachine.y() - centerMach.y());
            double newDy = std::abs(nowMach.y() - centerMach.y());
            if (origDy > 0.001) scaleY = newDy / origDy;
            break;
        }
        default: break;
        }

        scaleX = std::max(0.01, scaleX);
        scaleY = std::max(0.01, scaleY);

        m_moves = GcodeParser::transformMoves(m_preDragMoves, scaleX, scaleY, 0, 0, 0);
        m_bboxDirty = true;
        update();
        return;
    }
    case DragMode::Rotate: {
        QRectF bb = m_dragStartBBox;
        QPointF centerMach((bb.left() + bb.right()) / 2.0,
                           (bb.top() + bb.bottom()) / 2.0);
        QPointF centerScreen = machineToScreen(centerMach);

        double currentAngle = std::atan2(pos.y() - centerScreen.y(),
                                         pos.x() - centerScreen.x());
        double deltaAngle = currentAngle - m_dragStartAngle;
        double deltaDeg = -deltaAngle * 180.0 / M_PI;  // negate because screen Y is flipped

        m_moves = GcodeParser::transformMoves(m_preDragMoves, 1.0, 1.0, 0, 0, deltaDeg);
        m_bboxDirty = true;
        update();
        return;
    }
    case DragMode::None:
        break;
    }

    // Hover cursor feedback
    HandleId hh = hitTestHandles(pos);
    if (hh != HandleId::None) {
        setCursor(cursorForHandle(hh));
    } else if (isInsideBBox(pos)) {
        setCursor(Qt::SizeAllCursor);
    } else {
        setCursor(Qt::ArrowCursor);
    }
    update(); // Repaint crosshair
}

void ToolpathCanvas::mouseReleaseEvent(QMouseEvent *event)
{
    Q_UNUSED(event);

    if (m_dragMode == DragMode::Translate ||
        m_dragMode == DragMode::Scale ||
        m_dragMode == DragMode::Rotate) {
        // Commit: emit the transformed moves
        emit movesTransformed(m_moves);
    }

    m_dragMode = DragMode::None;
    m_activeHandle = HandleId::None;
    m_preDragMoves.clear();
    setCursor(Qt::ArrowCursor);
}

void ToolpathCanvas::wheelEvent(QWheelEvent *event)
{
    double factor = event->angleDelta().y() > 0 ? 1.15 : (1.0 / 1.15);
    QPointF pivot = event->position();

    m_offset = pivot - factor * (pivot - m_offset);
    m_scale *= factor;
    m_pathsDirty = true;
    update();
}

void ToolpathCanvas::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    // Keep origin at bottom-left corner when widget is resized
    if (m_moves.isEmpty()) {
        m_offset = QPointF(50, height() - 50);
        m_pathsDirty = true;
        update();
    }
}

void ToolpathCanvas::leaveEvent(QEvent *event)
{
    QWidget::leaveEvent(event);
    m_mouseInWidget = false;
    update();
}

void ToolpathCanvas::drawCrosshair(QPainter &painter)
{
    if (!m_mouseInWidget) return;

    painter.save();

    // Dashed crosshair lines
    QPen crossPen(QColor(100, 100, 100, 150), 1.0, Qt::DashLine);
    painter.setPen(crossPen);
    painter.drawLine(QPointF(m_mouseScreenPos.x(), 0),
                     QPointF(m_mouseScreenPos.x(), height()));
    painter.drawLine(QPointF(0, m_mouseScreenPos.y()),
                     QPointF(width(), m_mouseScreenPos.y()));

    // Convert to machine coordinates
    QPointF machPos = screenToMachine(m_mouseScreenPos);

    // Draw coordinate label near cursor
    QString coordText = QString("X:%1  Y:%2")
                            .arg(machPos.x(), 0, 'f', 2)
                            .arg(machPos.y(), 0, 'f', 2);

    QFont font("Consolas", 9);
    painter.setFont(font);
    QFontMetrics fm(font);
    QRect textRect = fm.boundingRect(coordText);
    textRect.adjust(-4, -2, 4, 2);

    // Position label offset from cursor to avoid overlap
    double labelX = m_mouseScreenPos.x() + 12;
    double labelY = m_mouseScreenPos.y() - 12;
    // Keep label within widget bounds
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
