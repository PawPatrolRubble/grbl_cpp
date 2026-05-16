#include "SelectTool.h"

#include "../Entity.h"
#include "../LineEntity.h"
#include "../PolylineEntity.h"

#include <QLineF>
#include <QSet>
#include <memory>

namespace {

bool isSelectableEntity(const Document& document, const Entity* entity)
{
    return entity && entity->isEnabled() && document.isEntityVisible(entity) && !document.isEntityLocked(entity);
}

int nearestSelectableEntity(const Document& document, const QPointF& pos, double tolerance)
{
    int nearest = -1;
    double bestDist = tolerance;
    for (int i = 0; i < document.entityCount(); ++i) {
        const Entity* entity = document.entityAt(i);
        if (!isSelectableEntity(document, entity))
            continue;
        const double distance = entity->distanceTo(pos);
        if (distance < bestDist) {
            bestDist = distance;
            nearest = i;
        }
    }
    return nearest;
}

} // namespace

SelectTool::SelectTool(Document& document, CommandStack& commandStack)
    : Tool(document, commandStack)
{
}

QString SelectTool::name() const
{
    return "Select";
}

QString SelectTool::instructions() const
{
    if (m_mode == Mode::DraggingHandle) {
        return "Drag handle to resize/rotate. Release to apply.";
    }
    if (m_mode == Mode::MovingEntity) {
        return "Drag to move selection. Release to apply.";
    }
    if (m_mode == Mode::RubberBand) {
        return "Drag to select entities in region. Release to confirm.";
    }
    return "Click to select. Ctrl+click to add/remove. Drag empty space to box-select.";
}

void SelectTool::mousePress(const QPointF& pos)
{
    m_pressPos = pos;
    m_currentPos = pos;
    m_dragEntityPreview.reset();
    m_dragEntityIndices.clear();
    m_dragEntityBefore.clear();
    m_dragEntityAfter.clear();

    Entity* selected = document().selectedEntity();
    if (selected && !m_hitHandleId.isEmpty()) {
        m_mode = Mode::DraggingHandle;
        m_activeHandleId = m_hitHandleId;
        m_hitHandleId.clear();
        m_dragEntityPreview = selected->withDraggedHandle(m_activeHandleId, pos);
        return;
    }
    m_hitHandleId.clear();

    const int nearest = nearestSelectableEntity(document(), pos, 15.0);
    if (m_ctrlPressed && nearest >= 0) {
        const Entity* entity = document().entityAt(nearest);
        if (document().selectedIds().contains(entity->id()))
            document().deselect(entity->id());
        else
            document().select(entity->id());
        m_mode = Mode::None;
        return;
    }

    if (nearest >= 0) {
        const Entity* entity = document().entityAt(nearest);
        if (!document().selectedIds().contains(entity->id())) {
            document().clearSelection();
            document().select(entity->id());
        }
        m_mode = Mode::Selecting;
    } else {
        m_rubberBandAdditive = m_ctrlPressed;
        m_mode = Mode::RubberBand;
    }
}

void SelectTool::mouseMove(const QPointF& pos)
{
    m_currentPos = pos;

    if (m_mode == Mode::DraggingHandle) {
        Entity* selected = document().selectedEntity();
        if (selected && m_dragEntityPreview) {
            m_dragEntityPreview = selected->withDraggedHandle(m_activeHandleId, pos);
        }
    } else if (m_mode == Mode::Selecting) {
        if (QLineF(m_pressPos, pos).length() > 5.0) {
            m_mode = Mode::MovingEntity;
            m_lastMovePos = pos;
            const QVector<int> selectedIndices = document().selectedIndices();
            m_dragEntityIndices.reserve(selectedIndices.size());
            m_dragEntityBefore.reserve(selectedIndices.size());
            m_dragEntityAfter.reserve(selectedIndices.size());
            for (int index : selectedIndices) {
                Entity* entity = document().entityAt(index);
                if (!entity)
                    continue;
                m_dragEntityIndices.push_back(index);
                m_dragEntityBefore.push_back(entity->clone());
                auto preview = entity->clone();
                preview->translate(pos - m_pressPos);
                m_dragEntityAfter.push_back(std::move(preview));
            }
        }
    } else if (m_mode == Mode::MovingEntity) {
        const QPointF delta = pos - m_lastMovePos;
        for (auto& preview : m_dragEntityAfter) {
            if (preview)
                preview->translate(delta);
        }
        m_lastMovePos = pos;
    }
}

