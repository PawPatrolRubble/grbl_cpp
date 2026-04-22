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

    /// Set by the canvas before mousePress using screen-space hit-testing.
    void setHitHandleId(const QString& id) { m_hitHandleId = id; }

    /// Set by the canvas before mousePress to indicate Ctrl key was held.
    void setCtrlPressed(bool ctrl) { m_ctrlPressed = ctrl; }

    /// Returns the rubber-band rect (machine coords) when in RubberBand mode.
    std::optional<QRectF> rubberBandRect() const;

private:
    enum class Mode {
        None,
        Selecting,
        DraggingHandle,
        MovingEntity,
        RubberBand
    };

    Mode m_mode = Mode::None;
    QPointF m_pressPos;
    QPointF m_lastMovePos;
    QPointF m_currentPos;
    QString m_activeHandleId;
    QString m_hitHandleId;  ///< Pre-resolved by canvas (screen-space hit-test)
    bool m_ctrlPressed = false;
    std::unique_ptr<Entity> m_dragEntityPreview;
};