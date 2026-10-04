#include "Entity.h"
#include "LineEntity.h"
#include "RectangleEntity.h"
#include "CircleEntity.h"
#include "ArcEntity.h"
#include "TextEntity.h"
#include "PolylineEntity.h"
#include "SplineEntity.h"
#include "GroupEntity.h"
#include "LinearDimensionEntity.h"
#include "AngularDimensionEntity.h"
#include "RadiusDimensionEntity.h"

Entity::Entity(Kind kind)
    : m_id(QUuid::createUuid())
    , m_kind(kind)
{
}

std::unique_ptr<Entity> Entity::fromJson(const QJsonObject& json)
{
    QString typeStr = json["type"].toString();

    std::unique_ptr<Entity> entity;
    if (typeStr == "line") entity = LineEntity::fromJson(json);
    else if (typeStr == "rectangle") entity = RectangleEntity::fromJson(json);
    else if (typeStr == "circle") entity = CircleEntity::fromJson(json);
    else if (typeStr == "arc") entity = ArcEntity::fromJson(json);
    else if (typeStr == "text") entity = TextEntity::fromJson(json);
    else if (typeStr == "polyline") entity = PolylineEntity::fromJson(json);
    else if (typeStr == "spline") entity = SplineEntity::fromJson(json);
    else if (typeStr == "dimension") entity = LinearDimensionEntity::fromJson(json);
    else if (typeStr == "angularDimension") entity = AngularDimensionEntity::fromJson(json);
    else if (typeStr == "radiusDimension") entity = RadiusDimensionEntity::fromJson(json);
    else if (typeStr == "group") entity = GroupEntity::fromJson(json);
    if (entity) {
        const QUuid id = QUuid::fromString(json["id"].toString());
        if (!id.isNull()) entity->setId(id);
        entity->setToolId(QUuid::fromString(json["toolId"].toString()));
        entity->setEnabled(json["enabled"].toBool(true));
        if (json.contains("layerId"))
            entity->setLayerId(QUuid::fromString(json["layerId"].toString()));
    }
    return entity;
}

bool Entity::isDrawable() const
{
    return kind() != Kind::LinearDimension && kind() != Kind::AngularDimension
        && kind() != Kind::RadiusDimension;
}

void Entity::copyPlotMetadataFrom(const Entity& source)
{
    m_toolId = source.m_toolId;
    m_enabled = source.m_enabled;
    if (m_layerId.isNull()) m_layerId = source.m_layerId;
}

QJsonObject Entity::toJsonWithMetadata() const
{
    QJsonObject json = toJson();
    json["id"] = id().toString();
    json["enabled"] = isEnabled();
    json["layerId"] = layerId().toString();
    if (!toolId().isNull()) json["toolId"] = toolId().toString();
    return json;
}

QString Entity::displayName() const
{
    static const QHash<Kind, QString> names = {
        {Kind::Line, "Line"}, {Kind::Rectangle, "Rectangle"},
        {Kind::Circle, "Circle"}, {Kind::Arc, "Arc"},
        {Kind::Polygon, "Polygon"}, {Kind::Text, "Text"},
        {Kind::LinearDimension, "Dimension"},
        {Kind::AngularDimension, "Angular Dim"},
        {Kind::RadiusDimension, "Radius Dim"},
        {Kind::Spline, "Spline"},
        {Kind::Group, "Group"},
    };
    return names.value(m_kind, "Entity");
}
