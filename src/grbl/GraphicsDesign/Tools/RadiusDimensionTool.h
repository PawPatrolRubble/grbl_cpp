#pragma once

#include "Tool.h"

/// Radius / Diameter dimension tool.
/// Two-click workflow:
///   1st click = circle center
///   2nd click = point on circumference (sets radius and leader angle)
/// Holding Shift while clicking the second point switches to diameter mode.
class RadiusDimensionTool final : public Tool
{
public:
    RadiusDimensionTool(Document& document, CommandStack& commandStack);

    QString name() const override;
    QString instructions() const override;

    void mousePress(const QPointF& pos) override;
    void mouseMove(const QPointF& pos) override;
    void mouseRelease(const QPointF& pos) override;
    void cancel() override;
    void paintPreview(QPainter& painter) const override;
    bool isActive() const override;

    void setDiameterMode(bool on) { m_diameter = on; }
    bool isDiameterMode() const { return m_diameter; }

private:
    enum class Phase { Idle, HasCenter };
    Phase   m_phase = Phase::Idle;
    QPointF m_center;
    QPointF m_currentPos;
    bool    m_diameter = false;
};
