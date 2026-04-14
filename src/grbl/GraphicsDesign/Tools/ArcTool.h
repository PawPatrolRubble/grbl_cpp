#pragma once

#include "Tool.h"

#include <optional>

/// Arc drawing tool.
/// Click for center, click for radius/start angle, click for end angle.
class ArcTool final : public Tool
{
public:
    ArcTool(Document& document, CommandStack& commandStack);

    QString name() const override;
    QString instructions() const override;

    void mousePress(const QPointF& pos) override;
    void mouseMove(const QPointF& pos) override;
    void mouseRelease(const QPointF& pos) override;
    void cancel() override;
    void paintPreview(QPainter& painter) const override;
    bool isActive() const override;

private:
    enum class Step { None, CenterSet, RadiusSet };

    Step m_step = Step::None;
    QPointF m_center;
    double m_radius = 0.0;
    double m_startAngle = 0.0;
    QPointF m_currentPoint;
};