#pragma once

#include "Entity.h"

#include <QLineF>

/// Line entity defined by two points.
/// Merges tinyCad's LineEntity (editHandles, snapPoints, distanceTo)
/// with grbl_cpp's LineShape (toGcodeMoves).
class LineEntity final : public Entity
{
public:
    LineEntity(const QPointF& start, const QPointF& end);

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
    static std::unique_ptr<LineEntity> fromJson(const QJsonObject& json);

    // Geometry
    QPointF startPoint() const;
    QPointF endPoint() const;
    double length() const;
    double angleDegrees() const;
    void setStartEnd(const QPointF& start, const QPointF& end);
    void setCenterLengthAngle(const QPointF& center, double length, double angleDegrees);

private:
    QLineF m_line;
};