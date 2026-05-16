#pragma once

#include "Entity.h"

#include <memory>
#include <vector>

class GroupEntity final : public Entity
{
public:
    explicit GroupEntity(std::vector<std::unique_ptr<Entity>> children = {});

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
    static std::unique_ptr<GroupEntity> fromJson(const QJsonObject& json);

    TrimEnd getTrimPoint(const QPointF& clickPos, const QPointF& intersection) const override;
    QPointF pointAtTrimEnd(TrimEnd end) const override;

    const std::vector<std::unique_ptr<Entity>>& children() const { return m_children; }
    std::vector<std::unique_ptr<Entity>> takeChildren();
    void setChildren(std::vector<std::unique_ptr<Entity>> children);
    int childCount() const { return static_cast<int>(m_children.size()); }

private:
    std::vector<std::unique_ptr<Entity>> m_children;
};