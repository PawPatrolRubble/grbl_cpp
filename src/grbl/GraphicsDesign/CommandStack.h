#pragma once

#include <memory>
#include <vector>
#include <QUuid>
#include "Layer.h"
#include <QList>
#include <QHash>

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
    QUuid m_addedId;
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
    QUuid m_targetId;
};

/// Command to update an entity (for handle edits and translations).
class UpdateEntityCommand : public Command
{
public:
    UpdateEntityCommand(int index, std::unique_ptr<Entity> before, std::unique_ptr<Entity> after,
                        bool preservePlotMetadata = true);

    void execute(Document& document) override;
    void undo(Document& document) override;

private:
    int m_index = -1;
    std::unique_ptr<Entity> m_before;
    std::unique_ptr<Entity> m_after;
    QUuid m_targetId;
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

/// Command grouping multiple child commands into one undo/redo step.
class CompositeCommand : public Command
{
public:
    void add(std::unique_ptr<Command> command);
    bool empty() const { return m_commands.empty(); }

    void execute(Document& document) override;
    void undo(Document& document) override;

private:
    std::vector<std::unique_ptr<Command>> m_commands;
};

/// Undoable change of the document's fallback pen.
class SetDefaultToolCommand : public Command
{
public:
    SetDefaultToolCommand(const QUuid& before, const QUuid& after);
    void execute(Document& document) override;
    void undo(Document& document) override;

private:
    QUuid m_before;
    QUuid m_after;
};

struct DocumentLayerState
{
    QList<Layer> layers;
    QUuid activeLayerId;
    QHash<QUuid, QUuid> entityLayers;
};

class EditLayersCommand : public Command
{
public:
    EditLayersCommand(DocumentLayerState before, DocumentLayerState after);
    void execute(Document& document) override;
    void undo(Document& document) override;
private:
    DocumentLayerState m_before;
    DocumentLayerState m_after;
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
    void clear();

private:
    Document& m_document;
    std::vector<std::unique_ptr<Command>> m_undoStack;
    std::vector<std::unique_ptr<Command>> m_redoStack;
};
