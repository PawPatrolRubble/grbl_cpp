#pragma once

#include "Entity.h"

/// Rectangle entity with center, dimensions, and optional rotation.
/// Merges tinyCad's RectangleEntity (editHandles, snapPoints)
/// with grbl_cpp's RectShape (toGcodeMoves).
class RectangleEntity final : public Entity
{
public:
    RectangleEntity(const QPointF& firstCorner, const QPointF& secondCorner);
    RectangleEntity(const QPointF& center, double width, double height, double angleDegrees = 0.0);

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
    static std::unique_ptr<RectangleEntity> fromJson(const QJsonObject& json);

    // Geometry
    double width() const;
    double height() const;
    double angleDegrees() const;
    void setGeometry(const QPointF& center, double width, double height, double angleDegrees);

private:
    QPointF toLocal(const QPointF& worldPoint) const;
    std::array<QPointF, 4> corners() const;

    QPointF m_center;
    double m_width = 0.0;
    double m_height = 0.0;
    double m_angleDegrees = 0.0;
};