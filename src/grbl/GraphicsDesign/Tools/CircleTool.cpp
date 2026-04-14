#include "CircleTool.h"

#include "../CircleEntity.h"

#include <QLineF>
#include <memory>

CircleTool::CircleTool(Document& document, CommandStack& commandStack)
    : Tool(document, commandStack)
{
}

QString CircleTool::name() const
{
    return "Circle";
}

QString CircleTool::instructions() const
{
    if (m_center.has_value()) {
        return "Click to set the radius. Esc or right-click cancels.";
    }
    return "Click to set the center of the circle.";
}

void CircleTool::mousePress(const QPointF& pos)
{
    if (!m_center.has_value()) {
        m_center = pos;
        m_currentPoint = pos;
        return;
    }

    // Finish circle
    const double radius = QLineF(*m_center, pos).length();
    commandStack().execute(std::make_unique<AddEntityCommand>(
        std::make_unique<CircleEntity>(*m_center, radius)));
    m_center.reset();
    m_currentPoint = QPointF();
}

void CircleTool::mouseMove(const QPointF& pos)
{
    m_currentPoint = pos;
}

void CircleTool::mouseRelease(const QPointF& pos)
{
    // Circle tool uses press for both center and radius
}

void CircleTool::cancel()
{
    m_center.reset();
    m_currentPoint = QPointF();
}

void CircleTool::paintPreview(QPainter& painter) const
{
    if (!m_center.has_value()) {
        return;
    }

    const double radius = QLineF(*m_center, m_currentPoint).length();
    painter.drawEllipse(*m_center, radius, radius);
}

bool CircleTool::isActive() const
{
    return m_center.has_value();
}