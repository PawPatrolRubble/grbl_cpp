#include "GroupEntity.h"

#include <QJsonArray>

#include <algorithm>
#include <limits>

namespace {

bool isDimensionKind(Entity::Kind kind)
{
    return kind == Entity::Kind::LinearDimension
        || kind == Entity::Kind::AngularDimension
        || kind == Entity::Kind::RadiusDimension;
}

QJsonObject entityToNestedJson(const Entity& entity)
{
    return entity.toJsonWithMetadata();
}

} // namespace

GroupEntity::GroupEntity(std::vector<std::unique_ptr<Entity>> children)
    : Entity(Kind::Group)
    , m_children(std::move(children))
{
    for (const auto& child : m_children) {
        if (child && !child->layerId().isNull()) {
            setLayerId(child->layerId());
            break;
        }
    }
}

Entity::Kind GroupEntity::kind() const
{
    return Kind::Group;
}

QRectF GroupEntity::boundingRect() const
{
    QRectF bounds;
    bool hasBounds = false;
    for (const auto& child : m_children) {
        if (!child) continue;
        const QRectF childBounds = child->boundingRect();
        bounds = hasBounds ? bounds.united(childBounds) : childBounds;
        hasBounds = true;
    }
    return bounds;
}

QPointF GroupEntity::center() const
{
    return boundingRect().center();
}

void GroupEntity::draw(QPainter& painter) const
{
    for (const auto& child : m_children) {
        if (child && child->isEnabled())
            child->draw(painter);
    }
}

double GroupEntity::distanceTo(const QPointF& point) const
{
    double best = std::numeric_limits<double>::max();
    for (const auto& child : m_children) {
        if (!child || !child->isEnabled()) continue;
        best = std::min(best, child->distanceTo(point));
    }
    return best;
}

void GroupEntity::translate(const QPointF& delta)
{
    for (auto& child : m_children) {
        if (child)
            child->translate(delta);
    }
}

std::unique_ptr<Entity> GroupEntity::clone() const
{
    std::vector<std::unique_ptr<Entity>> children;
    children.reserve(m_children.size());
    for (const auto& child : m_children) {
        if (child)
            children.push_back(child->clone());
    }

    auto group = std::make_unique<GroupEntity>(std::move(children));
    copyBaseFieldsTo(group.get());
    return group;
}

std::vector<Entity::EditHandle> GroupEntity::editHandles() const
{
    return {};
}

std::unique_ptr<Entity> GroupEntity::withDraggedHandle(const QString& handleId, const QPointF& worldPos) const
{
    Q_UNUSED(handleId);
    Q_UNUSED(worldPos);
    return clone();
}

std::vector<QPointF> GroupEntity::snapPoints() const
{
    std::vector<QPointF> points;
    for (const auto& child : m_children) {
        if (!child || !child->isEnabled()) continue;
        const auto childPoints = child->snapPoints();
        points.insert(points.end(), childPoints.begin(), childPoints.end());
    }

    const QRectF bounds = boundingRect();
    if (bounds.isValid() && !bounds.isEmpty())
        points.push_back(bounds.center());

    return points;
}

QList<GcodeMove> GroupEntity::toGcodeMoves(double zSafe, double zCut, double feedRate) const
{
    QList<GcodeMove> moves;
    for (const auto& child : m_children) {
        if (!child || !child->isEnabled()) continue;
        if (isDimensionKind(child->kind())) continue;

        const QList<GcodeMove> childMoves = child->toGcodeMoves(zSafe, zCut, feedRate);
        for (const GcodeMove& move : childMoves)
            moves.append(move);
    }
    return moves;
}

QJsonObject GroupEntity::toJson() const
{
    QJsonObject json;
    json["type"] = "group";
    json["rotation"] = rotation();

    QJsonArray children;
    for (const auto& child : m_children) {
        if (child)
            children.append(entityToNestedJson(*child));
    }
    json["children"] = children;
    return json;
}

QString GroupEntity::displayName() const
{
    return QString("Group (%1)").arg(m_children.size());
}

std::unique_ptr<GroupEntity> GroupEntity::fromJson(const QJsonObject& json)
{
    std::vector<std::unique_ptr<Entity>> children;
    const QJsonArray childArray = json["children"].toArray();
    children.reserve(static_cast<std::size_t>(childArray.size()));

    for (const QJsonValue& value : childArray) {
        const QJsonObject childJson = value.toObject();
        auto child = Entity::fromJson(childJson);
        if (!child) continue;

        child->setEnabled(childJson["enabled"].toBool(true));
        const QUuid layerId = QUuid::fromString(childJson["layerId"].toString());
        if (!layerId.isNull())
            child->setLayerId(layerId);
        children.push_back(std::move(child));
    }

    auto group = std::make_unique<GroupEntity>(std::move(children));
    group->setRotation(json["rotation"].toDouble(0));
    return group;
}

TrimEnd GroupEntity::getTrimPoint(const QPointF& clickPos, const QPointF& intersection) const
{
    Q_UNUSED(clickPos);
    Q_UNUSED(intersection);
    return TrimEnd::None;
}

QPointF GroupEntity::pointAtTrimEnd(TrimEnd end) const
{
    Q_UNUSED(end);
    return center();
}

std::vector<std::unique_ptr<Entity>> GroupEntity::takeChildren()
{
    return std::move(m_children);
}

void GroupEntity::setChildren(std::vector<std::unique_ptr<Entity>> children)
{
    m_children = std::move(children);
}
