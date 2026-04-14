#pragma once

#include "Tool.h"

#include <optional>

/// Rectangle drawing tool.
/// Click for first corner, click again for opposite corner.
class RectangleTool final : public Tool
{
public:
    RectangleTool(Document& document, CommandStack& commandStack);

    QString name() const override;
    QString instructions() const override;

    void mousePress(const QPointF& pos) override;
    void mouseMove(const QPointF& pos) override;
    void mouseRelease(const QPointF& pos) override;
    void cancel() override;
    void paintPreview(QPainter& painter) const override;
    bool isActive() const override;

private:
    std::optional<QPointF> m_firstCorner;
    QPointF m_currentCorner;
};