#include "LineTool.h"

#include "../LineEntity.h"

#include <QLineF>
#include <memory>

LineTool::LineTool(Document& document, CommandStack& commandStack)
    : Tool(document, commandStack)
{
}

QString LineTool::name() const
{
    return "Line";
}

QString LineTool::instructions() const
{
    if (m_startPoint.has_value()) {
        return "Click to finish the line. Esc or right-click cancels.";
    }
    return "Click to start a line.";
}

void LineTool::mousePress(const QPointF& pos)
{
    if (!m_startPoint.has_value()) {
        m_startPoint = pos;
        m_currentPoint = pos;
        return;
    }

    // Finish line
    commandStack().execute(std::make_unique<AddEntityCommand>(
        std::make_unique<LineEntity>(*m_startPoint, pos)));
    m_startPoint.reset();
    m_currentPoint = QPointF();
}

void LineTool::mouseMove(const QPointF& pos)
{
    m_currentPoint = pos;
}

void LineTool::mouseRelease(const QPointF& pos)
{
    // Line tool uses press for both start and end
}

void LineTool::cancel()
{
    m_startPoint.reset();
    m_currentPoint = QPointF();
}

void LineTool::paintPreview(QPainter& painter) const
{
    if (!m_startPoint.has_value()) {
        return;
    }
    painter.drawLine(QLineF(*m_startPoint, m_currentPoint));
}

bool LineTool::isActive() const
{
    return m_startPoint.has_value();
}

std::optional<QPointF> LineTool::inferenceReferencePoint() const
{
    return m_startPoint;
}