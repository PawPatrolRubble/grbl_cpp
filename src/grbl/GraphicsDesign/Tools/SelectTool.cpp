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
    return "Click to select entity. Drag handle to edit.";
}

void SelectTool::mousePress(const QPointF& pos)
{
    m_pressPos = pos;
    m_currentPos = pos;
    m_dragEntityPreview.reset();

    Entity* selected = document().selectedEntity();
    if (selected) {
        // Check for handle hit first
        auto handles = selected->editHandles();
        for (const auto& handle : handles) {
            const double dist = QLineF(handle.worldPos, pos).length();
            if (dist < 15.0) {  // Handle hit threshold
                m_mode = Mode::DraggingHandle;
                m_activeHandleId = handle.id;
                m_dragEntityPreview = selected->withDraggedHandle(handle.id, pos);
                return;
            }
        }
    }

    // Try selection
    document().selectNearest(pos, 15.0);
    if (document().selectedEntity()) {
        m_mode = Mode::Selecting;
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
            }
        }
    } else if (m_mode == Mode::MovingEntity) {
        // Update move preview
        Entity* selected = document().selectedEntity();
        if (selected) {
            m_dragEntityPreview = selected->clone();
            m_dragEntityPreview->translate(pos - m_pressPos);
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
}

bool SelectTool::isActive() const
{
    return m_mode != Mode::None;
}