#include "RadiusDimensionTool.h"

#include "../RadiusDimensionEntity.h"
#include "../CommandStack.h"

#include <QPainter>
#include <QLineF>
#include <QtMath>

RadiusDimensionTool::RadiusDimensionTool(Document& document, CommandStack& commandStack)
    : Tool(document, commandStack)
{
}

QString RadiusDimensionTool::name() const
{
    return m_diameter ? "Diameter Dimension" : "Radius Dimension";
}

QString RadiusDimensionTool::instructions() const
{
    switch (m_phase) {
    case Phase::Idle:      return "Click to set circle center.";
    case Phase::HasCenter: return "Click to set point on circumference. Esc cancels.";
    }
    return {};
}

void RadiusDimensionTool::mousePress(const QPointF& pos)
{
    switch (m_phase) {
    case Phase::Idle:
        m_center = pos;
        m_phase  = Phase::HasCenter;
        break;
    case Phase::HasCenter: {
        double r = QLineF(m_center, pos).length();
        if (r < 0.1) break;
        double angleDeg = qRadiansToDegrees(qAtan2(pos.y() - m_center.y(),
                                                    pos.x() - m_center.x()));
        commandStack().execute(std::make_unique<AddEntityCommand>(
            std::make_unique<RadiusDimensionEntity>(m_center, r, angleDeg, m_diameter)));
        m_phase = Phase::Idle;
        break;
    }
    }
}

void RadiusDimensionTool::mouseMove(const QPointF& pos)
{
    m_currentPos = pos;
}

void RadiusDimensionTool::mouseRelease(const QPointF& pos)
{
    Q_UNUSED(pos)
}

void RadiusDimensionTool::cancel()
{
    m_phase = Phase::Idle;
}

bool RadiusDimensionTool::isActive() const
{
    return m_phase != Phase::Idle;
}

void RadiusDimensionTool::paintPreview(QPainter& painter) const
{
    if (m_phase == Phase::Idle) return;

    QPen previewPen(Qt::cyan);
    previewPen.setWidthF(0.5);
    previewPen.setStyle(Qt::DashLine);
    painter.setPen(previewPen);

    double r = QLineF(m_center, m_currentPos).length();
    if (r < 0.1) {
        painter.drawPoint(m_center);
        return;
    }

    // Draw the live circle and leader
    painter.drawEllipse(m_center, r, r);

    double angleDeg = qRadiansToDegrees(qAtan2(m_currentPos.y() - m_center.y(),
                                                m_currentPos.x() - m_center.x()));
    RadiusDimensionEntity preview(m_center, r, angleDeg, m_diameter);
    painter.setPen(previewPen);
    preview.draw(painter);
}
