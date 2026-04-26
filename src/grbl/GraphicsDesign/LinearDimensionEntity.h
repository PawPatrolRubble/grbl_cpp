#pragma once

#include "Entity.h"

#include <QPointF>
#include <QString>

/// Linear dimension entity.
/// Shows the distance between two points with an offset dimension line
/// and extension lines, like an engineering drawing annotation.
/// Dimension annotations are NOT converted to G-code moves
/// (they are drawing aids only).
class LinearDimensionEntity final : public Entity
{
public:
    /// p1 / p2 are the two points being dimensioned.
    /// offsetDistance is the perpendicular offset of the dimension line
    /// (positive = left of the p1→p2 direction).
    LinearDimensionEntity(const QPointF& p1, const QPointF& p2,
                          double offsetDistance = 10.0);

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
    static std::unique_ptr<LinearDimensionEntity> fromJson(const QJsonObject& json);
    QString displayName() const override;

    // Trim support - dimension entities are not trimmable
    TrimEnd getTrimPoint(const QPointF& clickPos, const QPointF& intersection) const override;
    QPointF pointAtTrimEnd(TrimEnd end) const override;

    // Geometry
    QPointF p1() const { return m_p1; }
    QPointF p2() const { return m_p2; }
    double offsetDistance() const { return m_offsetDistance; }
    void setOffsetDistance(double d) { m_offsetDistance = d; }
    double measuredLength() const;

private:
    // Returns the unit perpendicular to the p1→p2 direction (rotated 90° CCW)
    QPointF perp() const;

    QPointF m_p1;
    QPointF m_p2;
    double m_offsetDistance = 10.0;
};
