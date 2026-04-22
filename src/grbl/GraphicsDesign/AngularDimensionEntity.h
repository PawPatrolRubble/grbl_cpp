#pragma once

#include "Entity.h"

#include <QPointF>

/// Angular dimension entity.
/// Measures and displays the angle at a vertex formed by two rays:
///   vertex → p1 and vertex → p2.
/// A circular arc is drawn at the given radius from the vertex,
/// with arrow heads, and the angle label in degrees.
/// Angular dimension annotations are NOT converted to G-code moves.
class AngularDimensionEntity final : public Entity
{
public:
    /// vertex  – the apex of the angle
    /// p1, p2  – points on the two rays (they define the direction only)
    /// radius  – arc radius for the dimension (in world units)
    AngularDimensionEntity(const QPointF& vertex,
                           const QPointF& p1,
                           const QPointF& p2,
                           double radius = 15.0);

    // Entity interface
    Kind kind() const override;
    QRectF boundingRect() const override;
    QPointF center() const override;
    void draw(QPainter& painter) const override;
    double distanceTo(const QPointF& point) const override;
    void translate(const QPointF& delta) override;
    std::unique_ptr<Entity> clone() const override;
    std::vector<EditHandle> editHandles() const override;
    std::unique_ptr<Entity> withDraggedHandle(const QString& handleId, const QPointF& worldPos) const override;
    std::vector<QPointF> snapPoints() const override;
    QList<GcodeMove> toGcodeMoves(double zSafe, double zCut, double feedRate) const override;
    QJsonObject toJson() const override;
    static std::unique_ptr<AngularDimensionEntity> fromJson(const QJsonObject& json);
    QString displayName() const override;

    // Geometry accessors
    QPointF vertex() const { return m_vertex; }
    QPointF p1() const { return m_p1; }
    QPointF p2() const { return m_p2; }
    double radius() const { return m_radius; }
    void setRadius(double r) { m_radius = r; }
    double measuredAngleDegrees() const;

private:
    // Angle of a point from vertex relative to +X axis, in degrees [0, 360)
    double angleDeg(const QPointF& p) const;
    // Normalise angle to [0,360)
    static double normDeg(double a);
    // Sweep angle from a1 to a2 going CCW, result in (0, 360]
    static double sweepCCW(double a1, double a2);

    QPointF m_vertex;
    QPointF m_p1;
    QPointF m_p2;
    double  m_radius = 15.0;
};
