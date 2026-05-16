#pragma once

#include "Tool.h"

#include <vector>

/// Interactive move tool.
/// Click a base point, then click a destination point to move the current selection.
class MoveTool final : public Tool
{
public:
    using Tool::Tool;

    QString name() const override { return "Move"; }
    QString instructions() const override;

    void mousePress(const QPointF& pos) override;
    void mouseMove(const QPointF& pos) override;
    void mouseRelease(const QPointF& pos) override;
    void cancel() override;
    void paintPreview(QPainter& painter) const override;
    bool isActive() const override { return m_hasBasePoint; }
    QSet<QUuid> snapExclusionIds() const override;

private:
    void beginMove(const QPointF& pos);
    void updatePreview(const QPointF& pos);
    void commitMove();
    void reset();

    bool m_hasBasePoint = false;
    QPointF m_basePoint;
    QPointF m_currentPoint;
    std::vector<int> m_selectedIndices;
    std::vector<std::unique_ptr<Entity>> m_originalEntities;
    std::vector<std::unique_ptr<Entity>> m_previewEntities;
};
