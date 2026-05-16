#include "RotateTool.h"

#include "../../../ui/panels/EntityTransform.h"

#include <QLineF>
#include <QtMath>

namespace {

bool isRotatableKind(Entity::Kind kind)
{
    switch (kind) {
        case Entity::Kind::Line:
        case Entity::Kind::Rectangle:
        case Entity::Kind::Circle:
        case Entity::Kind::Arc:
        case Entity::Kind::Polygon:
            return true;
        case Entity::Kind::Text:
        case Entity::Kind::LinearDimension:
        case Entity::Kind::AngularDimension:
        case Entity::Kind::RadiusDimension:
        case Entity::Kind::Spline:
            return false;
    }
    return false;
}

double angleDegrees(const QPointF& origin, const QPointF& point)
{
    return qRadiansToDegrees(std::atan2(point.y() - origin.y(), point.x() - origin.x()));
}

QRectF selectionBounds(const QVector<Entity*>& entities)
{
    QRectF bounds;
    for (const Entity* entity : entities) {
        if (!entity)
            continue;
        bounds = bounds.isNull() ? entity->boundingRect() : bounds.united(entity->boundingRect());
    }
    return bounds;
}

QPointF selectionReferencePoint(const QVector<Entity*>& entities)
{
    const QRectF bounds = selectionBounds(entities);
    if (!bounds.isValid() || bounds.isEmpty())
        return {};
    return QPointF(bounds.right(), bounds.center().y());
}

} // namespace

QString RotateTool::instructions() const
{
    if (document().selectedIds().isEmpty())
        return "Select entities to rotate.";
    if (!canRotateSelection())
        return "Rotate supports lines, rectangles, circles, arcs, and polylines only.";
    if (!m_hasPivot)
        return "Click pivot point for rotation.";
    return "Click destination angle to rotate selection.";
}

void RotateTool::mousePress(const QPointF& pos)
{
    if (!m_hasPivot) {
        beginRotate(pos);
        return;
    }

    updatePreview(pos);
    commitRotate();
}

void RotateTool::mouseMove(const QPointF& pos)
{
    if (!m_hasPivot)
        return;
    updatePreview(pos);
}

void RotateTool::mouseRelease(const QPointF&)
{
}

void RotateTool::cancel()
{
    reset();
}

void RotateTool::paintPreview(QPainter& painter) const
{
    if (!m_hasPivot)
        return;

    for (const auto& entity : m_previewEntities) {
        if (entity)
            entity->draw(painter);
    }
}

QSet<QUuid> RotateTool::snapExclusionIds() const
{
    return m_hasPivot ? document().selectedIds() : QSet<QUuid>{};
}

bool RotateTool::canRotateSelection() const
{
    const QVector<Entity*> selected = document().selectedEntities();
    if (selected.isEmpty())
        return false;

    for (const Entity* entity : selected) {
        if (!entity || !isRotatableKind(entity->kind()))
            return false;
    }
    return true;
}

void RotateTool::beginRotate(const QPointF& pos)
{
    if (!canRotateSelection())
        return;

    const QVector<int> selectedIndices = document().selectedIndices();
    const QVector<Entity*> selectedEntities = document().selectedEntities();
    if (selectedIndices.isEmpty() || selectedEntities.isEmpty())
        return;

    reset();
    m_hasPivot = true;
    m_pivotPoint = pos;
    m_currentPoint = pos;
    m_referencePoint = selectionReferencePoint(selectedEntities);
    if (QLineF(m_pivotPoint, m_referencePoint).length() <= 1e-9)
        m_referencePoint = QPointF(m_pivotPoint.x() + 1.0, m_pivotPoint.y());

    m_selectedIndices.reserve(selectedIndices.size());
    m_originalEntities.reserve(selectedIndices.size());
    for (int index : selectedIndices) {
        Entity* entity = document().entityAt(index);
        if (!entity)
            continue;
        m_selectedIndices.push_back(index);
        m_originalEntities.push_back(entity->clone());
    }

    updatePreview(pos);
}

void RotateTool::updatePreview(const QPointF& pos)
{
    m_currentPoint = pos;
    m_previewEntities.clear();
    m_previewEntities.reserve(m_originalEntities.size());

    const double deltaAngle = angleDegrees(m_pivotPoint, pos) - angleDegrees(m_pivotPoint, m_referencePoint);
    for (const auto& entity : m_originalEntities) {
        if (!entity) {
            m_previewEntities.push_back(nullptr);
            continue;
        }
        m_previewEntities.push_back(EntityTransform::makeRotatedEntity(entity.get(), m_pivotPoint, deltaAngle));
    }
}

void RotateTool::commitRotate()
{
    auto command = std::make_unique<CompositeCommand>();
    for (std::size_t i = 0; i < m_selectedIndices.size(); ++i) {
        if (!m_originalEntities[i] || !m_previewEntities[i])
            continue;
        command->add(std::make_unique<UpdateEntityCommand>(
            m_selectedIndices[i], std::move(m_originalEntities[i]), std::move(m_previewEntities[i])));
    }

    if (!command->empty())
        commandStack().execute(std::move(command));

    reset();
}

void RotateTool::reset()
{
    m_hasPivot = false;
    m_pivotPoint = {};
    m_currentPoint = {};
    m_referencePoint = {};
    m_selectedIndices.clear();
    m_originalEntities.clear();
    m_previewEntities.clear();
}
