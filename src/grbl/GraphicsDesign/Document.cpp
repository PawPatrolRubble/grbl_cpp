#include "Document.h"
#include "../GcodeParser.h"
#include "GroupEntity.h"

#include <QJsonArray>
#include <algorithm>

namespace {

void collectToolIds(const Entity* entity, const Document& document, QSet<QUuid>& ids,
                    bool resolveDefault, bool plottingOnly)
{
    if (!entity || !entity->isDrawable()) return;
    if (plottingOnly && (!entity->isEnabled() || !document.isEntityVisible(entity))) return;
    if (const auto* group = dynamic_cast<const GroupEntity*>(entity)) {
        for (const auto& child : group->children())
            collectToolIds(child.get(), document, ids, resolveDefault, plottingOnly);
        return;
    }
    ids.insert(resolveDefault && entity->toolId().isNull()
                   ? document.defaultToolId() : entity->toolId());
}

bool assignTool(Entity* entity, const Document& document, const QUuid& toolId)
{
    if (!entity || !entity->isDrawable() || document.isEntityLocked(entity)) return false;
    if (auto* group = dynamic_cast<GroupEntity*>(entity)) {
        bool changed = false;
        for (const auto& child : group->children())
            changed = assignTool(child.get(), document, toolId) || changed;
        return changed;
    }
    if (entity->toolId() == toolId) return false;
    entity->setToolId(toolId);
    return true;
}

bool validToolReferences(const QJsonObject& json)
{
    for (const QString& field : {QStringLiteral("toolId"), QStringLiteral("defaultToolId")}) {
        if (!json.contains(field)) continue;
        if (!json[field].isString()) return false;
        const QString value = json[field].toString();
        if (!value.isEmpty() && QUuid::fromString(value).isNull()) return false;
    }
    for (const QString& field : {QStringLiteral("entities"), QStringLiteral("children")}) {
        for (const auto& child : json[field].toArray()) {
            if (!validToolReferences(child.toObject())) return false;
        }
    }
    return true;
}

} // namespace

Document::Document(QObject* parent)
    : QObject(parent)
    , m_commandStack(new CommandStack(*this))
{
    Layer def = Layer::makeDefault();
    m_activeLayerId = def.id;
    m_layers.append(std::move(def));
}

Document::~Document()
{
    m_entities.clear();
    delete m_commandStack;
}

void Document::addEntity(std::unique_ptr<Entity> entity)
{
    if (!entity) return;
    if (indexOf(entity->id()) >= 0) entity->setId(QUuid::createUuid());
    if (entity->layerId().isNull())
        entity->setLayerId(m_activeLayerId);
    m_entities.push_back(std::move(entity));
    if (m_batchDepth == 0) emit changed();
}

void Document::importEntities(std::vector<std::unique_ptr<Entity>> entities)
{
    auto command = std::make_unique<CompositeCommand>();
    for (auto& entity : entities)
        if (entity) command->add(std::make_unique<AddEntityCommand>(std::move(entity)));
    if (!command->empty()) m_commandStack->execute(std::move(command));
}

void Document::beginBatch() { ++m_batchDepth; }
void Document::endBatch() { if (--m_batchDepth <= 0) { m_batchDepth = 0; emit changed(); } }

void Document::insertEntity(int index, std::unique_ptr<Entity> entity)
{
    if (!entity) return;
    if (index < 0) index = 0;
    if (index > entityCount()) index = entityCount();
    if (entity->layerId().isNull())
        entity->setLayerId(m_activeLayerId);
    m_entities.insert(m_entities.begin() + index, std::move(entity));
    if (m_batchDepth == 0) emit changed();
}

void Document::replaceEntity(int index, std::unique_ptr<Entity> entity)
{
    if (index >= 0 && index < entityCount()) {
        // Preserve entity ID so selection stays valid.
        // Preserve layer only if the replacement doesn't carry an explicit assignment.
        entity->setId(m_entities[index]->id());
        if (entity->layerId().isNull())
            entity->setLayerId(m_entities[index]->layerId());
        m_entities[index] = std::move(entity);
        if (m_batchDepth == 0) emit changed();
    }
}

