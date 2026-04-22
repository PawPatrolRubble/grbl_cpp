#include "EntityClipboard.h"

void EntityClipboard::copy()
{
    const Entity* selected = m_document.selectedEntity();
    if (!selected) return;

    m_copiedEntity = selected->clone();
    m_pasteSerial = 0;
}

void EntityClipboard::paste()
{
    if (!m_copiedEntity) return;

    auto entity = m_copiedEntity->clone();
    ++m_pasteSerial;
    entity->translate(shortcutOffsetForStep(m_pasteSerial));
    addEntity(std::move(entity));
}

void EntityClipboard::duplicate()
{
    const Entity* selected = m_document.selectedEntity();
    if (!selected) return;

    auto entity = selected->clone();
    entity->translate(shortcutOffsetForStep(1));
    addEntity(std::move(entity));

    // Also update clipboard so subsequent pastes work
    m_copiedEntity = selected->clone();
    m_pasteSerial = 1;
}

void EntityClipboard::addEntity(std::unique_ptr<Entity> entity)
{
    if (!entity) return;

    const int entityCountBefore = m_document.entityCount();
    m_commandStack.execute(std::make_unique<AddEntityCommand>(std::move(entity)));

    if (m_document.entityCount() > entityCountBefore) {
        m_document.clearSelection();
        if (Entity* created = m_document.entityAt(entityCountBefore)) {
            m_document.select(created->id());
        }
    }
}
