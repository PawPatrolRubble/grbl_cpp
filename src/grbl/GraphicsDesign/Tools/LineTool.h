#pragma once

#include "Tool.h"

#include <optional>

/// Line drawing tool.
/// Click to set start point, click again to finish line.
class LineTool final : public Tool
{
public:
    LineTool(Document& document, CommandStack& commandStack);

    QString name() const override;
    QString instructions() const override;

    void mousePress(const QPointF& pos) override;
    void mouseMove(const QPointF& pos) override;
    void mouseRelease(const QPointF& pos) override;
    void cancel() override;
    void paintPreview(QPainter& painter) const override;
    bool isActive() const override;
    std::optional<QPointF> inferenceReferencePoint() const override;

private:
    std::optional<QPointF> m_startPoint;
    QPointF m_currentPoint;
};