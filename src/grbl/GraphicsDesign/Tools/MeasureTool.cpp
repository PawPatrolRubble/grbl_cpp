#include "MeasureTool.h"

#include <QPainter>
#include <QLineF>
#include <cmath>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

void MeasureTool::updateFrom(const QPointF &p2)
{
    m_deltaX   = p2.x() - m_p1.x();
    m_deltaY   = p2.y() - m_p1.y();
    m_distance = std::hypot(m_deltaX, m_deltaY);
    m_angle    = std::atan2(m_deltaY, m_deltaX) * 180.0 / M_PI;
}

// ---------------------------------------------------------------------------
// Tool interface
// ---------------------------------------------------------------------------

QString MeasureTool::instructions() const
{
    if (!m_hasFirst)
        return "Click to set first measurement point";
    return QString("Click to set second point — dist: %1  Δx: %2  Δy: %3  angle: %4°")
        .arg(m_distance, 0, 'f', 3)
        .arg(m_deltaX,   0, 'f', 3)
        .arg(m_deltaY,   0, 'f', 3)
        .arg(m_angle,    0, 'f', 2);
}

void MeasureTool::mousePress(const QPointF &pos)
{
    if (!m_hasFirst) {
        // First click: set anchor
        m_p1       = pos;
        m_p2       = pos;
        m_hasFirst = true;
        updateFrom(pos);
    } else {
        // Second click: accept final point (keep showing result)
        m_p2 = pos;
        updateFrom(pos);
    }
}

void MeasureTool::mouseMove(const QPointF &pos)
{
    if (m_hasFirst) {
        m_p2 = pos;
        updateFrom(pos);
    }
}

void MeasureTool::mouseRelease(const QPointF & /*pos*/) {}

void MeasureTool::cancel()
{
    m_hasFirst = false;
    m_distance = m_deltaX = m_deltaY = m_angle = 0.0;
}

void MeasureTool::paintPreview(QPainter &painter) const
{
    if (!m_hasFirst) return;

    painter.save();

    // Dashed measurement line
    QPen linePen(QColor(255, 200, 50), 1.5);
    linePen.setCosmetic(true);
    linePen.setStyle(Qt::DashLine);
    painter.setPen(linePen);
    painter.drawLine(m_p1, m_p2);

    // Endpoint circles
    const double r = 3.0;
    QPen dotPen(QColor(255, 200, 50), 2.0);
    dotPen.setCosmetic(true);
    painter.setPen(dotPen);
    painter.setBrush(QColor(255, 200, 50, 180));
    painter.drawEllipse(m_p1, r, r);
    painter.drawEllipse(m_p2, r, r);

    painter.restore();
}
