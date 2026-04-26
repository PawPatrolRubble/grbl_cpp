#pragma once

#include "Entity.h"
#include "GeometryUtils.h"

#include <vector>
#include <memory>

class Document;
class CommandStack;

/// Separates trim business logic from TrimTool UI.
/// Follows LibreCAD's RS_Modification::trim() pattern.
class TrimModification
{
public:
    /// Trim result: the replacement entities after trimming
    struct TrimResult {
        std::vector<std::unique_ptr<Entity>> replacements;
        bool success = false;
        QString errorMessage;
    };

    explicit TrimModification(Document& doc);

    /// Quick trim: trim entity against all visible entities in the document.
    /// Automatically finds nearest intersection to click position.
    /// @param entityToTrim The entity to trim (by pointer)
    /// @param entityIndex Index of the entity in document (to exclude from intersection search)
    /// @param clickPos Click position to determine which portion to remove
    /// @return Result containing replacement entities
    TrimResult quickTrim(const Entity* entityToTrim, int entityIndex, const QPointF& clickPos);

    /// Trim entity at a specific intersection point.
    /// @param entityToTrim The entity to trim
    /// @param intersection The intersection point where trim occurs
    /// @param clickPos Click position to determine which side to keep
    /// @return Result containing replacement entities
    TrimResult trimAtIntersection(const Entity* entityToTrim,
                                   const QPointF& intersection,
                                   const QPointF& clickPos);

    /// Extend an entity to an intersection point.
    /// @param entityToExtend The entity to extend
    /// @param targetPoint The point to extend to
    /// @param extendEnd Which end to extend (TrimEnd::Start or TrimEnd::End)
    /// @return Result containing the extended entity
    TrimResult extendEntity(const Entity* entityToExtend,
                            const QPointF& targetPoint,
                            TrimEnd extendEnd);

    /// Convert a circle to an arc based on trimming.
    /// @param circle The circle entity to convert
    /// @param intersection The intersection point where trim occurs
    /// @param clickPos Click position to determine arc span
    /// @return Result containing the arc entity
    TrimResult trimCircleToArc(const CircleEntity* circle,
                               const QPointF& intersection,
                               const QPointF& clickPos);

private:
    Document& m_document;

    // Internal trim implementations per entity type
    TrimResult trimLine(const LineEntity* line, const QPointF& clickPos,
                        const std::vector<QPointF>& intersections);
    TrimResult trimArc(const ArcEntity* arc, const QPointF& clickPos,
                       const std::vector<QPointF>& intersections);
};