std::unique_ptr<Entity> Document::takeLastEntity()
{
    if (m_entities.empty()) return nullptr;
    auto entity = std::move(m_entities.back());
    m_entities.pop_back();
    normalizeSelectionAfterRemoval(entityCount());
    return entity;
}

std::unique_ptr<Entity> Document::takeEntityAt(int index)
{
    if (index < 0 || index >= entityCount()) return nullptr;
    auto entity = std::move(m_entities[index]);
    m_entities.erase(m_entities.begin() + index);
    normalizeSelectionAfterRemoval(index);
    if (m_batchDepth == 0) emit changed();
    return entity;
}

void Document::restoreEntities(std::vector<std::unique_ptr<Entity>> entities)
{
    m_entities = std::move(entities);
    emit changed();
}

void Document::clear()
{
    m_entities.clear();
    m_selectedIds.clear();
    m_primarySelectedId = {};
    m_defaultToolId = {};
    m_toolNameHints.clear();
    emit changed();
    emit selectionChanged();
}

Entity* Document::entityAt(int index) const
{
    if (index < 0 || index >= entityCount()) return nullptr;
    return m_entities[index].get();
}

int Document::indexOf(const QUuid& id) const
{
    for (int i = 0; i < entityCount(); ++i) {
        if (m_entities[i]->id() == id) return i;
    }
    return -1;
}

void Document::select(const QUuid& id)
{
    if (indexOf(id) < 0) return;

    const bool inserted = !m_selectedIds.contains(id);
    m_selectedIds.insert(id);
    const bool primaryChanged = (m_primarySelectedId != id);
    m_primarySelectedId = id;

    if (inserted || primaryChanged) {
        emit selectionChanged();
    }
}

void Document::deselect(const QUuid& id)
{
    if (m_selectedIds.remove(id)) {
        if (m_primarySelectedId == id)
            ensurePrimarySelection();
        emit selectionChanged();
    }
}

void Document::clearSelection()
{
    if (!m_selectedIds.isEmpty() || !m_primarySelectedId.isNull()) {
        m_selectedIds.clear();
        m_primarySelectedId = QUuid();
        emit selectionChanged();
    }
}

void Document::selectAll()
{
    m_selectedIds.clear();
    m_primarySelectedId = QUuid();
    for (const auto& e : m_entities) {
        if (e && e->isEnabled()) {
            m_selectedIds.insert(e->id());
            if (m_primarySelectedId.isNull())
                m_primarySelectedId = e->id();
        }
    }
    emit selectionChanged();
}

void Document::setSelected(const QUuid& id, bool selected)
{
    if (selected) select(id);
    else deselect(id);
}

QVector<int> Document::selectedIndices() const
{
    QVector<int> indices;
    indices.reserve(m_selectedIds.size());
    if (!m_primarySelectedId.isNull() && m_selectedIds.contains(m_primarySelectedId)) {
        const int primaryIndex = indexOf(m_primarySelectedId);
        if (primaryIndex >= 0)
            indices.push_back(primaryIndex);
    }

    for (int i = 0; i < entityCount(); ++i) {
        if (m_entities[i] && m_selectedIds.contains(m_entities[i]->id()) && m_entities[i]->id() != m_primarySelectedId)
            indices.push_back(i);
    }
    return indices;
}

QVector<Entity*> Document::selectedEntities() const
{
    QVector<Entity*> entities;
    const QVector<int> indices = selectedIndices();
    entities.reserve(indices.size());
    for (int index : indices)
        entities.push_back(entityAt(index));
    return entities;
}

Entity* Document::selectedEntity() const
{
    if (m_selectedIds.isEmpty()) return nullptr;
    if (!m_primarySelectedId.isNull()) {
        const int primaryIndex = indexOf(m_primarySelectedId);
        if (primaryIndex >= 0)
            return entityAt(primaryIndex);
    }

    for (int i = 0; i < entityCount(); ++i) {
        if (m_entities[i] && m_selectedIds.contains(m_entities[i]->id()))
            return m_entities[i].get();
    }
    return nullptr;
}

