#include "SelectTool.h"

#include "../Entity.h"

#include <QSet>
#include <memory>

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
        return "Drag to move entity. Release to apply.";
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

    Entity* selected = document().selectedEntity();
    if (selected && !m_hitHandleId.isEmpty()) {
        m_mode = Mode::DraggingHandle;
        m_activeHandleId = m_hitHandleId;
        m_hitHandleId.clear();
        m_dragEntityPreview = selected->withDraggedHandle(m_activeHandleId, pos);
        return;
    }
    m_hitHandleId.clear();

    if (m_ctrlPressed) {
        // Ctrl+click: find nearest and toggle its selection state
        const double tol = 15.0;
        // find nearest entity
        int nearest = -1;
        double bestDist = tol;
        for (int i = 0; i < document().entityCount(); ++i) {
            const Entity* e = document().entityAt(i);
            if (!e || !e->isEnabled()) continue;
            double d = e->distanceTo(pos);
            if (d < bestDist) { bestDist = d; nearest = i; }
        }
        if (nearest >= 0) {
            const Entity* e = document().entityAt(nearest);
            if (document().selectedIds().contains(e->id()))
                document().deselect(e->id());
            else
                document().select(e->id());
        }
        m_mode = Mode::None;
        return;
    }

    // Normal click: find nearest; if miss, start rubber-band
    int nearest = -1;
    double bestDist = 15.0;
    for (int i = 0; i < document().entityCount(); ++i) {
        const Entity* e = document().entityAt(i);
        if (!e || !e->isEnabled()) continue;
        double d = e->distanceTo(pos);
        if (d < bestDist) { bestDist = d; nearest = i; }
    }

    if (nearest >= 0) {
        document().clearSelection();
        document().select(document().entityAt(nearest)->id());
        m_mode = Mode::Selecting;
    } else {
        // Miss: start rubber-band
        document().clearSelection();
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
        // Check if we've moved far enough to start dragging
        if (QLineF(m_pressPos, pos).length() > 5.0) {
            m_mode = Mode::MovingEntity;
            // Create initial move preview
            Entity* selected = document().selectedEntity();
            if (selected) {
                m_dragEntityPreview = selected->clone();
                m_dragEntityPreview->translate(pos - m_pressPos);
                m_lastMovePos = pos;
            }
        }
    } else if (m_mode == Mode::RubberBand) {
        m_currentPos = pos;  // update current pos for drawing
    } else if (m_mode == Mode::MovingEntity) {
        Entity* selected = document().selectedEntity();
        if (selected && m_dragEntityPreview) {
            // Reset to original position, then apply new delta
            m_dragEntityPreview->translate(pos - m_lastMovePos);
            m_lastMovePos = pos;
        }
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
        Entity* selected = document().selectedEntity();
        if (selected) {
            auto before = selected->clone();
            const QPointF delta = pos - m_pressPos;
            auto after = selected->clone();
            after->translate(delta);
            int index = document().indexOf(selected->id());
            if (index >= 0) {
                commandStack().execute(std::make_unique<UpdateEntityCommand>(
                    index, std::move(before), std::move(after)));
            }
        }
    } else if (m_mode == Mode::RubberBand) {
        // Select all entities whose bounding rect intersects the rubber-band
        QRectF band(m_pressPos, m_currentPos);
        band = band.normalized();
        if (band.width() > 1.0 || band.height() > 1.0) {
            for (int i = 0; i < document().entityCount(); ++i) {
                const Entity* e = document().entityAt(i);
                if (!e || !e->isEnabled()) continue;
                if (band.intersects(e->boundingRect()))
                    document().select(e->id());
            }
        }
    }

    m_mode = Mode::None;
    m_activeHandleId.clear();
    m_dragEntityPreview.reset();
}

void SelectTool::cancel()
{
    m_mode = Mode::None;
    m_activeHandleId.clear();
    m_dragEntityPreview.reset();
}

void SelectTool::paintPreview(QPainter& painter) const
{
    if ((m_mode == Mode::DraggingHandle || m_mode == Mode::MovingEntity) && m_dragEntityPreview) {
        m_dragEntityPreview->draw(painter);
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

std::optional<QRectF> SelectTool::rubberBandRect() const
{
    if (m_mode != Mode::RubberBand) return std::nullopt;
    return QRectF(m_pressPos, m_currentPos).normalized();
}