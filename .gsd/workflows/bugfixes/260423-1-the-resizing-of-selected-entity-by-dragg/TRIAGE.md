# Triage: Resize Handle Dragging Bug

**Bug:** Resizing selected entity by dragging resize handle no longer works.

## Root Cause

The canvas detects handle hit in `mousePressEvent()` and stores it in `m_hoverHandleId`, but it **never calls `setHitHandleId()` on the SelectTool** before calling `activeTool().mousePress()`.

The SelectTool requires `m_hitHandleId` to be set before `mousePress()` is called:

```cpp
// SelectTool::mousePress()
Entity* selected = document().selectedEntity();
if (selected && !m_hitHandleId.isEmpty()) {
    m_mode = Mode::DraggingHandle;  // <-- this never happens because m_hitHandleId is empty
    m_activeHandleId = m_hitHandleId;
    m_dragEntityPreview = selected->withDraggedHandle(m_activeHandleId, pos);
    return;
}
```

Since `m_hitHandleId` is never set (via `setHitHandleId()`), the tool never enters `DraggingHandle` mode.

## Reproduction Steps

1. Open Graphics Design page
2. Draw an entity (rectangle, circle, etc.)
3. Select the entity (handles appear)
4. Click on a resize handle and drag
5. **Expected:** Entity resizes as handle is dragged
6. **Actual:** Handle is ignored, entity is deselected or starts rubber-band selection

## Affected Files

- `src/ui/DesignCanvas.cpp` — missing `setHitHandleId()` call in `mousePressEvent()`
- `src/grbl/GraphicsDesign/Tools/SelectTool.h` — has `setHitHandleId()` method (works correctly)
- `src/grbl/GraphicsDesign/Tools/SelectTool.cpp` — uses `m_hitHandleId` in `mousePress()` (works correctly)

## Proposed Fix

In `DesignCanvas::mousePressEvent()`, after detecting the handle hit, call `setHitHandleId()` on the SelectTool:

```cpp
if (m_toolManagerActiveType == ToolManager::Type::Select) {
    auto handle = m_snap.hitHandle(rawWorldPos, selectedHandles());
    m_hoverHandleId = handle ? handle->id : QString();
    // FIX: Pass the hit handle ID to the SelectTool
    if (auto* selectTool = m_toolManager.toolAs<SelectTool>(ToolManager::Type::Select)) {
        selectTool->setHitHandleId(m_hoverHandleId);
    }
}
```

This is a minimal, localized fix that restores the expected behavior.