void Document::selectNearest(const QPointF& worldPos, double tolerance)
{
    double minDist = tolerance;
    int nearestIndex = -1;

    for (int i = 0; i < entityCount(); ++i) {
        // Skip hidden or locked entities (can't select them)
        if (!isEntityVisible(m_entities[i].get())) continue;
        if (isEntityLocked(m_entities[i].get())) continue;
        double dist = m_entities[i]->distanceTo(worldPos);
        if (dist < minDist) {
            minDist = dist;
            nearestIndex = i;
        }
    }

    clearSelection();
    if (nearestIndex >= 0) {
        m_selectedIds.insert(m_entities[nearestIndex]->id());
        m_primarySelectedId = m_entities[nearestIndex]->id();
        emit selectionChanged();
    }
}

QJsonDocument Document::toJson() const
{
    QJsonObject root;
    root["schemaVersion"] = 2;
    if (!m_defaultToolId.isNull()) root["defaultToolId"] = m_defaultToolId.toString();
    QJsonArray toolReferences;
    for (auto it = m_toolNameHints.constBegin(); it != m_toolNameHints.constEnd(); ++it)
        toolReferences.append(QJsonObject{{"id", it.key().toString()}, {"name", it.value()}});
    root["toolReferences"] = toolReferences;

    // Layers
    QJsonArray layersArray;
    for (const Layer &layer : m_layers) {
        QJsonObject lo;
        lo["id"]      = layer.id.toString();
        lo["name"]    = layer.name;
        lo["visible"] = layer.visible;
        lo["locked"]  = layer.locked;
        lo["color"]   = layer.color.name();
        layersArray.append(lo);
    }
    root["layers"] = layersArray;
    root["activeLayerId"] = m_activeLayerId.toString();

    // Entities
    QJsonArray entitiesArray;
    for (const auto& entity : m_entities) {
        entitiesArray.append(entity->toJsonWithMetadata());
    }

    root["entities"] = entitiesArray;
    return QJsonDocument(root);
}

bool Document::loadFromJson(const QJsonDocument& doc)
{
    if (!doc.isObject() || doc.object()["schemaVersion"].toInt(1) > 2
        || !validToolReferences(doc.object())) return false;
    m_commandStack->clear();
    clear();

    QJsonObject root = doc.object();
    m_defaultToolId = QUuid::fromString(root["defaultToolId"].toString());
    for (const auto& value : root["toolReferences"].toArray()) {
        const auto reference = value.toObject();
        rememberToolName(QUuid::fromString(reference["id"].toString()),
                         reference["name"].toString());
    }

    // Load layers (if present, otherwise keep default layer)
    QJsonArray layersArray = root["layers"].toArray();
    if (!layersArray.isEmpty()) {
        m_layers.clear();
        for (const QJsonValue &val : layersArray) {
            QJsonObject lo = val.toObject();
            Layer layer;
            layer.id      = QUuid::fromString(lo["id"].toString());
            layer.name    = lo["name"].toString("Layer");
            layer.visible = lo["visible"].toBool(true);
            layer.locked  = lo["locked"].toBool(false);
            QString colorStr = lo["color"].toString();
            layer.color = colorStr.isEmpty() ? defaultLayerColor(m_layers.size()) : QColor(colorStr);
            if (!layer.id.isNull())
                m_layers.append(layer);
        }
        if (m_layers.isEmpty()) {
            Layer def = Layer::makeDefault();
            m_activeLayerId = def.id;
            m_layers.append(def);
        } else {
            QUuid activeId = QUuid::fromString(root["activeLayerId"].toString());
            m_activeLayerId = (!activeId.isNull() && indexOfLayer(activeId) >= 0)
                              ? activeId
                              : m_layers[0].id;
        }
    }

    // Load entities
    QJsonArray entitiesArray = root["entities"].toArray();
    for (const QJsonValue& val : entitiesArray) {
        QJsonObject obj = val.toObject();
        std::unique_ptr<Entity> entity = Entity::fromJson(obj);
        if (entity) {
            entity->setEnabled(obj["enabled"].toBool(true));
            QUuid layerId = QUuid::fromString(obj["layerId"].toString());
            entity->setLayerId((!layerId.isNull() && indexOfLayer(layerId) >= 0)
                               ? layerId
                               : m_layers[0].id);
            m_entities.push_back(std::move(entity));
        }
    }

    emit changed();
    return true;
}

