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

    if (typeStr == "line")
        return LineEntity::fromJson(json);
    if (typeStr == "rectangle")
        return RectangleEntity::fromJson(json);
    if (typeStr == "circle")
        return CircleEntity::fromJson(json);
    if (typeStr == "arc")
        return ArcEntity::fromJson(json);
    if (typeStr == "text")
        return TextEntity::fromJson(json);
    if (typeStr == "polyline")
        return PolylineEntity::fromJson(json);
    if (typeStr == "spline")
        return SplineEntity::fromJson(json);
    if (typeStr == "dimension")
        return LinearDimensionEntity::fromJson(json);
    if (typeStr == "angularDimension")
        return AngularDimensionEntity::fromJson(json);
    if (typeStr == "radiusDimension")
        return RadiusDimensionEntity::fromJson(json);
    if (typeStr == "group")
        return GroupEntity::fromJson(json);

    return nullptr;
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