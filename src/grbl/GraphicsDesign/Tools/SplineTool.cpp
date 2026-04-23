#include "SplineTool.h"

#include "../SplineEntity.h"
#include "../CommandStack.h"

#include <QPainter>
#include <QPainterPath>
#include <QLineF>
#include <cmath>
#include <algorithm>

namespace {
constexpr double kDuplicateTolerance = 0.001;  // world units
constexpr int    kPreviewSteps       = 15;      // segments per span for preview
}

// ---------------------------------------------------------------------------
// Catmull-Rom helpers (duplicated here to avoid depending on SplineEntity)
// ---------------------------------------------------------------------------

QPointF SplineTool::catmullRom(const QPointF& p0, const QPointF& p1,
                                const QPointF& p2, const QPointF& p3, double t)
{
    double t2 = t * t, t3 = t2 * t;
    return 0.5 * (2.0 * p1
        + (-p0 + p2) * t
        + (2.0 * p0 - 5.0 * p1 + 4.0 * p2 - p3) * t2
        + (-p0 + 3.0 * p1 - 3.0 * p2 + p3) * t3);
}

QVector<QPointF> SplineTool::evaluatePreview(const QList<QPointF>& pts) const
{
    QVector<QPointF> out;
    const int n = pts.size();
    if (n < 2) {
        if (n == 1) out.append(pts[0]);
        return out;
    }
    auto p = [&](int i) -> const QPointF& {
        return pts[std::clamp(i, 0, n - 1)];
    };
    for (int seg = 0; seg < n - 1; ++seg) {
        for (int step = 0; step <= kPreviewSteps; ++step) {
            double t = static_cast<double>(step) / kPreviewSteps;
            out.append(catmullRom(p(seg - 1), p(seg), p(seg + 1), p(seg + 2), t));
        }
    }
    return out;
}

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

SplineTool::SplineTool(Document& document, CommandStack& commandStack)
    : Tool(document, commandStack)
{}

// ---------------------------------------------------------------------------
// Tool interface
// ---------------------------------------------------------------------------

QString SplineTool::instructions() const
{
    if (m_points.isEmpty())
        return "Click to place first spline point.";
    return QString("Points: %1  |  Click to add, Backspace to remove last, Enter to finish, Esc to cancel.")
        .arg(m_points.size());
}

void SplineTool::mousePress(const QPointF& pos)
{
    if (!m_points.isEmpty() &&
        QLineF(m_points.last(), pos).length() < kDuplicateTolerance)
        return;

    m_points.append(pos);
    m_cursor = pos;
}

void SplineTool::mouseMove(const QPointF& pos)
{
    m_cursor = pos;
}

void SplineTool::mouseRelease(const QPointF& /*pos*/) {}

void SplineTool::cancel()
{
    m_points.clear();
    m_cursor = QPointF();
}

void SplineTool::paintPreview(QPainter& painter) const
{
    if (m_points.isEmpty()) return;

    // Build preview point list: confirmed points + live cursor
    QList<QPointF> preview = m_points;
    if (QLineF(preview.last(), m_cursor).length() > kDuplicateTolerance)
        preview.append(m_cursor);

    // Draw control-point markers
    QPen dotPen(QColor(100, 200, 255), 1.5);
    dotPen.setCosmetic(true);
    painter.save();
    painter.setPen(dotPen);
    painter.setBrush(QColor(100, 200, 255, 120));
    for (const QPointF& pt : m_points)
        painter.drawEllipse(pt, 2.5, 2.5);
    // Cursor marker (dimmer)
    painter.setBrush(QColor(100, 200, 255, 60));
    painter.drawEllipse(m_cursor, 2.5, 2.5);

    // Draw smooth spline preview
    if (preview.size() >= 2) {
        const QVector<QPointF> curve = evaluatePreview(preview);
        if (curve.size() >= 2) {
            QPen curvePen(QColor(100, 200, 255), 1.5);
            curvePen.setCosmetic(true);
            painter.setPen(curvePen);
            QPainterPath path;
            path.moveTo(curve[0]);
            for (int i = 1; i < curve.size(); ++i)
                path.lineTo(curve[i]);
            painter.setBrush(Qt::NoBrush);
            painter.drawPath(path);
        }
    }

    painter.restore();
}

bool SplineTool::finish()
{
    if (m_points.size() < 2)
        return false;

    commandStack().execute(
        std::make_unique<AddEntityCommand>(
            std::make_unique<SplineEntity>(m_points)));
    cancel();
    return true;
}

bool SplineTool::removeLastPoint()
{
    if (m_points.isEmpty()) return false;
    m_points.removeLast();
    m_cursor = m_points.isEmpty() ? QPointF() : m_points.last();
    return true;
}