// ---------------------------------------------------------------------------
// Layer management
// ---------------------------------------------------------------------------

bool Document::assignToolToSelection(const QUuid& toolId)
{
    auto command = std::make_unique<CompositeCommand>();
    for (int index : selectedIndices()) {
        const Entity* entity = entityAt(index);
        if (!entity) continue;
        auto after = entity->clone();
        if (assignTool(after.get(), *this, toolId))
            command->add(std::make_unique<UpdateEntityCommand>(
                index, entity->clone(), std::move(after), false));
    }
    if (command->empty()) return false;
    m_commandStack->execute(std::move(command));
    return true;
}

QSet<QUuid> Document::toolIdsForEntity(const Entity* entity, bool resolveDefault) const
{
    QSet<QUuid> ids;
    collectToolIds(entity, *this, ids, resolveDefault, false);
    return ids;
}

QSet<QUuid> Document::plottingToolIds() const
{
    QSet<QUuid> ids;
    for (const auto& entity : m_entities)
        collectToolIds(entity.get(), *this, ids, true, true);
    return ids;
}

void Document::setDefaultToolId(const QUuid& id)
{
    if (m_defaultToolId == id) return;
    m_defaultToolId = id;
    emit changed();
}

void Document::rememberToolName(const QUuid& id, const QString& name)
{
    if (!id.isNull() && !name.trimmed().isEmpty()) m_toolNameHints[id] = name.trimmed();
}

QString Document::toolNameHint(const QUuid& id) const
{
    return m_toolNameHints.value(id, id.toString(QUuid::WithoutBraces).left(8));
}

int Document::indexOfLayer(const QUuid& id) const
{
    for (int i = 0; i < m_layers.size(); ++i)
        if (m_layers[i].id == id) return i;
    return -1;
}

void Document::setActiveLayerId(const QUuid& id)
{
    if (indexOfLayer(id) >= 0)
        m_activeLayerId = id;
}

void Document::addLayer(const QString& name)
{
    const auto before = layerState();
    auto after = before;
    Layer l;
    l.id   = QUuid::createUuid();
    l.name = name;
    after.layers.append(l);
    m_commandStack->execute(std::make_unique<EditLayersCommand>(before, std::move(after)));
}

void Document::removeLayerAt(int index)
{
    if (index <= 0 || index >= m_layers.size()) return;  // cannot remove layer 0
    const auto before = layerState();
    auto after = before;
    QUuid removedId = m_layers[index].id;
    QUuid fallbackId = m_layers[0].id;
    for (auto it = after.entityLayers.begin(); it != after.entityLayers.end(); ++it)
        if (it.value() == removedId) it.value() = fallbackId;
    after.layers.removeAt(index);
    if (after.activeLayerId == removedId) after.activeLayerId = fallbackId;
    m_commandStack->execute(std::make_unique<EditLayersCommand>(before, std::move(after)));
}

void Document::setLayerName(int index, const QString& name)
{
    if (index < 0 || index >= m_layers.size()) return;
    if (m_layers[index].name == name) return;
    auto after = layerState();
    after.layers[index].name = name;
    m_commandStack->execute(std::make_unique<EditLayersCommand>(layerState(), std::move(after)));
}

void Document::setLayerVisible(int index, bool visible)
{
    if (index < 0 || index >= m_layers.size()) return;
    if (m_layers[index].visible == visible) return;
    auto after = layerState();
    after.layers[index].visible = visible;
    m_commandStack->execute(std::make_unique<EditLayersCommand>(layerState(), std::move(after)));
}

