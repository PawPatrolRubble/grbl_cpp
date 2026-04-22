#pragma once

#include "Tool.h"

/// Angular dimension tool.
/// Workflow (3 clicks):
///   1st click = vertex (apex of angle)
///   2nd click = point on first ray
///   3rd click = point on second ray (places the dimension arc at radius = distance from vertex to cursor)
class AngularDimensionTool final : public Tool
{
public:
    AngularDimensionTool(Document& document, CommandStack& commandStack);

    QString name() const override;
    QString instructions() const override;

    void mousePress(const QPointF& pos) override;
    void mouseMove(const QPointF& pos) override;
    void mouseRelease(const QPointF& pos) override;
    void cancel() override;
    void paintPreview(QPainter& painter) const override;
    bool isActive() const override;

private:
    enum class Phase { Idle, HasVertex, HasP1 };
    Phase   m_phase = Phase::Idle;
    QPointF m_vertex;
    QPointF m_p1;
    QPointF m_currentPos;
};
