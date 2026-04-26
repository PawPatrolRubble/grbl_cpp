#include "TrimModification.h"
#include "Document.h"
#include "LineEntity.h"
#include "ArcEntity.h"
#include "CircleEntity.h"
#include "GeometryUtils.h"

#include <QDebug>
#include <algorithm>
#include <cmath>

TrimModification::TrimModification(Document& doc)
    : m_document(doc)
{
}

TrimModification::TrimResult TrimModification::quickTrim(const Entity* entityToTrim,
                                                          int entityIndex,
                                                          const QPointF& clickPos)
{
    TrimResult result;

    if (!entityToTrim) {
        result.errorMessage = "Null entity";
        qDebug() << "quickTrim: Null entity";
        return result;
    }

    qDebug() << "quickTrim: Entity kind=" << static_cast<int>(entityToTrim->kind())
             << "index=" << entityIndex << "clickPos=" << clickPos;

    // Collect all visible entities for intersection search
    std::vector<const Entity*> allEntities;
    for (int i = 0; i < m_document.entityCount(); ++i) {
        const Entity* e = m_document.entityAt(i);
        if (e && m_document.isEntityVisible(e)) {
            allEntities.push_back(e);
        }
    }

    qDebug() << "quickTrim: Total visible entities=" << allEntities.size();

    // Find all intersections with other entities
    auto intersections = GeometryUtils::findAllIntersections(entityToTrim, entityIndex, allEntities);

    qDebug() << "quickTrim: Found intersections=" << intersections.size();
    for (const auto& pt : intersections) {
        qDebug() << "  intersection at" << pt;
    }

    if (intersections.empty()) {
        result.errorMessage = "No intersections found";
        qDebug() << "quickTrim: No intersections found";
        return result;
    }

    // Find the nearest intersection to click position
    QPointF nearestIntersection;
    double nearestDist = std::numeric_limits<double>::max();

    for (const auto& pt : intersections) {
        double dist = std::hypot(pt.x() - clickPos.x(), pt.y() - clickPos.y());
        if (dist < nearestDist) {
            nearestDist = dist;
            nearestIntersection = pt;
        }
    }

    // Delegate to trimAtIntersection
    return trimAtIntersection(entityToTrim, nearestIntersection, clickPos);
}

TrimModification::TrimResult TrimModification::trimAtIntersection(const Entity* entityToTrim,
                                                                   const QPointF& intersection,
                                                                   const QPointF& clickPos)
{
    TrimResult result;

    if (!entityToTrim) {
        result.errorMessage = "Null entity";
        qDebug() << "trimAtIntersection: Null entity";
        return result;
    }

    auto kind = entityToTrim->kind();
    qDebug() << "trimAtIntersection: Entity kind=" << static_cast<int>(kind)
             << "intersection=" << intersection << "clickPos=" << clickPos;

    if (kind == Entity::Kind::Line) {
        const auto* line = static_cast<const LineEntity*>(entityToTrim);
        result = trimLine(line, clickPos, {intersection});
    }
    else if (kind == Entity::Kind::Arc) {
        const auto* arc = static_cast<const ArcEntity*>(entityToTrim);
        result = trimArc(arc, clickPos, {intersection});
    }
    else if (kind == Entity::Kind::Circle) {
        const auto* circle = static_cast<const CircleEntity*>(entityToTrim);
        result = trimCircleToArc(circle, intersection, clickPos);
    }
    else {
        result.errorMessage = "Unsupported entity type for trimming";
        qDebug() << "trimAtIntersection: Unsupported entity type";
    }

    qDebug() << "trimAtIntersection: Result success=" << result.success
             << "replacements=" << result.replacements.size()
             << "errorMessage=" << result.errorMessage;

    return result;
}

TrimModification::TrimResult TrimModification::extendEntity(const Entity* entityToExtend,
                                                            const QPointF& targetPoint,
                                                            TrimEnd extendEnd)
{
    TrimResult result;

    if (!entityToExtend) {
        result.errorMessage = "Null entity";
        return result;
    }

    auto kind = entityToExtend->kind();

    if (kind == Entity::Kind::Line) {
        const auto* line = static_cast<const LineEntity*>(entityToExtend);
        QPointF start = line->startPoint();
        QPointF end = line->endPoint();

        std::unique_ptr<Entity> newLine;
        if (extendEnd == TrimEnd::Start) {
            newLine = std::make_unique<LineEntity>(targetPoint, end);
        } else {
            newLine = std::make_unique<LineEntity>(start, targetPoint);
        }
        result.replacements.push_back(std::move(newLine));
        result.success = true;
    }
    else if (kind == Entity::Kind::Arc) {
        const auto* arc = static_cast<const ArcEntity*>(entityToExtend);
        QPointF center = arc->center();
        double radius = arc->radius();
        double startAngle = arc->startAngleDegrees();
        double sweepAngle = arc->sweepAngleDegrees();

        // Compute target angle
        double targetAngle = GeometryUtils::pointAngle(targetPoint, center);

        std::unique_ptr<Entity> newArc;
        if (extendEnd == TrimEnd::Start) {
            // Extend start point to target angle
            double newSweep = startAngle + sweepAngle - targetAngle;
            newArc = std::make_unique<ArcEntity>(center, radius, targetAngle, newSweep);
        } else {
            // Extend end point to target angle
            double newSweep = targetAngle - startAngle;
            newArc = std::make_unique<ArcEntity>(center, radius, startAngle, newSweep);
        }
        result.replacements.push_back(std::move(newArc));
        result.success = true;
    }
    else {
        result.errorMessage = "Cannot extend this entity type";
    }

    return result;
}

