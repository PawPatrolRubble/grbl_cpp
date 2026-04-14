#pragma once

#include "Shape.h"

/// Rectangle shape defined by origin and dimensions.
class RectShape : public Shape
{
public:
    RectShape(const QPointF &origin = QPointF(0, 0), double width = 10.0, double height = 10.0);

    QPointF origin() const { return m_origin; }
    double width() const { return m_width; }
    double height() const { return m_height; }
    void setOrigin(const QPointF &p);
    void setWidth(double w);
    void setHeight(double h);

    // Shape interface
    QRectF boundingRect() const override;
    void draw(QPainterPath &path) const override;
    QList<QPointF> handlePositions() const override;
    QList<GcodeMove> toGcodeMoves(double zSafe, double zCut, double feedRate) const override;
    QJsonObject toJson() const override;
    static RectShape* fromJson(const QJsonObject &json);

    void translate(double dx, double dy) override;
    void scale(double sx, double sy, const QPointF &pivot) override;

private:
    QPointF m_origin;
    double m_width, m_height;
};