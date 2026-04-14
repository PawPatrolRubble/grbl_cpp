#pragma once

#include "Shape.h"

/// Line shape defined by two points.
class LineShape : public Shape
{
public:
    LineShape(const QPointF &p1 = QPointF(0, 0), const QPointF &p2 = QPointF(10, 0));

    QPointF startPoint() const { return m_p1; }
    QPointF endPoint() const { return m_p2; }
    void setStartPoint(const QPointF &p);
    void setEndPoint(const QPointF &p);

    // Shape interface
    QRectF boundingRect() const override;
    void draw(QPainterPath &path) const override;
    QList<QPointF> handlePositions() const override;
    QList<GcodeMove> toGcodeMoves(double zSafe, double zCut, double feedRate) const override;
    QJsonObject toJson() const override;
    static LineShape* fromJson(const QJsonObject &json);

    void translate(double dx, double dy) override;
    void scale(double sx, double sy, const QPointF &pivot) override;

private:
    QPointF m_p1, m_p2;
};