void SelectTool::mouseRelease(const QPointF& pos)
{
    if (m_mode == Mode::DraggingHandle) {
        Entity* selected = document().selectedEntity();
        if (selected) {
            auto before = selected->clone();
            auto after = selected->withDraggedHandle(m_activeHandleId, pos);
            int index = document().indexOf(selected->id());
            if (index >= 0) {
                commandStack().execute(std::make_unique<UpdateEntityCommand>(
                    index, std::move(before), std::move(after)));
            }
        }
    } else if (m_mode == Mode::MovingEntity) {
        auto command = std::make_unique<CompositeCommand>();
        for (size_t i = 0; i < m_dragEntityIndices.size(); ++i) {
            if (!m_dragEntityBefore[i] || !m_dragEntityAfter[i])
                continue;
            command->add(std::make_unique<UpdateEntityCommand>(
                m_dragEntityIndices[i], std::move(m_dragEntityBefore[i]), std::move(m_dragEntityAfter[i])));
        }
        if (!command->empty())
            commandStack().execute(std::move(command));
    } else if (m_mode == Mode::RubberBand) {
        QRectF band(m_pressPos, m_currentPos);
        band = band.normalized();
        if (band.width() > 1.0 || band.height() > 1.0) {
            if (!m_rubberBandAdditive)
                document().clearSelection();
            for (int i = 0; i < document().entityCount(); ++i) {
                const Entity* entity = document().entityAt(i);
                if (!isSelectableEntity(document(), entity))
                    continue;
                if (band.intersects(entity->boundingRect()))
                    document().select(entity->id());
            }
        } else if (!m_rubberBandAdditive) {
            document().clearSelection();
        }
    }

    m_mode = Mode::None;
    m_activeHandleId.clear();
    m_dragEntityPreview.reset();
    m_dragEntityIndices.clear();
    m_dragEntityBefore.clear();
    m_dragEntityAfter.clear();
    m_rubberBandAdditive = false;
}

void SelectTool::cancel()
{
    m_mode = Mode::None;
    m_activeHandleId.clear();
    m_dragEntityPreview.reset();
    m_dragEntityIndices.clear();
    m_dragEntityBefore.clear();
    m_dragEntityAfter.clear();
    m_rubberBandAdditive = false;
}

void SelectTool::paintPreview(QPainter& painter) const
{
    if (m_mode == Mode::DraggingHandle && m_dragEntityPreview) {
        m_dragEntityPreview->draw(painter);
    }
    if (m_mode == Mode::MovingEntity) {
        for (const auto& preview : m_dragEntityAfter) {
            if (preview)
                preview->draw(painter);
        }
    }
    if (m_mode == Mode::RubberBand) {
        QRectF band(m_pressPos, m_currentPos);
        painter.save();
        painter.setPen(QPen(QColor(59, 130, 246, 200), 1.5 / painter.transform().m11(), Qt::DashLine));
        painter.setBrush(QColor(59, 130, 246, 30));
        painter.drawRect(band);
        painter.restore();
    }
}

bool SelectTool::isActive() const
{
    return m_mode != Mode::None;
}

QSet<QUuid> SelectTool::snapExclusionIds() const
{
    if (m_mode == Mode::DraggingHandle || m_mode == Mode::MovingEntity)
        return document().selectedIds();
    return {};
}

std::optional<QPointF> SelectTool::inferenceReferencePoint() const
{
    if (m_mode != Mode::DraggingHandle)
        return std::nullopt;

    const Entity* selected = document().selectedEntity();
    if (!selected)
        return std::nullopt;

    if (selected->kind() == Entity::Kind::Line) {
        const auto* line = static_cast<const LineEntity*>(selected);
        if (m_activeHandleId == "start") return line->endPoint();
        if (m_activeHandleId == "end") return line->startPoint();
        return std::nullopt;
    }

    if (selected->kind() == Entity::Kind::Polygon && m_activeHandleId.startsWith("pt_")) {
        const auto* polyline = static_cast<const PolylineEntity*>(selected);
        const int index = m_activeHandleId.mid(3).toInt();
        const auto& points = polyline->points();
        if (index == 0 && points.size() > 1) return points[1];
        if (index == points.size() - 1 && points.size() > 1) return points[points.size() - 2];
    }

    return std::nullopt;
}

std::optional<QRectF> SelectTool::rubberBandRect() const
{
    if (m_mode != Mode::RubberBand) return std::nullopt;
    return QRectF(m_pressPos, m_currentPos).normalized();
}
