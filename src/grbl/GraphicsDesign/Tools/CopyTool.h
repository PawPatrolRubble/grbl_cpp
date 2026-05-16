#pragma once

#include "Tool.h"

#include <vector>

/// Interactive copy tool.
/// Click a base point, then click a destination point to copy the current selection.
class CopyTool final : public Tool
{
public:
    using Tool::Tool;

    QString name() const override { return "Copy"; }
    QString instructions() const override;

    void mousePress(const QPointF& pos) override;
    void mouseMove(const QPointF& pos) override;
    void mouseRelease(const QPointF& pos) override;
    void cancel() override;
    void paintPreview(QPainter& painter) const override;
    bool isActive() const override { return m_hasBasePoint; }
    QSet<QUuid> snapExclusionIds() const override;

private:
    void beginCopy(const QPointF& pos);
    void updatePreview(const QPointF& pos);
    void commitCopy();
    void reset();

    bool m_hasBasePoint = false;
    QPointF m_basePoint;
    QPointF m_currentPoint;
    std::vector<std::unique_ptr<Entity>> m_originalEntities;
    std::vector<std::unique_ptr<Entity>> m_previewEntities;
};
