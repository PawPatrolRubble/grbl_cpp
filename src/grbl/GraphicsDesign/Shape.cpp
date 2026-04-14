#include "Shape.h"
#include "LineShape.h"
#include "RectShape.h"
#include "CircleShape.h"
#include "ArcShape.h"

Shape::Shape(Type type)
    : m_id(QUuid::createUuid())
    , m_type(type)
{
}

QPointF Shape::center() const
{
    QRectF bbox = boundingRect();
    return QPointF(bbox.center().x(), bbox.center().y());
}

void Shape::setRotation(double degrees)
{
    m_rotation = degrees;
}

Shape* Shape::fromJson(const QJsonObject &json)
{
    QString typeStr = json["type"].toString();

    if (typeStr == "line") {
        return LineShape::fromJson(json);
    } else if (typeStr == "rect") {
        return RectShape::fromJson(json);
    } else if (typeStr == "circle") {
        return CircleShape::fromJson(json);
    } else if (typeStr == "arc") {
        return ArcShape::fromJson(json);
    }

    return nullptr;
}