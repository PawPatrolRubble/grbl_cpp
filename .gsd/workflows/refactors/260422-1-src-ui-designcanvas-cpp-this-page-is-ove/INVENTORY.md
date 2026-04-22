# Inventory: DesignCanvas Refactor

## Source: `src/ui/DesignCanvas.cpp` (991 lines) + `src/ui/DesignCanvas.h` (177 lines)

### 1. Tool Management (lines 55-81 in .h, lines 127-144 in .cpp)
- **15 tool member variables** in DesignCanvas.h (SelectTool through OffsetTool)
- **Duplicated switch statement** — `activeTool()` and `activeTool() const` are near-identical (~30 lines each)
- **17 tool includes** in the header file
- `setTool()` method manages tool switching

### 2. Coordinate Transforms (lines 265-290 in .cpp)
- `toWorld()` — Screen to world (machine) coords
- `toScreen()` — World to screen coords
- `machineToScreen()` — Machine to screen coords
- `screenToMachine()` — Screen to machine coords
- `applyZoom()` — Zoom with anchor point
- State: `m_zoom`, `m_panOffset`, `m_zoomSettleTimer`

### 3. Drawing Subsystems (lines 296-830 in .cpp)
- `drawGridAndRulers()` — Grid lines computation + batched rendering (~40 lines)
- `drawRulerStrips()` — Horizontal + vertical ruler strips with labels (~80 lines)
- `drawWorktable()` — Worktable background, ruled lines, margin, border (~50 lines)
- `drawAxisOrigin()` — X/Y axis arrows with labels (~40 lines)
- `drawEntities()` — Entity rendering with culling, layer visibility, opacity (~40 lines)
- `drawHandles()` — Edit handles for selected entity (~40 lines)
- `drawSnapIndicator()` — Snap crosshair + circle (~20 lines)
- `drawCrosshair()` — Mouse crosshair + coordinate label (~45 lines)
- `drawGcodeOverlay()` — G-code toolpath overlay with arc approximation (~60 lines)
- `drawPreview()` — Stub, delegates to tool (~2 lines)
- Pen factory functions: `createEntityPen()`, `createSelectedPen()`, `createPreviewPen()` (~15 lines)

### 4. Input Handling (lines 830-991 in .cpp)
- `mousePressEvent()` — Left/middle/right button handling, snap, tool delegation (~30 lines)
- `mouseMoveEvent()` — Panning, snap, hover cursor, tool delegation (~25 lines)
- `mouseReleaseEvent()` — Snap, tool delegation (~20 lines)
- `mouseDoubleClickEvent()` — Polyline finish (~20 lines)
- `wheelEvent()` — Zoom (~5 lines)
- `keyPressEvent()` — 10+ keyboard shortcuts (polyline, copy/paste/dup, undo/redo, escape, delete, snap, select all, export) (~80 lines)
- `contextMenuEvent()` — Move to layer, delete, export (~40 lines)
- `leaveEvent()` — Mouse tracking (~5 lines)

### 5. Clipboard Operations (lines 690-760 in .cpp)
- `copySelectedEntity()` — Clone selected entity
- `pasteCopiedEntity()` — Paste with offset
- `duplicateSelectedEntity()` — Clone + offset
- `addEntityFromShortcut()` — Add to command stack, auto-select
- State: `m_copiedEntity`, `m_pasteSerial`
- Helper: `shortcutOffsetForStep()`

### 6. Paint Event Orchestration (lines 480-545 in .cpp)
- `paintEvent()` — 65-line orchestration of pixmap cache + drawing layers

### 7. Utility Methods (lines 113-260 in .cpp)
- `emitToolStatus()`, `emitUndoRedoState()` — Signal emissions
- `scheduleUpdate()` — Throttled repaint
- `cancelActiveTool()` — Reset snap, cursor, tool state
- `undo()`, `redo()` — With tool cancellation
- `deleteSelectedEntity()` — Reverse-order deletion
- `fitToSelection()`, `autoFit()` — View fitting
- Settings setters: `setCanvasSettings`, `setSnapEnabled`, `setGridSize`, etc.
- Tool property getters/setters: `filletRadius`, `chamferDistance`, `offsetDistance`

### 8. Event Handlers (resize, show)
- `resizeEvent()` — Invalidate cache (~5 lines)
- `showEvent()` — Initial auto-fit (~10 lines)

### 9. Snap System (lines 660-690 in .cpp)
- `computeSnap()` — Grid + entity snap point evaluation (~35 lines)
- `selectedHandles()` — Get handles from selected entity
- `hitHandle()` — Screen-space handle hit test
- `updateHoverCursor()` — Cursor based on handle role
- State: `m_snapResult`, `m_snapEnabled`, `m_hoverHandleId`

### 10. Constants (lines 11-20 in .cpp)
- `kPi`, `kShortcutDuplicateOffset`, `kHandleHalfSizePx`, `kHandleHitHalfSizePx`, `kGridStep`, `kSnapDistancePx`

## Dependencies

```
DesignCanvas
├── ToolManager (extracted) — needs Document, CommandStack, all tool types
├── CoordinateTransform (extracted) — standalone math
├── CanvasDrawing (extracted) — needs Document, Entity, settings, transforms
├── CanvasInput (extracted) — needs ToolManager, CanvasDrawing, clipboard, transforms
├── EntityClipboard (extracted) — needs Document, CommandStack
└── DesignCanvas (remaining) — orchestrates all subsystems, ~150 lines
```

## Files to Modify
| File | Change |
|------|--------|
| `src/ui/DesignCanvas.h` | Remove 15 tool members, ~30 member vars, ~20 method declarations; add new subsystem members |
| `src/ui/DesignCanvas.cpp` | Remove ~850 lines; keep orchestration + event delegation |

## Files to Create
| File | Lines (est.) |
|------|-------------|
| `src/ui/ToolManager.h` | ~60 |
| `src/ui/ToolManager.cpp` | ~50 |
| `src/ui/CoordinateTransform.h` | ~50 |
| `src/ui/CoordinateTransform.cpp` | ~60 |
| `src/ui/CanvasDrawing.h` | ~80 |
| `src/ui/CanvasDrawing.cpp` | ~400 |
| `src/ui/CanvasInput.h` | ~60 |
| `src/ui/CanvasInput.cpp` | ~300 |
| `src/ui/EntityClipboard.h` | ~40 |
| `src/ui/EntityClipboard.cpp` | ~80 |

## Estimated Scope
- **10 files total** (5 new .h/.cpp pairs)
- **~1,200 lines of new code** (extracted from existing, not new logic)
- **~1,000 lines removed** from DesignCanvas.cpp
- **~100 lines remaining** in DesignCanvas.cpp (orchestration)
- **0 behavioral changes** — pure extraction, no feature changes
