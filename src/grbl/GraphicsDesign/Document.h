#pragma once

#include "Entity.h"
#include "CommandStack.h"

#include <QObject>
#include <QSet>
#include <QJsonDocument>
#include <memory>
#include <vector>
#include <optional>

/// Manages a collection of entities with selection and undo/redo support.
/// Merges tinyCad's Document with grbl_cpp's ShapeModel.
class Document : public QObject
{
    Q_OBJECT

public:
    explicit Document(QObject* parent = nullptr);
    ~Document();

    // Entity management
    void addEntity(std::unique_ptr<Entity> entity);
    void insertEntity(int index, std::unique_ptr<Entity> entity);
    void replaceEntity(int index, std::unique_ptr<Entity> entity);
    std::unique_ptr<Entity> takeLastEntity();
    std::unique_ptr<Entity> takeEntityAt(int index);
    void restoreEntities(std::vector<std::unique_ptr<Entity>> entities);
    void clear();

    const std::vector<std::unique_ptr<Entity>>& entities() const { return m_entities; }
    Entity* entityAt(int index) const;
    int entityCount() const { return static_cast<int>(m_entities.size()); }
    int indexOf(const QUuid& id) const;

    // Selection (multi-select from ShapeModel)
    void select(const QUuid& id);
    void deselect(const QUuid& id);
    void clearSelection();
    void setSelected(const QUuid& id, bool selected);
    QSet<QUuid> selectedIds() const { return m_selectedIds; }
    Entity* selectedEntity() const;  // Returns first selected or null

    // Selection by position (from tinyCad)
    void selectNearest(const QPointF& worldPos, double tolerance);

    // G-code generation
    QStringList generateGcode(double feedRateXY, double plungeRateZ,
                              double safeZ, double cutZ) const;

    // Serialization
    QJsonDocument toJson() const;
    bool loadFromJson(const QJsonDocument& doc);

    // Undo/redo
    CommandStack* commandStack() { return m_commandStack; }
    bool canUndo() const;
    bool canRedo() const;
    void undo();
    void redo();

signals:
    void changed();
    void selectionChanged();

private:
    void normalizeSelectionAfterRemoval(int removedIndex);

    std::vector<std::unique_ptr<Entity>> m_entities;
    QSet<QUuid> m_selectedIds;
    CommandStack* m_commandStack;
};