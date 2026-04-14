#include "RectangleTool.h"

#include "../RectangleEntity.h"

#include <QRectF>
#include <memory>

RectangleTool::RectangleTool(Document& document, CommandStack& commandStack)
    : Tool(document, commandStack)
{
}

QString RectangleTool::name() const
{
    return "Rectangle";
}

QString RectangleTool::instructions() const
{
    if (m_firstCorner.has_value()) {
        return "Click opposite corner to finish. Esc or right-click cancels.";
    }
    return "Click first corner of rectangle.";
}

void RectangleTool::mousePress(const QPointF& pos)
{
    if (!m_firstCorner.has_value()) {
        m_firstCorner = pos;
        m_currentCorner = pos;
        return;
    }

    // Finish rectangle
    commandStack().execute(std::make_unique<AddEntityCommand>(
        std::make_unique<RectangleEntity>(*m_firstCorner, pos)));
    m_firstCorner.reset();
    m_currentCorner = QPointF();
}

void RectangleTool::mouseMove(const QPointF& pos)
{
    m_currentCorner = pos;
}

void RectangleTool::mouseRelease(const QPointF& pos)
{
    // Rectangle tool uses press for both corners
}

void RectangleTool::cancel()
{
    m_firstCorner.reset();
    m_currentCorner = QPointF();
}

void RectangleTool::paintPreview(QPainter& painter) const
{
    if (!m_firstCorner.has_value()) {
        return;
    }
    painter.drawRect(QRectF(*m_firstCorner, m_currentCorner).normalized());
}

bool RectangleTool::isActive() const
{
    return m_firstCorner.has_value();
}