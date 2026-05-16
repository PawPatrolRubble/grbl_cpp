#pragma once

#include "Tool.h"

#include <optional>

/// Angular dimension tool.
/// Workflow:
///   1st click = first line-like entity segment
///   2nd click = second line-like entity segment, creating an angle annotation
class AngularDimensionTool : public Tool
{
public:
    struct Segment {
        QPointF start;
        QPointF end;
        QPointF pickPoint;
        QPointF mousePoint;
        bool hasPickPoint = false;
        bool hasMousePoint = false;
    };

    AngularDimensionTool(Document& document, CommandStack& commandStack);

    QString name() const override;
    QString instructions() const override;

    void mousePress(const QPointF& pos) override;
    void mouseMove(const QPointF& pos) override;
    void mouseRelease(const QPointF& pos) override;
    void cancel() override;
    void paintPreview(QPainter& painter) const override;
    bool isActive() const override;

    bool hasMeasurement() const { return m_hasResult; }
    bool hasLinearMeasurement() const { return false; }
    double distance() const { return 0.0; }
    double deltaX() const { return 0.0; }
    double deltaY() const { return 0.0; }
    double angle() const { return m_angle; }

private:
    bool m_hasFirst = false;
    bool m_hasHover = false;
    bool m_hasResult = false;
    Segment m_firstSegment;
    Segment m_secondSegment;
    Segment m_hoverSegment;
    double m_angle = 0.0;

    std::optional<Segment> pickSegment(const QPointF& pos) const;
    bool createAngleAnnotation(const Segment& secondSegment);
};
