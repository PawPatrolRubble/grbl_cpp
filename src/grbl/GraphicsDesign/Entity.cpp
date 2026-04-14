#include "Entity.h"
#include "LineEntity.h"
#include "RectangleEntity.h"
#include "CircleEntity.h"
#include "ArcEntity.h"

Entity::Entity(Kind kind)
    : m_id(QUuid::createUuid())
    , m_kind(kind)
{
}

std::unique_ptr<Entity> Entity::fromJson(const QJsonObject& json)
{
    QString typeStr = json["type"].toString();

    if (typeStr == "line")
        return LineEntity::fromJson(json);
    if (typeStr == "rectangle")
        return RectangleEntity::fromJson(json);
    if (typeStr == "circle")
        return CircleEntity::fromJson(json);
    if (typeStr == "arc")
        return ArcEntity::fromJson(json);

    return nullptr;
}