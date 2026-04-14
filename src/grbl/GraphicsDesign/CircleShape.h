#pragma once

#include "Shape.h"

/// Circle shape defined by center and radius.
class CircleShape : public Shape
{
public:
    CircleShape(const QPointF &center = QPointF(0, 0), double radius = 10.0);

    QPointF centerPoint() const { return m_center; }
    double radius() const { return m_radius; }
    void setCenter(const QPointF &c);
    void setRadius(double r);

    // Shape interface
    QRectF boundingRect() const override;
    void draw(QPainterPath &path) const override;
    QList<QPointF> handlePositions() const override;
    QList<GcodeMove> toGcodeMoves(double zSafe, double zCut, double feedRate) const override;
    QJsonObject toJson() const override;
    static CircleShape* fromJson(const QJsonObject &json);

    void translate(double dx, double dy) override;
    void scale(double sx, double sy, const QPointF &pivot) override;

private:
    QPointF m_center;
    double m_radius;
};