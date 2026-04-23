#pragma once

#include <QPointF>
#include <QRectF>
#include <memory>

class Entity;
class Document;

namespace EntityTransform {

// Rotate a point around a pivot by given angle (degrees)
QPointF rotatePoint(const QPointF &pt, const QPointF &pivot, double angleDeg);

// Normalize angle to [0, 360)
double normAngle(double a);

// Flip point horizontally about pivot (mirror about vertical axis)
QPointF flipH(const QPointF &pt, const QPointF &pivot);

// Flip point vertically about pivot (mirror about horizontal axis)
QPointF flipV(const QPointF &pt, const QPointF &pivot);

// Mirror axis direction for makeMirroredEntity
enum class FlipAxis { H, V };

// Create a rotated copy of an entity (around entity's own center)
std::unique_ptr<Entity> makeRotatedEntity(const Entity *e, double angleDeg);

// Create a scaled copy of an entity around a pivot point
std::unique_ptr<Entity> makeScaledEntity(const Entity *e, const QPointF &pivot, double factor);

// Create a mirrored copy of an entity about a pivot point and axis
std::unique_ptr<Entity> makeMirroredEntity(const Entity *e, const QPointF &pivot, FlipAxis axis);

// Compute the center of the bounding box containing all selected entities
QPointF selectionCenter(Document *doc);

} // namespace EntityTransform