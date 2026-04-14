#pragma once

#include "Shape.h"

/// Arc shape defined by center, radius, start angle and sweep angle.
class ArcShape : public Shape
{
public:
    ArcShape(const QPointF &center = QPointF(0, 0),
             double radius = 10.0,
             double startAngle = 0.0,
             double sweepAngle = 90.0);

    QPointF centerPoint() const { return m_center; }
    double radius() const { return m_radius; }
    double startAngle() const { return m_startAngle; }
    double sweepAngle() const { return m_sweepAngle; }

    void setCenter(const QPointF &c);
    void setRadius(double r);
    void setStartAngle(double deg);
    void setSweepAngle(double deg);

    QPointF startPoint() const;  // Calculated from center/radius/angle
    QPointF endPoint() const;

    // Shape interface
    QRectF boundingRect() const override;
    void draw(QPainterPath &path) const override;
    QList<QPointF> handlePositions() const override;
    QList<GcodeMove> toGcodeMoves(double zSafe, double zCut, double feedRate) const override;
    QJsonObject toJson() const override;
    static ArcShape* fromJson(const QJsonObject &json);

    void translate(double dx, double dy) override;
    void scale(double sx, double sy, const QPointF &pivot) override;

private:
    QPointF m_center;
    double m_radius;
    double m_startAngle;  // Degrees, 0 = East (positive X)
    double m_sweepAngle;  // Positive = CCW, negative = CW
};