#pragma once

#include "Tool.h"

#include <optional>

/// Select tool for selecting and manipulating existing entities.
/// Handles selection, dragging handles, and moving entities.
class SelectTool final : public Tool
{
public:
    SelectTool(Document& document, CommandStack& commandStack);

    QString name() const override;
    QString instructions() const override;

    void mousePress(const QPointF& pos) override;
    void mouseMove(const QPointF& pos) override;
    void mouseRelease(const QPointF& pos) override;
    void cancel() override;
    void paintPreview(QPainter& painter) const override;
    bool isActive() const override;

private:
    enum class Mode {
        None,
        Selecting,
        DraggingHandle,
        MovingEntity
    };

    Mode m_mode = Mode::None;
    QPointF m_pressPos;
    QPointF m_lastMovePos;
    QPointF m_currentPos;
    QString m_activeHandleId;
    std::unique_ptr<Entity> m_dragEntityPreview;
};