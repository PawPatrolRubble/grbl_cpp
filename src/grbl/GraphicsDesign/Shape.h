#pragma once

#include <QPointF>
#include <QRectF>
#include <QUuid>
#include <QJsonObject>
#include <QPainterPath>
#include <QList>
#include "../GcodeParser.h"

/// Abstract base class for all drawable shapes.
/// Each shape can be drawn, transformed, serialized, and converted to G-code moves.
class Shape
{
public:
    enum class Type { Line, Rectangle, Circle, Arc };
    enum class ToolMode { PenPlotter, Laser };

    explicit Shape(Type type);
    virtual ~Shape() = default;

    // Identity
    QUuid id() const { return m_id; }
    Type type() const { return m_type; }

    // Geometry
    virtual QRectF boundingRect() const = 0;
    virtual QPointF center() const;
    double rotation() const { return m_rotation; }
    void setRotation(double degrees);

    // Rendering
    virtual void draw(QPainterPath &path) const = 0;
    virtual QList<QPointF> handlePositions() const = 0;

    // G-code generation
    virtual QList<GcodeMove> toGcodeMoves(double zSafe, double zCut, double feedRate) const = 0;

    // Serialization
    virtual QJsonObject toJson() const = 0;
    static Shape* fromJson(const QJsonObject &json);

    // Transform
    virtual void translate(double dx, double dy) = 0;
    virtual void scale(double sx, double sy, const QPointF &pivot) = 0;

protected:
    QUuid m_id;
    Type m_type;
    double m_rotation = 0.0;
};