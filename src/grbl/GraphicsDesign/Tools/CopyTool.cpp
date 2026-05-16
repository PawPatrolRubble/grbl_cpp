#include "CopyTool.h"

#include <QUuid>

QString CopyTool::instructions() const
{
    if (document().selectedIds().isEmpty())
        return "Select entities to copy.";
    if (!m_hasBasePoint)
        return "Click base point for copy.";
    return "Click destination point to place copies.";
}

void CopyTool::mousePress(const QPointF& pos)
{
    if (!m_hasBasePoint) {
        beginCopy(pos);
        return;
    }

    updatePreview(pos);
    commitCopy();
}

void CopyTool::mouseMove(const QPointF& pos)
{
    if (!m_hasBasePoint)
        return;
    updatePreview(pos);
}

void CopyTool::mouseRelease(const QPointF&)
{
}

void CopyTool::cancel()
{
    reset();
}

void CopyTool::paintPreview(QPainter& painter) const
{
    if (!m_hasBasePoint)
        return;

    for (const auto& entity : m_previewEntities) {
        if (entity)
            entity->draw(painter);
    }
}

QSet<QUuid> CopyTool::snapExclusionIds() const
{
    return m_hasBasePoint ? document().selectedIds() : QSet<QUuid>{};
}

void CopyTool::beginCopy(const QPointF& pos)
{
    const QVector<Entity*> selected = document().selectedEntities();
    if (selected.isEmpty())
        return;

    reset();
    m_hasBasePoint = true;
    m_basePoint = pos;
    m_currentPoint = pos;
    m_originalEntities.reserve(selected.size());

    for (const Entity* entity : selected) {
        if (entity)
            m_originalEntities.push_back(entity->clone());
    }

    updatePreview(pos);
}

void CopyTool::updatePreview(const QPointF& pos)
{
    m_currentPoint = pos;
    m_previewEntities.clear();
    m_previewEntities.reserve(m_originalEntities.size());

    const QPointF delta = pos - m_basePoint;
    for (const auto& entity : m_originalEntities) {
        if (!entity) {
            m_previewEntities.push_back(nullptr);
            continue;
        }
        auto preview = entity->clone();
        preview->translate(delta);
        m_previewEntities.push_back(std::move(preview));
    }
}

void CopyTool::commitCopy()
{
    auto command = std::make_unique<CompositeCommand>();
    QVector<QUuid> createdIds;
    createdIds.reserve(static_cast<qsizetype>(m_previewEntities.size()));

    for (const auto& entity : m_previewEntities) {
        if (!entity)
            continue;
        auto created = entity->clone();
        created->setId(QUuid::createUuid());
        createdIds.push_back(created->id());
        command->add(std::make_unique<AddEntityCommand>(std::move(created)));
    }

    if (!command->empty()) {
        commandStack().execute(std::move(command));
        document().clearSelection();
        for (const QUuid& id : createdIds)
            document().select(id);
    }

    reset();
}

void CopyTool::reset()
{
    m_hasBasePoint = false;
    m_basePoint = {};
    m_currentPoint = {};
    m_originalEntities.clear();
    m_previewEntities.clear();
}
