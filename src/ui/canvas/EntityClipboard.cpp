#include "EntityClipboard.h"

void EntityClipboard::copy()
{
    m_copiedEntities.clear();
    const QVector<Entity*> selected = m_document.selectedEntities();
    if (selected.isEmpty()) return;

    m_copiedEntities.reserve(selected.size());
    for (const Entity* entity : selected) {
        if (entity)
            m_copiedEntities.push_back(entity->clone());
    }
    m_pasteSerial = 0;
}

void EntityClipboard::paste()
{
    if (m_copiedEntities.empty()) return;

    ++m_pasteSerial;
    std::vector<std::unique_ptr<Entity>> entities;
    entities.reserve(m_copiedEntities.size());
    for (const auto& copied : m_copiedEntities) {
        if (!copied) continue;
        auto entity = copied->clone();
        entity->translate(shortcutOffsetForStep(m_pasteSerial));
        entities.push_back(std::move(entity));
    }
    addEntities(std::move(entities));
}

void EntityClipboard::duplicate()
{
    const QVector<Entity*> selected = m_document.selectedEntities();
    if (selected.isEmpty()) return;

    std::vector<std::unique_ptr<Entity>> entities;
    entities.reserve(selected.size());
    m_copiedEntities.clear();
    m_copiedEntities.reserve(selected.size());
    for (const Entity* entity : selected) {
        if (!entity) continue;
        auto duplicate = entity->clone();
        duplicate->translate(shortcutOffsetForStep(1));
        entities.push_back(std::move(duplicate));
        m_copiedEntities.push_back(entity->clone());
    }

    m_pasteSerial = 1;
    addEntities(std::move(entities));
}

void EntityClipboard::addEntities(std::vector<std::unique_ptr<Entity>> entities)
{
    if (entities.empty()) return;

    const int entityCountBefore = m_document.entityCount();
    auto command = std::make_unique<CompositeCommand>();
    for (auto& entity : entities) {
        if (entity)
            command->add(std::make_unique<AddEntityCommand>(std::move(entity)));
    }

    if (command->empty()) return;

    m_commandStack.execute(std::move(command));

    if (m_document.entityCount() <= entityCountBefore) return;

    m_document.clearSelection();
    for (int index = entityCountBefore; index < m_document.entityCount(); ++index) {
        if (Entity* created = m_document.entityAt(index))
            m_document.select(created->id());
    }
}
