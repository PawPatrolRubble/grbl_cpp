#include "CommandStack.h"
#include "Document.h"

#include <algorithm>

AddEntityCommand::AddEntityCommand(std::unique_ptr<Entity> entity)
    : m_entity(std::move(entity))
{
}

void AddEntityCommand::execute(Document& document)
{
    document.addEntity(std::move(m_entity));
}

void AddEntityCommand::undo(Document& document)
{
    m_entity = document.takeLastEntity();
}

DeleteEntityCommand::DeleteEntityCommand(int index)
    : m_index(index)
{
}

void DeleteEntityCommand::execute(Document& document)
{
    m_entity = document.takeEntityAt(m_index);
}

void DeleteEntityCommand::undo(Document& document)
{
    document.insertEntity(m_index, std::move(m_entity));
}

UpdateEntityCommand::UpdateEntityCommand(int index, std::unique_ptr<Entity> before, std::unique_ptr<Entity> after)
    : m_index(index)
    , m_before(std::move(before))
    , m_after(std::move(after))
{
}

void UpdateEntityCommand::execute(Document& document)
{
    if (!m_after) return;
    document.replaceEntity(m_index, m_after->clone());
}

void UpdateEntityCommand::undo(Document& document)
{
    if (!m_before) return;
    document.replaceEntity(m_index, m_before->clone());
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
}

void CommandStack::redo()
{
    if (m_redoStack.empty()) return;

    auto command = std::move(m_redoStack.back());
    m_redoStack.pop_back();
    command->execute(m_document);
    m_undoStack.push_back(std::move(command));
}