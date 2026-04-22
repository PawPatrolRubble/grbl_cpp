#include "DimensionTool.h"

#include "../LinearDimensionEntity.h"
#include "../CommandStack.h"

#include <QPainter>
#include <QLineF>
#include <QtMath>

DimensionTool::DimensionTool(Document& document, CommandStack& commandStack)
    : Tool(document, commandStack)
{
}

QString DimensionTool::name() const
{
    return "Dimension";
}

QString DimensionTool::instructions() const
{
    switch (m_phase) {
    case Phase::Idle:  return "Click to set first measurement point.";
    case Phase::HasP1: return "Click to set second measurement point.";
    case Phase::HasP2: return "Click to place the dimension line. Esc cancels.";
    }
    return {};
}

void DimensionTool::mousePress(const QPointF& pos)
{
    switch (m_phase) {
    case Phase::Idle:
        m_p1 = pos;
        m_phase = Phase::HasP1;
        break;
    case Phase::HasP1:
        if (QLineF(m_p1, pos).length() > 0.1) {
            m_p2 = pos;
            m_phase = Phase::HasP2;
        }
        break;
    case Phase::HasP2: {
        // Compute offset = signed perpendicular distance from the midpoint of p1-p2
        QPointF d = m_p2 - m_p1;
        double len = QLineF(m_p1, m_p2).length();
        QPointF perp(-d.y() / len, d.x() / len);
        QPointF mid = (m_p1 + m_p2) / 2.0;
        double offset = QPointF::dotProduct(pos - mid, perp);
        if (std::abs(offset) < 3.0)
            offset = offset < 0 ? -3.0 : 3.0;

        commandStack().execute(std::make_unique<AddEntityCommand>(
            std::make_unique<LinearDimensionEntity>(m_p1, m_p2, offset)));
        m_phase = Phase::Idle;
        break;
    }
    }
}

void DimensionTool::mouseMove(const QPointF& pos)
{
    m_currentPos = pos;
}

void DimensionTool::mouseRelease(const QPointF& pos)
{
    Q_UNUSED(pos)
}

void DimensionTool::cancel()
{
    m_phase = Phase::Idle;
}

void DimensionTool::paintPreview(QPainter& painter) const
{
    if (m_phase == Phase::Idle) return;

    QPointF p2 = (m_phase == Phase::HasP1) ? m_currentPos : m_p2;

    // Draw the two measured points and the line between them
    painter.drawLine(m_p1, p2);

    // Draw small crosses at p1 and p2
    const double cx = 2.0;
    painter.drawLine(m_p1 + QPointF(-cx, 0), m_p1 + QPointF(cx, 0));
    painter.drawLine(m_p1 + QPointF(0, -cx), m_p1 + QPointF(0, cx));
    if (m_phase == Phase::HasP2) {
        painter.drawLine(m_p2 + QPointF(-cx, 0), m_p2 + QPointF(cx, 0));
        painter.drawLine(m_p2 + QPointF(0, -cx), m_p2 + QPointF(0, cx));

        // Preview dimension with current offset
        QPointF d = m_p2 - m_p1;
        double len = QLineF(m_p1, m_p2).length();
        if (len < 1e-9) return;
        QPointF perp(-d.y() / len, d.x() / len);
        QPointF mid = (m_p1 + m_p2) / 2.0;
        double offset = QPointF::dotProduct(m_currentPos - mid, perp);
        if (std::abs(offset) < 3.0)
            offset = offset < 0 ? -3.0 : 3.0;
        LinearDimensionEntity preview(m_p1, m_p2, offset);
        preview.draw(painter);
    }
}

bool DimensionTool::isActive() const
{
    return m_phase != Phase::Idle;
}
