# Refactor Plan: DesignCanvas

## Waves Executed

### Wave 1: Infrastructure modules (no cross-dependencies)
- `CoordinateTransform.h` — header-only coordinate math
- `EntityClipboard.h/cpp` — copy/paste/duplicate
- `ToolManager.h/cpp` — tool registry, replaces 15 members + switches

### Wave 2: Drawing extraction
- `CanvasDrawing.h/cpp` — all rendering logic (~600 lines extracted)
- `CanvasSnap.h/cpp` — snap computation + handle hit-testing

### Wave 3: Integration
- Rewrote `DesignCanvas.h` — removed 15 tool members, 17 tool includes
- Rewrote `DesignCanvas.cpp` — delegates to subsystems
- Updated `CMakeLists.txt` — added new files

### Wave 4: Verification
- Full Release build passes
- No old tool member references remain
- Zero behavioral changes — pure extraction
