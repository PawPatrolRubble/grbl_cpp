#pragma once

#include "Tool.h"

#include <optional>

/// Dimension tool.
/// First click = first point, second click = second point,
/// third click = offset distance (drag dimension line away from the measured edge).
class DimensionTool final : public Tool
{
public:
    DimensionTool(Document& document, CommandStack& commandStack);

    QString name() const override;
    QString instructions() const override;

    void mousePress(const QPointF& pos) override;
    void mouseMove(const QPointF& pos) override;
    void mouseRelease(const QPointF& pos) override;
    void cancel() override;
    void paintPreview(QPainter& painter) const override;
    bool isActive() const override;

private:
    enum class Phase { Idle, HasP1, HasP2 };
    Phase       m_phase = Phase::Idle;
    QPointF     m_p1;
    QPointF     m_p2;
    QPointF     m_currentPos;
};
