#pragma once

#include "Tool.h"

#include <QList>
#include <optional>

/// Polyline drawing tool.
/// Click to add vertices, then finish with Enter or double-click.
class PolylineTool final : public Tool
{
public:
    PolylineTool(Document& document, CommandStack& commandStack);

    QString name() const override;
    QString instructions() const override;

    void mousePress(const QPointF& pos) override;
    void mouseMove(const QPointF& pos) override;
    void mouseRelease(const QPointF& pos) override;
    void cancel() override;
    void paintPreview(QPainter& painter) const override;
    bool isActive() const override;
    std::optional<QPointF> inferenceReferencePoint() const override;

    bool finish();
    bool removeLastVertex();

private:
    QList<QPointF> m_points;
    QPointF m_currentPoint;
};