void Document::setLayerLocked(int index, bool locked)
{
    if (index < 0 || index >= m_layers.size()) return;
    if (m_layers[index].locked == locked) return;
    auto after = layerState();
    after.layers[index].locked = locked;
    m_commandStack->execute(std::make_unique<EditLayersCommand>(layerState(), std::move(after)));
}

void Document::setLayerColor(int index, const QColor& color)
{
    if (index < 0 || index >= m_layers.size()) return;
    if (m_layers[index].color == color) return;
    auto after = layerState();
    after.layers[index].color = color;
    m_commandStack->execute(std::make_unique<EditLayersCommand>(layerState(), std::move(after)));
}

DocumentLayerState Document::layerState() const
{
    DocumentLayerState state{m_layers, m_activeLayerId, {}};
    auto collect = [&](auto&& self, const Entity* entity) -> void {
        if (!entity) return;
        state.entityLayers.insert(entity->id(), entity->layerId());
        if (const auto* group = dynamic_cast<const GroupEntity*>(entity))
            for (const auto& child : group->children()) self(self, child.get());
    };
    for (const auto& entity : m_entities) collect(collect, entity.get());
    return state;
}

void Document::restoreLayerState(const DocumentLayerState& state)
{
    m_layers = state.layers;
    m_activeLayerId = state.activeLayerId;
    auto restore = [&](auto&& self, Entity* entity) -> void {
        if (!entity) return;
        if (state.entityLayers.contains(entity->id())) entity->setLayerId(state.entityLayers[entity->id()]);
        if (auto* group = dynamic_cast<GroupEntity*>(entity))
            for (const auto& child : group->children()) self(self, child.get());
    };
    for (const auto& entity : m_entities) restore(restore, entity.get());
    emit layersChanged();
    if (m_batchDepth == 0) emit changed();
}

bool Document::isEntityVisible(const Entity* e) const
{
    int idx = indexOfLayer(e->layerId());
    if (idx < 0) return true;  // unknown layer = visible
    return m_layers[idx].visible;
}

QColor Document::entityLayerColor(const Entity* e) const
{
    int idx = indexOfLayer(e->layerId());
    if (idx < 0) return QColor("#1f2937");
    return m_layers[idx].color;
}

bool Document::isEntityLocked(const Entity* e) const
{
    int idx = indexOfLayer(e->layerId());
    if (idx < 0) return false;
    return m_layers[idx].locked;
}

bool Document::canUndo() const
{
    return m_commandStack->canUndo();
}

bool Document::canRedo() const
{
    return m_commandStack->canRedo();
}

void Document::undo()
{
    m_commandStack->undo();
}

void Document::redo()
{
    m_commandStack->redo();
}

void Document::normalizeSelectionAfterRemoval(int removedIndex)
{
    Q_UNUSED(removedIndex);

    QSet<QUuid> remainingSelectedIds;
    for (int i = 0; i < entityCount(); ++i) {
        if (m_selectedIds.contains(m_entities[i]->id()))
            remainingSelectedIds.insert(m_entities[i]->id());
    }

    if (remainingSelectedIds == m_selectedIds) {
        ensurePrimarySelection();
        return;
    }

    m_selectedIds = std::move(remainingSelectedIds);
    ensurePrimarySelection();
    emit selectionChanged();
}

void Document::ensurePrimarySelection()
{
    if (m_selectedIds.isEmpty()) {
        m_primarySelectedId = QUuid();
        return;
    }

    if (!m_primarySelectedId.isNull() && m_selectedIds.contains(m_primarySelectedId) && indexOf(m_primarySelectedId) >= 0)
        return;

    for (int i = 0; i < entityCount(); ++i) {
        if (m_entities[i] && m_selectedIds.contains(m_entities[i]->id())) {
            m_primarySelectedId = m_entities[i]->id();
            return;
        }
    }

    m_primarySelectedId = QUuid();
}
