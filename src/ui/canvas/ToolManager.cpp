#include "ToolManager.h"
#include "../../grbl/GraphicsDesign/Tools/SelectTool.h"
#include "../../grbl/GraphicsDesign/Tools/LineTool.h"
#include "../../grbl/GraphicsDesign/Tools/PolylineTool.h"
#include "../../grbl/GraphicsDesign/Tools/RectangleTool.h"
#include "../../grbl/GraphicsDesign/Tools/CircleTool.h"
#include "../../grbl/GraphicsDesign/Tools/ArcTool.h"
#include "../../grbl/GraphicsDesign/Tools/TextTool.h"
#include "../../grbl/GraphicsDesign/Tools/DimensionTool.h"
#include "../../grbl/GraphicsDesign/Tools/AngularDimensionTool.h"
#include "../../grbl/GraphicsDesign/Tools/RadiusDimensionTool.h"
#include "../../grbl/GraphicsDesign/Tools/TrimExtendTool.h"
#include "../../grbl/GraphicsDesign/Tools/ChamferTool.h"
#include "../../grbl/GraphicsDesign/Tools/FilletTool.h"
#include "../../grbl/GraphicsDesign/Tools/OffsetTool.h"

#include <stdexcept>

ToolManager::ToolManager(Document& document, CommandStack& commandStack)
{
    // Create default tools
    m_tools[static_cast<int>(Type::Select)] = std::make_unique<SelectTool>(document, commandStack);
    m_tools[static_cast<int>(Type::Line)] = std::make_unique<LineTool>(document, commandStack);
    m_tools[static_cast<int>(Type::Polyline)] = std::make_unique<PolylineTool>(document, commandStack);
    m_tools[static_cast<int>(Type::Rectangle)] = std::make_unique<RectangleTool>(document, commandStack);
    m_tools[static_cast<int>(Type::Circle)] = std::make_unique<CircleTool>(document, commandStack);
    m_tools[static_cast<int>(Type::Arc)] = std::make_unique<ArcTool>(document, commandStack);
    m_tools[static_cast<int>(Type::Text)] = std::make_unique<TextTool>(document, commandStack);
    m_tools[static_cast<int>(Type::Dimension)] = std::make_unique<DimensionTool>(document, commandStack);
    m_tools[static_cast<int>(Type::AngularDimension)] = std::make_unique<AngularDimensionTool>(document, commandStack);
    m_tools[static_cast<int>(Type::RadiusDimension)] = std::make_unique<RadiusDimensionTool>(document, commandStack);
    m_tools[static_cast<int>(Type::Trim)] = std::make_unique<TrimExtendTool>(document, commandStack, TrimExtendTool::Mode::Trim);
    m_tools[static_cast<int>(Type::Extend)] = std::make_unique<TrimExtendTool>(document, commandStack, TrimExtendTool::Mode::Extend);
    m_tools[static_cast<int>(Type::Chamfer)] = std::make_unique<ChamferTool>(document, commandStack);
    m_tools[static_cast<int>(Type::Fillet)] = std::make_unique<FilletTool>(document, commandStack);
    m_tools[static_cast<int>(Type::Offset)] = std::make_unique<OffsetTool>(document, commandStack);
}

void ToolManager::registerTool(Type type, std::unique_ptr<Tool> tool)
{
    m_tools[static_cast<int>(type)] = std::move(tool);
}

Tool& ToolManager::toolFor(Type type)
{
    auto it = m_tools.find(static_cast<int>(type));
    if (it == m_tools.end()) {
        throw std::runtime_error("ToolManager::toolFor — unknown tool type");
    }
    return *it->second;
}

const Tool& ToolManager::toolFor(Type type) const
{
    auto it = m_tools.find(static_cast<int>(type));
    if (it == m_tools.end()) {
        throw std::runtime_error("ToolManager::toolFor — unknown tool type");
    }
    return *it->second;
}
