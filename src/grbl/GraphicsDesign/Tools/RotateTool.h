#pragma once

#include "Tool.h"

#include <vector>

/// Interactive rotate tool.
/// Click a pivot point, then click a destination angle to rotate the current selection.
class RotateTool final : public Tool
{
public:
    using Tool::Tool;

    QString name() const override { return "Rotate"; }
    QString instructions() const override;

    void mousePress(const QPointF& pos) override;
    void mouseMove(const QPointF& pos) override;
    void mouseRelease(const QPointF& pos) override;
    void cancel() override;
    void paintPreview(QPainter& painter) const override;
    bool isActive() const override { return m_hasPivot; }
    QSet<QUuid> snapExclusionIds() const override;

private:
    bool canRotateSelection() const;
    void beginRotate(const QPointF& pos);
    void updatePreview(const QPointF& pos);
    void commitRotate();
    void reset();

    bool m_hasPivot = false;
    QPointF m_pivotPoint;
    QPointF m_currentPoint;
    QPointF m_referencePoint;
    std::vector<int> m_selectedIndices;
    std::vector<std::unique_ptr<Entity>> m_originalEntities;
    std::vector<std::unique_ptr<Entity>> m_previewEntities;
};
