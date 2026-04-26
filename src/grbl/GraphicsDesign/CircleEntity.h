#pragma once

#include "Entity.h"

/// Circle entity defined by center and radius.
/// Merges tinyCad's CircleEntity (editHandles, snapPoints, distanceTo)
/// with grbl_cpp's CircleShape (toGcodeMoves).
class CircleEntity final : public Entity
{
public:
    CircleEntity(const QPointF& center, double radius);

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
    static std::unique_ptr<CircleEntity> fromJson(const QJsonObject& json);

    // Trim support - circle trimming converts to arc
    TrimEnd getTrimPoint(const QPointF& clickPos, const QPointF& intersection) const override;
    QPointF pointAtTrimEnd(TrimEnd end) const override;

    // Geometry
    double radius() const;
    void setGeometry(const QPointF& center, double radius);

private:
    QPointF m_center;
    double m_radius = 1.0;
};