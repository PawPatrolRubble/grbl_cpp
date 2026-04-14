#pragma once

#include <memory>
#include <vector>

class Document;
class Entity;

/// Base class for undoable commands.
class Command
{
public:
    virtual ~Command() = default;

    virtual void execute(Document& document) = 0;
    virtual void undo(Document& document) = 0;
};

/// Command to add a new entity to the document.
class AddEntityCommand : public Command
{
public:
    explicit AddEntityCommand(std::unique_ptr<Entity> entity);

    void execute(Document& document) override;
    void undo(Document& document) override;

private:
    std::unique_ptr<Entity> m_entity;
};

/// Command to delete an entity from the document.
class DeleteEntityCommand : public Command
{
public:
    explicit DeleteEntityCommand(int index);

    void execute(Document& document) override;
    void undo(Document& document) override;

private:
    int m_index = -1;
    std::unique_ptr<Entity> m_entity;
};

/// Command to update an entity (for handle edits and translations).
class UpdateEntityCommand : public Command
{
public:
    UpdateEntityCommand(int index, std::unique_ptr<Entity> before, std::unique_ptr<Entity> after);

    void execute(Document& document) override;
    void undo(Document& document) override;

private:
    int m_index = -1;
    std::unique_ptr<Entity> m_before;
    std::unique_ptr<Entity> m_after;
};

/// Command to clear all entities from the document.
class ClearDocumentCommand : public Command
{
public:
    void execute(Document& document) override;
    void undo(Document& document) override;

private:
    std::vector<std::unique_ptr<Entity>> m_clearedEntities;
};

/// Undo/redo stack managing command history.
class CommandStack
{
public:
    explicit CommandStack(Document& document);

    void execute(std::unique_ptr<Command> command);
    bool canUndo() const;
    bool canRedo() const;
    void undo();
    void redo();

private:
    Document& m_document;
    std::vector<std::unique_ptr<Command>> m_undoStack;
    std::vector<std::unique_ptr<Command>> m_redoStack;
};