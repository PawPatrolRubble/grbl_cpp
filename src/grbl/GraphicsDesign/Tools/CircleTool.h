#pragma once

#include "Tool.h"

#include <optional>

/// Circle drawing tool.
/// Click for center, click again for radius point.
class CircleTool final : public Tool
{
public:
    CircleTool(Document& document, CommandStack& commandStack);

    QString name() const override;
    QString instructions() const override;

    void mousePress(const QPointF& pos) override;
    void mouseMove(const QPointF& pos) override;
    void mouseRelease(const QPointF& pos) override;
    void cancel() override;
    void paintPreview(QPainter& painter) const override;
    bool isActive() const override;

private:
    std::optional<QPointF> m_center;
    QPointF m_currentPoint;
};