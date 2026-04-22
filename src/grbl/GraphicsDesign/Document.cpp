#include "Document.h"
#include "../GcodeParser.h"

#include <QJsonArray>
#include <algorithm>

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
    if (entity->layerId().isNull())
        entity->setLayerId(m_activeLayerId);
    m_entities.push_back(std::move(entity));
    if (m_batchDepth == 0) emit changed();
}

void Document::beginBatch() { ++m_batchDepth; }
void Document::endBatch() { if (--m_batchDepth <= 0) { m_batchDepth = 0; emit changed(); } }

void Document::insertEntity(int index, std::unique_ptr<Entity> entity)
{
    if (index < 0) index = 0;
    if (index > entityCount()) index = entityCount();
    if (entity->layerId().isNull())
        entity->setLayerId(m_activeLayerId);
    m_entities.insert(m_entities.begin() + index, std::move(entity));
    emit changed();
}

void Document::replaceEntity(int index, std::unique_ptr<Entity> entity)
{
    if (index >= 0 && index < entityCount()) {
        // Preserve entity ID so selection stays valid, and preserve layer assignment
        entity->setId(m_entities[index]->id());
        entity->setLayerId(m_entities[index]->layerId());
        m_entities[index] = std::move(entity);
        emit changed();
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
    emit changed();
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
    if (indexOf(id) >= 0) {
        m_selectedIds.insert(id);
        emit selectionChanged();
    }
}

void Document::deselect(const QUuid& id)
{
    if (m_selectedIds.remove(id)) {
        emit selectionChanged();
    }
}

void Document::clearSelection()
{
    if (!m_selectedIds.isEmpty()) {
        m_selectedIds.clear();
        emit selectionChanged();
    }
}

void Document::selectAll()
{
    for (const auto& e : m_entities) {
        if (e && e->isEnabled())
            m_selectedIds.insert(e->id());
    }
    emit selectionChanged();
}

void Document::setSelected(const QUuid& id, bool selected)
{
    if (selected) select(id);
    else deselect(id);
}

Entity* Document::selectedEntity() const
{
    if (m_selectedIds.isEmpty()) return nullptr;
    return entityAt(indexOf(*m_selectedIds.begin()));
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
        emit selectionChanged();
    }
}

QStringList Document::generateGcode(double feedRateXY, double plungeRateZ,
                                     double safeZ, double cutZ) const
{
    QStringList gcode;
    gcode << "G21 G90        ; metric, absolute";
    gcode << QString("G0 Z%1        ; raise pen").arg(safeZ, 0, 'f', 2);

    for (const auto& entity : m_entities) {
        if (!entity->isEnabled()) continue;
        if (!isEntityVisible(entity.get())) continue;
        if (entity->kind() == Entity::Kind::LinearDimension) continue;
        if (entity->kind() == Entity::Kind::AngularDimension) continue;
        if (entity->kind() == Entity::Kind::RadiusDimension) continue;
        QList<GcodeMove> moves = entity->toGcodeMoves(safeZ, cutZ, feedRateXY);
        QStringList entityGcode = GcodeParser::movesToGcode(moves);
        gcode << entityGcode;
    }

    gcode << QString("G0 Z%1        ; final raise").arg(safeZ, 0, 'f', 2);
    gcode << "G0 X0 Y0        ; return to origin";
    gcode << "; End of design G-code";
    return gcode;
}

QJsonDocument Document::toJson() const
{
    QJsonObject root;

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
        QJsonObject entityJson = entity->toJson();
        entityJson["enabled"] = entity->isEnabled();
        entityJson["layerId"] = entity->layerId().toString();
        entitiesArray.append(entityJson);
    }

    root["entities"] = entitiesArray;
    return QJsonDocument(root);
}

bool Document::loadFromJson(const QJsonDocument& doc)
{
    clear();

    QJsonObject root = doc.object();

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
    Layer l;
    l.id   = QUuid::createUuid();
    l.name = name;
    m_layers.append(l);
    emit layersChanged();
}

void Document::removeLayerAt(int index)
{
    if (index <= 0 || index >= m_layers.size()) return;  // cannot remove layer 0
    QUuid removedId = m_layers[index].id;
    QUuid fallbackId = m_layers[0].id;
    // Reassign entities on removed layer to layer 0
    for (auto& e : m_entities) {
        if (e->layerId() == removedId)
            e->setLayerId(fallbackId);
    }
    m_layers.removeAt(index);
    if (m_activeLayerId == removedId)
        m_activeLayerId = fallbackId;
    emit layersChanged();
    emit changed();
}

void Document::setLayerName(int index, const QString& name)
{
    if (index < 0 || index >= m_layers.size()) return;
    m_layers[index].name = name;
    emit layersChanged();
}

void Document::setLayerVisible(int index, bool visible)
{
    if (index < 0 || index >= m_layers.size()) return;
    m_layers[index].visible = visible;
    emit layersChanged();
    emit changed();
}

void Document::setLayerLocked(int index, bool locked)
{
    if (index < 0 || index >= m_layers.size()) return;
    m_layers[index].locked = locked;
    emit layersChanged();
    emit changed();
}

void Document::setLayerColor(int index, const QColor& color)
{
    if (index < 0 || index >= m_layers.size()) return;
    m_layers[index].color = color;
    emit layersChanged();
    emit changed();
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
    // If selected entity was removed, clear its selection
    for (int i = 0; i < entityCount(); ++i) {
        if (m_selectedIds.contains(m_entities[i]->id())) {
            // Selection still valid
            return;
        }
    }
    clearSelection();
}