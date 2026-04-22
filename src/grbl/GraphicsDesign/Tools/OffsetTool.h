#pragma once

#include "Tool.h"
#include <QPointF>
#include <memory>

class Entity;

/// Offset / parallel-copy tool.
/// Hover over an entity to highlight it.  Click to create an offset copy on
/// the side of the cursor.  Works with Line, Circle, Arc, and Rectangle.
/// The original entity is preserved; a new copy is added at the offset distance.
class OffsetTool final : public Tool
{
public:
    OffsetTool(Document& doc, CommandStack& cs);

    double offsetDistance() const    { return m_distance; }
    void   setOffsetDistance(double d);

    // Tool interface
    QString name()         const override;
    QString instructions() const override;
    void mousePress  (const QPointF& pos) override;
    void mouseMove   (const QPointF& pos) override;
    void mouseRelease(const QPointF& pos) override;
    void cancel()          override;
    void paintPreview(QPainter& painter) const override;
    bool isActive()        const override;

private:
    double m_distance = 5.0;   // default 5 mm

    int    m_hoverIdx  = -1;       // entity index under cursor
    QPointF m_mousePos;            // current cursor in world coordinates

    // Preview entity (computed on hover, applied on click)
    bool   m_previewValid = false;
    std::unique_ptr<Entity> m_previewEntity;   // transient — rebuilt each move

    int  nearestEntity(const QPointF& worldPos) const;
    std::unique_ptr<Entity> makeOffset(int idx, const QPointF& sidePos) const;
};
