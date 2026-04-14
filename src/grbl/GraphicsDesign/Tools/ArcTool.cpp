#include "ArcTool.h"

#include "../ArcEntity.h"

#include <QLineF>
#include <cmath>
#include <memory>

namespace {
constexpr double kPi = 3.14159265358979323846;

double angleFromCenter(const QPointF& center, const QPointF& point) {
    return std::atan2(point.y() - center.y(), point.x() - center.x()) * 180.0 / kPi;
}
}

ArcTool::ArcTool(Document& document, CommandStack& commandStack)
    : Tool(document, commandStack)
{
}

QString ArcTool::name() const
{
    return "Arc";
}

QString ArcTool::instructions() const
{
    if (m_step == Step::None) {
        return "Click to set the center of the arc.";
    }
    if (m_step == Step::CenterSet) {
        return "Click to set the radius and start angle.";
    }
    return "Click to set the end angle. Esc or right-click cancels.";
}

void ArcTool::mousePress(const QPointF& pos)
{
    if (m_step == Step::None) {
        m_center = pos;
        m_currentPoint = pos;
        m_step = Step::CenterSet;
        return;
    }

    if (m_step == Step::CenterSet) {
        m_radius = QLineF(m_center, pos).length();
        m_startAngle = angleFromCenter(m_center, pos);
        m_currentPoint = pos;
        m_step = Step::RadiusSet;
        return;
    }

    // Finish arc
    const double endAngle = angleFromCenter(m_center, pos);
    double sweepAngle = endAngle - m_startAngle;

    commandStack().execute(std::make_unique<AddEntityCommand>(
        std::make_unique<ArcEntity>(m_center, m_radius, m_startAngle, sweepAngle)));

    m_step = Step::None;
    m_radius = 0.0;
    m_startAngle = 0.0;
    m_currentPoint = QPointF();
}

void ArcTool::mouseMove(const QPointF& pos)
{
    m_currentPoint = pos;
}

void ArcTool::mouseRelease(const QPointF& pos)
{
    // Arc tool uses press for all three points
}

void ArcTool::cancel()
{
    m_step = Step::None;
    m_radius = 0.0;
    m_startAngle = 0.0;
    m_currentPoint = QPointF();
}

void ArcTool::paintPreview(QPainter& painter) const
{
    if (m_step == Step::None) {
        return;
    }

    if (m_step == Step::CenterSet) {
        // Show radius line
        painter.drawLine(QLineF(m_center, m_currentPoint));
        const double radius = QLineF(m_center, m_currentPoint).length();
        painter.drawEllipse(m_center, radius, radius);
        return;
    }

    // Show arc preview
    const double endAngle = angleFromCenter(m_center, m_currentPoint);
    double sweepAngle = endAngle - m_startAngle;

    QRectF arcRect(m_center.x() - m_radius, m_center.y() - m_radius,
                   2.0 * m_radius, 2.0 * m_radius);
    painter.drawArc(arcRect,
                    static_cast<int>(m_startAngle * 16),
                    static_cast<int>(sweepAngle * 16));
}

bool ArcTool::isActive() const
{
    return m_step != Step::None;
}