#pragma once

#include "Entity.h"

#include <QPointF>

/// Radius/diameter dimension entity.
/// Displays the radius (or diameter) of a circle as a leader line
/// from the center to a point on the circumference, with an arrowhead
/// and a label showing "R <value>" (or "⌀ <value>" for diameter mode).
/// This annotation is NOT converted to G-code moves.
class RadiusDimensionEntity final : public Entity
{
public:
    /// center   – center of the measured circle
    /// radius   – radius of the circle
    /// angleDeg – angle (in degrees) of the leader line direction
    /// diameter – if true, shows "⌀" label with double the radius
    RadiusDimensionEntity(const QPointF& center,
                          double radius,
                          double angleDeg = 0.0,
                          bool diameter = false);

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
    static std::unique_ptr<RadiusDimensionEntity> fromJson(const QJsonObject& json);
    QString displayName() const override;

    // Trim support - dimension entities are not trimmable
    TrimEnd getTrimPoint(const QPointF& clickPos, const QPointF& intersection) const override;
    QPointF pointAtTrimEnd(TrimEnd end) const override;

    // Geometry accessors
    QPointF circleCenter() const { return m_center; }
    double circleRadius() const { return m_radius; }
    double angleDeg() const { return m_angleDeg; }
    bool isDiameter() const { return m_diameter; }
    void setDiameter(bool d) { m_diameter = d; }
    void setRadius(double r) { m_radius = r; }
    void setAngleDeg(double a) { m_angleDeg = a; }

private:
    // The tip point on circumference (direction from center by angleDeg)
    QPointF tipPoint() const;

    QPointF m_center;
    double  m_radius   = 10.0;
    double  m_angleDeg = 0.0;   // direction of the leader line
    bool    m_diameter = false;
};
