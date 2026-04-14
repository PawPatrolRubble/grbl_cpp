#include "Document.h"
#include "../GcodeParser.h"

#include <QJsonArray>
#include <algorithm>

Document::Document(QObject* parent)
    : QObject(parent)
    , m_commandStack(new CommandStack(*this))
{
}

Document::~Document()
{
    m_entities.clear();
    delete m_commandStack;
}

void Document::addEntity(std::unique_ptr<Entity> entity)
{
    m_entities.push_back(std::move(entity));
    emit changed();
}

void Document::insertEntity(int index, std::unique_ptr<Entity> entity)
{
    if (index < 0) index = 0;
    if (index > entityCount()) index = entityCount();
    m_entities.insert(m_entities.begin() + index, std::move(entity));
    emit changed();
}

void Document::replaceEntity(int index, std::unique_ptr<Entity> entity)
{
    if (index >= 0 && index < entityCount()) {
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
        QList<GcodeMove> moves = entity->toGcodeMoves(safeZ, cutZ, feedRateXY);
        QStringList entityGcode = GcodeParser::movesToGcode(moves);
        gcode << entityGcode;
    }

    gcode << QString("G0 Z%1        ; final raise").arg(safeZ, 0, 'f', 2);
    gcode << "; End of design G-code";
    return gcode;
}

QJsonDocument Document::toJson() const
{
    QJsonObject root;
    QJsonArray entitiesArray;

    for (const auto& entity : m_entities) {
        entitiesArray.append(entity->toJson());
    }

    root["entities"] = entitiesArray;
    return QJsonDocument(root);
}

bool Document::loadFromJson(const QJsonDocument& doc)
{
    clear();

    QJsonObject root = doc.object();
    QJsonArray entitiesArray = root["entities"].toArray();

    for (const QJsonValue& val : entitiesArray) {
        std::unique_ptr<Entity> entity = Entity::fromJson(val.toObject());
        if (entity) {
            m_entities.push_back(std::move(entity));
        }
    }

    emit changed();
    return true;
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