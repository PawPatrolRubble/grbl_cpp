#pragma once

#include "Entity.h"
#include "CommandStack.h"
#include "Layer.h"

#include <QObject>
#include <QList>
#include <QSet>
#include <QJsonDocument>
#include <memory>
#include <vector>
#include <optional>
#include <QVector>

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
    void beginBatch();
    void endBatch();
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
    void selectAll();
    void setSelected(const QUuid& id, bool selected);
    QSet<QUuid> selectedIds() const { return m_selectedIds; }
    QVector<int> selectedIndices() const;
    QVector<Entity*> selectedEntities() const;
    QUuid primarySelectedId() const { return m_primarySelectedId; }
    Entity* selectedEntity() const;

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

    // Layer management
    int layerCount() const { return m_layers.size(); }
    const Layer& layerAt(int index) const { return m_layers[index]; }
    Layer& layerAt(int index) { return m_layers[index]; }
    int indexOfLayer(const QUuid& id) const;
    QUuid activeLayerId() const { return m_activeLayerId; }
    void setActiveLayerId(const QUuid& id);
    void addLayer(const QString& name);
    void removeLayerAt(int index);   // moves entities on that layer to layer 0
    void setLayerName(int index, const QString& name);
    void setLayerVisible(int index, bool visible);
    void setLayerLocked(int index, bool locked);
    void setLayerColor(int index, const QColor& color);
    QColor entityLayerColor(const Entity* e) const;
    bool isEntityVisible(const Entity* e) const;
    bool isEntityLocked(const Entity* e) const;

signals:
    void changed();
    void selectionChanged();
    void layersChanged();

private:
    void normalizeSelectionAfterRemoval(int removedIndex);
    void ensurePrimarySelection();

    std::vector<std::unique_ptr<Entity>> m_entities;
    int m_batchDepth = 0;
    QSet<QUuid> m_selectedIds;
    QUuid m_primarySelectedId;
    CommandStack* m_commandStack;
    double m_worktableWidth = 300.0;
    QList<Layer> m_layers;
    QUuid m_activeLayerId;

public:
    double worktableWidth() const { return m_worktableWidth; }
    void setWorktableWidth(double w) { m_worktableWidth = w; }
};