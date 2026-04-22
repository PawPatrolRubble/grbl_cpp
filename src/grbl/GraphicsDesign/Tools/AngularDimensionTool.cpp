#include "AngularDimensionTool.h"

#include "../AngularDimensionEntity.h"
#include "../CommandStack.h"

#include <QPainter>
#include <QLineF>
#include <QtMath>

AngularDimensionTool::AngularDimensionTool(Document& document, CommandStack& commandStack)
    : Tool(document, commandStack)
{
}

QString AngularDimensionTool::name() const
{
    return "Angular Dimension";
}

QString AngularDimensionTool::instructions() const
{
    switch (m_phase) {
    case Phase::Idle:      return "Click to set angle vertex.";
    case Phase::HasVertex: return "Click to set first ray point.";
    case Phase::HasP1:     return "Click to set second ray point. Esc cancels.";
    }
    return {};
}

void AngularDimensionTool::mousePress(const QPointF& pos)
{
    switch (m_phase) {
    case Phase::Idle:
        m_vertex = pos;
        m_phase = Phase::HasVertex;
        break;
    case Phase::HasVertex:
        if (QLineF(m_vertex, pos).length() > 0.1) {
            m_p1 = pos;
            m_phase = Phase::HasP1;
        }
        break;
    case Phase::HasP1: {
        if (QLineF(m_vertex, pos).length() < 0.1) break;
        // Radius = average distance from vertex to both ray points
        double r1 = QLineF(m_vertex, m_p1).length();
        double r2 = QLineF(m_vertex, pos).length();
        double radius = (r1 + r2) / 2.0;
        if (radius < 3.0) radius = 3.0;

        commandStack().execute(std::make_unique<AddEntityCommand>(
            std::make_unique<AngularDimensionEntity>(m_vertex, m_p1, pos, radius)));
        m_phase = Phase::Idle;
        break;
    }
    }
}

void AngularDimensionTool::mouseMove(const QPointF& pos)
{
    m_currentPos = pos;
}

void AngularDimensionTool::mouseRelease(const QPointF& pos)
{
    Q_UNUSED(pos)
}

void AngularDimensionTool::cancel()
{
    m_phase = Phase::Idle;
}

bool AngularDimensionTool::isActive() const
{
    return m_phase != Phase::Idle;
}

void AngularDimensionTool::paintPreview(QPainter& painter) const
{
    if (m_phase == Phase::Idle) return;

    QPen previewPen(Qt::cyan);
    previewPen.setWidthF(0.5);
    previewPen.setStyle(Qt::DashLine);
    painter.setPen(previewPen);

    if (m_phase == Phase::HasVertex) {
        // Draw rubberband line from vertex to current cursor
        painter.drawLine(m_vertex, m_currentPos);
        return;
    }

    // HasP1: draw first ray + rubberband second ray + live angular dimension preview
    painter.drawLine(m_vertex, m_p1);
    painter.drawLine(m_vertex, m_currentPos);

    if (QLineF(m_vertex, m_currentPos).length() > 0.1) {
        double r1 = QLineF(m_vertex, m_p1).length();
        double r2 = QLineF(m_vertex, m_currentPos).length();
        double radius = (r1 + r2) / 2.0;
        if (radius < 3.0) radius = 3.0;

        AngularDimensionEntity preview(m_vertex, m_p1, m_currentPos, radius);
        painter.setPen(previewPen);
        preview.draw(painter);
    }
}
