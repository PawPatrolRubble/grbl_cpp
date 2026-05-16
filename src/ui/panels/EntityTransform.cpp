#include "EntityTransform.h"
#include "../../grbl/GraphicsDesign/Entity.h"
#include "../../grbl/GraphicsDesign/LineEntity.h"
#include "../../grbl/GraphicsDesign/RectangleEntity.h"
#include "../../grbl/GraphicsDesign/CircleEntity.h"
#include "../../grbl/GraphicsDesign/ArcEntity.h"
#include "../../grbl/GraphicsDesign/PolylineEntity.h"
#include "../../grbl/GraphicsDesign/Document.h"

#include <cmath>
#include <QtMath>

namespace EntityTransform {

QPointF rotatePoint(const QPointF &pt, const QPointF &pivot, double angleDeg)
{
    const double rad = qDegreesToRadians(angleDeg);
    const double c = qCos(rad), s = qSin(rad);
    const double dx = pt.x() - pivot.x(), dy = pt.y() - pivot.y();
    return { pivot.x() + dx * c - dy * s, pivot.y() + dx * s + dy * c };
}

double normAngle(double a)
{
    a = std::fmod(a, 360.0);
    if (a < 0.0) a += 360.0;
    return a;
}

QPointF flipH(const QPointF &pt, const QPointF &pivot)
{
    return QPointF(2.0 * pivot.x() - pt.x(), pt.y());
}

QPointF flipV(const QPointF &pt, const QPointF &pivot)
{
    return QPointF(pt.x(), 2.0 * pivot.y() - pt.y());
}

std::unique_ptr<Entity> makeRotatedEntity(const Entity *e, const QPointF &pivot, double angleDeg)
{
    auto rotated = e->clone();

    switch (e->kind()) {
    case Entity::Kind::Line: {
        const auto *line = static_cast<const LineEntity*>(e);
        auto *target = static_cast<LineEntity*>(rotated.get());
        target->setStartEnd(
            rotatePoint(line->startPoint(), pivot, angleDeg),
            rotatePoint(line->endPoint(), pivot, angleDeg));
        return rotated;
    }
    case Entity::Kind::Rectangle: {
        const auto *rect = static_cast<const RectangleEntity*>(e);
        auto *target = static_cast<RectangleEntity*>(rotated.get());
        target->setGeometry(
            rotatePoint(rect->center(), pivot, angleDeg),
            rect->width(), rect->height(), rect->angleDegrees() + angleDeg);
        return rotated;
    }
    case Entity::Kind::Circle: {
        const auto *circle = static_cast<const CircleEntity*>(e);
        auto *target = static_cast<CircleEntity*>(rotated.get());
        target->setGeometry(rotatePoint(circle->center(), pivot, angleDeg), circle->radius());
        return rotated;
    }
    case Entity::Kind::Arc: {
        const auto *arc = static_cast<const ArcEntity*>(e);
        auto *target = static_cast<ArcEntity*>(rotated.get());
        target->setGeometry(
            rotatePoint(arc->center(), pivot, angleDeg),
            arc->radius(),
            arc->startAngleDegrees() + angleDeg,
            arc->sweepAngleDegrees());
        return rotated;
    }
    case Entity::Kind::Polygon: {
        const auto *poly = static_cast<const PolylineEntity*>(e);
        auto *target = static_cast<PolylineEntity*>(rotated.get());
        QList<QPointF> pts;
        pts.reserve(poly->points().size());
        for (const QPointF &pt : poly->points())
            pts.append(rotatePoint(pt, pivot, angleDeg));
        target->setPoints(pts);
        return rotated;
    }
    default:
        return rotated;
    }
}

std::unique_ptr<Entity> makeRotatedEntity(const Entity *e, double angleDeg)
{
    return makeRotatedEntity(e, e->center(), angleDeg);
}

std::unique_ptr<Entity> makeScaledEntity(const Entity *e, const QPointF &pivot, double factor)
{
    auto sp = [&](const QPointF &pt) -> QPointF {
        return pivot + (pt - pivot) * factor;
    };
    switch (e->kind()) {
    case Entity::Kind::Line: {
        const auto *line = static_cast<const LineEntity*>(e);
        return std::make_unique<LineEntity>(sp(line->startPoint()), sp(line->endPoint()));
    }
    case Entity::Kind::Circle: {
        const auto *c = static_cast<const CircleEntity*>(e);
        return std::make_unique<CircleEntity>(sp(c->center()), c->radius() * factor);
    }
    case Entity::Kind::Arc: {
        const auto *arc = static_cast<const ArcEntity*>(e);
        return std::make_unique<ArcEntity>(sp(arc->center()), arc->radius() * factor,
                                           arc->startAngleDegrees(), arc->sweepAngleDegrees());
    }
    case Entity::Kind::Rectangle: {
        const auto *rect = static_cast<const RectangleEntity*>(e);
        return std::make_unique<RectangleEntity>(sp(rect->center()),
                                                  rect->width()  * factor,
                                                  rect->height() * factor,
                                                  rect->angleDegrees());
    }
    case Entity::Kind::Polygon: {
        const auto *poly = static_cast<const PolylineEntity*>(e);
        QList<QPointF> pts;
        pts.reserve(poly->points().size());
        for (const QPointF &pt : poly->points())
            pts.append(sp(pt));
        return std::make_unique<PolylineEntity>(pts);
    }
    default:
        return e->clone();
    }
}

std::unique_ptr<Entity> makeMirroredEntity(const Entity *e, const QPointF &pivot, FlipAxis axis)
{
    auto fp = [&](const QPointF &pt) {
        return axis == FlipAxis::H ? flipH(pt, pivot) : flipV(pt, pivot);
    };

    switch (e->kind()) {
    case Entity::Kind::Line: {
        const auto *line = static_cast<const LineEntity*>(e);
        return std::make_unique<LineEntity>(fp(line->startPoint()), fp(line->endPoint()));
    }
    case Entity::Kind::Circle: {
        const auto *c = static_cast<const CircleEntity*>(e);
        return std::make_unique<CircleEntity>(fp(c->center()), c->radius());
    }
    case Entity::Kind::Arc: {
        const auto *arc = static_cast<const ArcEntity*>(e);
        QPointF newC = fp(arc->center());
        double start = arc->startAngleDegrees();
        double sweep = arc->sweepAngleDegrees();
        double newStart, newSweep = sweep;
        if (axis == FlipAxis::H) {
            // Mirror about vertical axis: angle θ → 180° - θ
            // Arc [start, start+sweep] maps to [180-(start+sweep), 180-start] CCW
            newStart = normAngle(180.0 - (start + sweep));
        } else {
            // Mirror about horizontal axis: angle θ → -θ (= 360-θ)
            // Arc [start, start+sweep] maps to [360-(start+sweep), 360-start] CCW
            newStart = normAngle(360.0 - (start + sweep));
        }
        return std::make_unique<ArcEntity>(newC, arc->radius(), newStart, newSweep);
    }
    case Entity::Kind::Rectangle: {
        const auto *rect = static_cast<const RectangleEntity*>(e);
        QPointF newC = fp(rect->center());
        double newAngle = axis == FlipAxis::H ? (180.0 - rect->angleDegrees()) : (-rect->angleDegrees());
        return std::make_unique<RectangleEntity>(newC, rect->width(), rect->height(), newAngle);
    }
    case Entity::Kind::Polygon: {
        const auto *poly = static_cast<const PolylineEntity*>(e);
        QList<QPointF> pts;
        pts.reserve(poly->points().size());
        for (const QPointF &pt : poly->points())
            pts.append(fp(pt));
        return std::make_unique<PolylineEntity>(pts);
    }
    default:
        return e->clone();  // Text, dimensions: just flip center position
    }
}

QPointF selectionCenter(Document *doc)
{
    QRectF bb;
    for (const QUuid &id : doc->selectedIds()) {
        int idx = doc->indexOf(id);
        if (idx >= 0 && doc->entityAt(idx))
            bb = bb.united(doc->entityAt(idx)->boundingRect());
    }
    return bb.center();
}

} // namespace EntityTransform