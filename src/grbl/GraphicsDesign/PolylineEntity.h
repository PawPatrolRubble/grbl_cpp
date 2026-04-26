#pragma once

#include "Entity.h"

#include <QList>
#include <QPointF>

/// Polyline entity defined by a sequence of connected points.
/// Generates a single continuous toolpath (one pen-down, multiple G1 moves, one pen-up).
class PolylineEntity final : public Entity
{
public:
    explicit PolylineEntity(const QList<QPointF>& points);

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
    QString displayName() const override;
    static std::unique_ptr<PolylineEntity> fromJson(const QJsonObject& json);

    // Trim support - polyline trimming is not directly supported
    TrimEnd getTrimPoint(const QPointF& clickPos, const QPointF& intersection) const override;
    QPointF pointAtTrimEnd(TrimEnd end) const override;

    // Geometry
    const QList<QPointF>& points() const { return m_points; }
    void setPoints(const QList<QPointF>& points) { m_points = points; }
    int pointCount() const { return m_points.size(); }

private:
    QList<QPointF> m_points;
};
