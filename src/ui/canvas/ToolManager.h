#pragma once

#include "../../grbl/GraphicsDesign/Document.h"
#include "../../grbl/GraphicsDesign/CommandStack.h"
#include "../../grbl/GraphicsDesign/Tools/Tool.h"

#include <QPointF>
#include <memory>
#include <unordered_map>
#include <stdexcept>

/// Registry for canvas tools. Replaces the 15 individual tool member variables
/// and duplicated switch(activeToolType) patterns in DesignCanvas.
///
/// Owns all tool instances and provides fast O(1) lookup by tool type enum.
class ToolManager
{
public:
    /// Enum mirroring DesignCanvas::ToolType for type-safe indexing.
    enum class Type {
        Select, Line, Polyline, Rectangle, Circle, Arc, Text,
        Dimension, AngularDimension, RadiusDimension,
        Trim, Extend, Chamfer, Fillet, Offset, Measure, Spline, Count
    };

    explicit ToolManager(Document& document, CommandStack& commandStack);

    /// Get mutable reference to the active tool.
    Tool& active(Type type) { return toolFor(type); }

    /// Get const reference to the active tool.
    const Tool& active(Type type) const { return toolFor(type); }

    /// Register a custom tool at a given type slot (useful for testing or overrides).
    void registerTool(Type type, std::unique_ptr<Tool> tool);

    /// Typed access to a specific tool (returns nullptr if type mismatch).
    template<typename T>
    T* toolAs(Type type) {
        auto it = m_tools.find(static_cast<int>(type));
        if (it == m_tools.end()) return nullptr;
        return dynamic_cast<T*>(it->second.get());
    }

    /// Typed const access to a specific tool.
    template<typename T>
    const T* toolAs(Type type) const {
        auto it = m_tools.find(static_cast<int>(type));
        if (it == m_tools.end()) return nullptr;
        return dynamic_cast<const T*>(it->second.get());
    }

private:
    Tool& toolFor(Type type);
    const Tool& toolFor(Type type) const;

    std::unordered_map<int, std::unique_ptr<Tool>> m_tools;
};