TrimModification::TrimResult TrimModification::trimCircleToArc(const CircleEntity* circle,
                                                               const QPointF& intersection,
                                                               const QPointF& clickPos)
{
    TrimResult result;

    if (!circle) {
        result.errorMessage = "Null circle";
        return result;
    }

    QPointF center = circle->center();
    double radius = circle->radius();

    // Compute angles
    double intAngle = GeometryUtils::pointAngle(intersection, center);
    double clickAngle = GeometryUtils::pointAngle(clickPos, center);

    // Determine the arc span based on click position
    // The arc should go from intersection toward the click position
    // along the shorter path

    double delta = GeometryUtils::normAngle(clickAngle - intAngle);

    // If delta > 180, take the shorter path (go the other way)
    if (delta > 180.0) {
        // Use the complement (go from click to intersection)
        std::swap(intAngle, clickAngle);
        delta = 360.0 - delta;
    }

    // Create arc from intersection angle with sweep = delta
    auto arc = std::make_unique<ArcEntity>(center, radius, intAngle, delta);
    arc->setId(circle->id());
    arc->setLayerId(circle->layerId());
    arc->setRotation(circle->rotation());
    arc->setEnabled(circle->isEnabled());

    result.replacements.push_back(std::move(arc));
    result.success = true;
    return result;
}

TrimModification::TrimResult TrimModification::trimLine(const LineEntity* line,
                                                         const QPointF& clickPos,
                                                         const std::vector<QPointF>& intersections)
{
    TrimResult result;

    if (!line || intersections.empty()) {
        result.errorMessage = "Invalid line or no intersections";
        qDebug() << "trimLine: Invalid - line=" << (line ? "valid" : "null")
                 << "intersections=" << intersections.size();
        return result;
    }

    QPointF start = line->startPoint();
    QPointF end = line->endPoint();
    qDebug() << "trimLine: Line from" << start << "to" << end;

    // For each intersection, determine which side to trim
    for (const auto& intersection : intersections) {
        TrimEnd trimEnd = line->getTrimPoint(clickPos, intersection);
        qDebug() << "trimLine: Intersection at" << intersection
                 << "trimEnd=" << (trimEnd == TrimEnd::Start ? "Start" : "End");

        if (trimEnd == TrimEnd::Start) {
            // Keep segment from intersection to end
            // (click is near start side, so trim start portion)
            qDebug() << "  Creating new line from intersection to end:" << intersection << "->" << end;
            auto newLine = std::make_unique<LineEntity>(intersection, end);
            newLine->setId(line->id());
            newLine->setLayerId(line->layerId());
            newLine->setRotation(line->rotation());
            newLine->setEnabled(line->isEnabled());
            result.replacements.push_back(std::move(newLine));
        }
        else if (trimEnd == TrimEnd::End) {
            // Keep segment from start to intersection
            // (click is near end side, so trim end portion)
            qDebug() << "  Creating new line from start to intersection:" << start << "->" << intersection;
            auto newLine = std::make_unique<LineEntity>(start, intersection);
            newLine->setId(line->id());
            newLine->setLayerId(line->layerId());
            newLine->setRotation(line->rotation());
            newLine->setEnabled(line->isEnabled());
            result.replacements.push_back(std::move(newLine));
        }
    }

    // If no valid replacements, entity is completely removed
    if (result.replacements.empty()) {
        // Entity will be deleted
        result.success = true;
    } else {
        // Only keep the first replacement (nearest intersection)
        if (result.replacements.size() > 1) {
            result.replacements.resize(1);
        }
        result.success = true;
    }

    return result;
}

TrimModification::TrimResult TrimModification::trimArc(const ArcEntity* arc,
                                                        const QPointF& clickPos,
                                                        const std::vector<QPointF>& intersections)
{
    TrimResult result;

    if (!arc || intersections.empty()) {
        result.errorMessage = "Invalid arc or no intersections";
        return result;
    }

    QPointF center = arc->center();
    double radius = arc->radius();
    double startAngle = arc->startAngleDegrees();
    double sweepAngle = arc->sweepAngleDegrees();
    double endAngle = startAngle + sweepAngle;

    // Compute intersection angles
    for (const auto& intersection : intersections) {
        double intAngle = GeometryUtils::pointAngle(intersection, center);

        TrimEnd trimEnd = arc->getTrimPoint(clickPos, intersection);

        if (trimEnd == TrimEnd::Start) {
            // Keep segment from intersection to end
            double newSweep = endAngle - intAngle;
            if (std::abs(newSweep) > 0.5) {
                auto newArc = std::make_unique<ArcEntity>(center, radius, intAngle, newSweep);
                newArc->setId(arc->id());
                newArc->setLayerId(arc->layerId());
                newArc->setRotation(arc->rotation());
                newArc->setEnabled(arc->isEnabled());
                result.replacements.push_back(std::move(newArc));
            }
        }
        else if (trimEnd == TrimEnd::End) {
            // Keep segment from start to intersection
            double newSweep = intAngle - startAngle;
            if (std::abs(newSweep) > 0.5) {
                auto newArc = std::make_unique<ArcEntity>(center, radius, startAngle, newSweep);
                newArc->setId(arc->id());
                newArc->setLayerId(arc->layerId());
                newArc->setRotation(arc->rotation());
                newArc->setEnabled(arc->isEnabled());
                result.replacements.push_back(std::move(newArc));
            }
        }
    }

    if (result.replacements.empty()) {
        result.success = true;  // Entity deleted
    } else {
        if (result.replacements.size() > 1) {
            result.replacements.resize(1);
        }
        result.success = true;
    }

    return result;
}