#include "MoveTool.h"

#include <QLineF>

namespace {
constexpr double kMinMoveDistance = 1e-9;
}

QString MoveTool::instructions() const
{
    if (document().selectedIds().isEmpty())
        return "Select entities to move.";
    if (!m_hasBasePoint)
        return "Click base point for move.";
    return "Click destination point to move selection.";
}

void MoveTool::mousePress(const QPointF& pos)
{
    if (!m_hasBasePoint) {
        beginMove(pos);
        return;
    }

    updatePreview(pos);
    commitMove();
}

void MoveTool::mouseMove(const QPointF& pos)
{
    if (!m_hasBasePoint)
        return;
    updatePreview(pos);
}

void MoveTool::mouseRelease(const QPointF&)
{
}

void MoveTool::cancel()
{
    reset();
}

void MoveTool::paintPreview(QPainter& painter) const
{
    if (!m_hasBasePoint)
        return;

    for (const auto& entity : m_previewEntities) {
        if (entity)
            entity->draw(painter);
    }
}

QSet<QUuid> MoveTool::snapExclusionIds() const
{
    return m_hasBasePoint ? document().selectedIds() : QSet<QUuid>{};
}

void MoveTool::beginMove(const QPointF& pos)
{
    const QVector<int> selectedIndices = document().selectedIndices();
    if (selectedIndices.isEmpty())
        return;

    reset();
    m_hasBasePoint = true;
    m_basePoint = pos;
    m_currentPoint = pos;
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

void MoveTool::updatePreview(const QPointF& pos)
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

void MoveTool::commitMove()
{
    if (QLineF(m_basePoint, m_currentPoint).length() <= kMinMoveDistance) {
        reset();
        return;
    }

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

void MoveTool::reset()
{
    m_hasBasePoint = false;
    m_basePoint = {};
    m_currentPoint = {};
    m_selectedIndices.clear();
    m_originalEntities.clear();
    m_previewEntities.clear();
}
