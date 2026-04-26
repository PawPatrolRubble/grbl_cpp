#pragma once

#include "Entity.h"

/// Arc entity defined by center, radius, start angle and sweep angle.
/// Merges grbl_cpp's ArcShape (toGcodeMoves) with Entity system.
class ArcEntity final : public Entity
{
public:
    ArcEntity(const QPointF& center, double radius,
              double startAngleDegrees, double sweepAngleDegrees);

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
    static std::unique_ptr<ArcEntity> fromJson(const QJsonObject& json);

    // Trim support
    TrimEnd getTrimPoint(const QPointF& clickPos, const QPointF& intersection) const override;
    QPointF pointAtTrimEnd(TrimEnd end) const override;

    // Geometry
    double radius() const;
    double startAngleDegrees() const;
    double sweepAngleDegrees() const;
    QPointF startPoint() const;
    QPointF endPoint() const;
    void setGeometry(const QPointF& center, double radius,
                     double startAngleDegrees, double sweepAngleDegrees);

private:
    QPointF m_center;
    double m_radius = 1.0;
    double m_startAngleDegrees = 0.0;  // 0 = East (positive X axis)
    double m_sweepAngleDegrees = 90.0; // Positive = CCW, negative = CW
};