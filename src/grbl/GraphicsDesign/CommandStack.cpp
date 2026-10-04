#include "CommandStack.h"
#include "Document.h"

#include <algorithm>

AddEntityCommand::AddEntityCommand(std::unique_ptr<Entity> entity)
    : m_entity(std::move(entity))
{
}

void AddEntityCommand::execute(Document& document)
{
    if (!m_entity) return;
    document.addEntity(std::move(m_entity));
    m_addedId = document.entityAt(document.entityCount() - 1)->id();
}

void AddEntityCommand::undo(Document& document)
{
    m_entity = document.takeEntityAt(document.indexOf(m_addedId));
}

DeleteEntityCommand::DeleteEntityCommand(int index)
    : m_index(index)
{
}

void DeleteEntityCommand::execute(Document& document)
{
    if (m_targetId.isNull()) {
        const auto* entity = document.entityAt(m_index);
        if (!entity) return;
        m_targetId = entity->id();
    }
    const int index = document.indexOf(m_targetId);
    if (index < 0) return;
    m_index = index;
    m_entity = document.takeEntityAt(index);
}

void DeleteEntityCommand::undo(Document& document)
{
    if (m_entity) document.insertEntity(m_index, std::move(m_entity));
}

UpdateEntityCommand::UpdateEntityCommand(int index, std::unique_ptr<Entity> before,
                                       std::unique_ptr<Entity> after, bool preservePlotMetadata)
    : m_index(index)
    , m_before(std::move(before))
    , m_after(std::move(after))
{
    if (m_before) m_targetId = m_before->id();
    if (preservePlotMetadata && m_before && m_after)
        m_after->copyPlotMetadataFrom(*m_before);
}

void UpdateEntityCommand::execute(Document& document)
{
    if (!m_after) return;
    document.replaceEntity(document.indexOf(m_targetId), m_after->clone());
}

void UpdateEntityCommand::undo(Document& document)
{
    if (!m_before) return;
    document.replaceEntity(document.indexOf(m_targetId), m_before->clone());
}

void ClearDocumentCommand::execute(Document& document)
{
    m_clearedEntities.clear();
    m_clearedEntities.reserve(static_cast<std::size_t>(document.entityCount()));

    while (document.entityCount() > 0) {
        auto entity = document.takeLastEntity();
        if (entity) {
            m_clearedEntities.push_back(std::move(entity));
        }
    }

    std::reverse(m_clearedEntities.begin(), m_clearedEntities.end());
}

void ClearDocumentCommand::undo(Document& document)
{
    document.restoreEntities(std::move(m_clearedEntities));
}

void CompositeCommand::add(std::unique_ptr<Command> command)
{
    if (command)
        m_commands.push_back(std::move(command));
}

void CompositeCommand::execute(Document& document)
{
    document.beginBatch();
    for (auto& command : m_commands) {
        if (command)
            command->execute(document);
    }
    document.endBatch();
}

void CompositeCommand::undo(Document& document)
{
    document.beginBatch();
    for (auto it = m_commands.rbegin(); it != m_commands.rend(); ++it) {
        if (*it)
            (*it)->undo(document);
    }
    document.endBatch();
}

SetDefaultToolCommand::SetDefaultToolCommand(const QUuid& before, const QUuid& after)
    : m_before(before), m_after(after)
{
}

void SetDefaultToolCommand::execute(Document& document) { document.setDefaultToolId(m_after); }
void SetDefaultToolCommand::undo(Document& document) { document.setDefaultToolId(m_before); }

EditLayersCommand::EditLayersCommand(DocumentLayerState before, DocumentLayerState after)
    : m_before(std::move(before)), m_after(std::move(after)) {}
void EditLayersCommand::execute(Document& document) { document.restoreLayerState(m_after); }
void EditLayersCommand::undo(Document& document) { document.restoreLayerState(m_before); }

void CommandStack::clear()
{
    m_undoStack.clear();
    m_redoStack.clear();
    emit m_document.historyChanged();
}

CommandStack::CommandStack(Document& document)
    : m_document(document)
{
}

void CommandStack::execute(std::unique_ptr<Command> command)
{
    if (!command) return;

    command->execute(m_document);
    m_undoStack.push_back(std::move(command));
    m_redoStack.clear();
    emit m_document.historyChanged();
}

bool CommandStack::canUndo() const
{
    return !m_undoStack.empty();
}

bool CommandStack::canRedo() const
{
    return !m_redoStack.empty();
}

void CommandStack::undo()
{
    if (m_undoStack.empty()) return;

    auto command = std::move(m_undoStack.back());
    m_undoStack.pop_back();
    command->undo(m_document);
    m_redoStack.push_back(std::move(command));
    emit m_document.historyChanged();
}

void CommandStack::redo()
{
    if (m_redoStack.empty()) return;

    auto command = std::move(m_redoStack.back());
    m_redoStack.pop_back();
    command->execute(m_document);
    m_undoStack.push_back(std::move(command));
    emit m_document.historyChanged();
}
