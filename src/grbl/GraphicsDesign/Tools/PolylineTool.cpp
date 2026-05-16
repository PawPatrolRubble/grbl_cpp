#include "PolylineTool.h"

#include "../PolylineEntity.h"

#include <QLineF>
#include <QPolygonF>
#include <memory>

namespace {
constexpr double kDuplicateVertexTolerance = 0.001;
}

PolylineTool::PolylineTool(Document& document, CommandStack& commandStack)
    : Tool(document, commandStack)
{
}

QString PolylineTool::name() const
{
    return "Polyline";
}

QString PolylineTool::instructions() const
{
    if (m_points.isEmpty()) {
        return "Click to start a polyline.";
    }

    return "Click to add vertices. Double-click or Enter finishes. Backspace removes the last point. Esc cancels.";
}

void PolylineTool::mousePress(const QPointF& pos)
{
    if (m_points.isEmpty()) {
        m_points.append(pos);
        m_currentPoint = pos;
        return;
    }

    if (QLineF(m_points.last(), pos).length() > kDuplicateVertexTolerance) {
        m_points.append(pos);
    }
    m_currentPoint = pos;
}

void PolylineTool::mouseMove(const QPointF& pos)
{
    m_currentPoint = pos;
}

void PolylineTool::mouseRelease(const QPointF& pos)
{
    Q_UNUSED(pos);
}

void PolylineTool::cancel()
{
    m_points.clear();
    m_currentPoint = QPointF();
}

void PolylineTool::paintPreview(QPainter& painter) const
{
    if (m_points.isEmpty()) {
        return;
    }

    QPolygonF preview(m_points);
    if (QLineF(m_points.last(), m_currentPoint).length() > kDuplicateVertexTolerance) {
        preview.append(m_currentPoint);
    }

    if (preview.size() >= 2) {
        painter.drawPolyline(preview);
    }
}

bool PolylineTool::isActive() const
{
    return !m_points.isEmpty();
}

std::optional<QPointF> PolylineTool::inferenceReferencePoint() const
{
    if (m_points.isEmpty())
        return std::nullopt;
    return m_points.last();
}

bool PolylineTool::finish()
{
    if (m_points.size() < 2) {
        return false;
    }

    commandStack().execute(std::make_unique<AddEntityCommand>(
        std::make_unique<PolylineEntity>(m_points)));
    cancel();
    return true;
}

bool PolylineTool::removeLastVertex()
{
    if (m_points.isEmpty()) {
        return false;
    }

    m_points.removeLast();
    m_currentPoint = m_points.isEmpty() ? QPointF() : m_points.last();
    return true;
}