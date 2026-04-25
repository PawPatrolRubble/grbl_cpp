# Refactor Inventory: GraphicsDesignPanel.cpp

**File:** `src/ui/panels/GraphicsDesignPanel.cpp`
**Lines:** 3101
**Date:** 2026-04-24

---

## Executive Summary

`GraphicsDesignPanel.cpp` is severely bloated (~3100 lines) with 15+ distinct responsibilities that should be separated into focused modules. The file mixes:
- UI construction and layout
- Property editor pages for 11 entity types
- Signal/slot connection wiring
- Entity selection handling
- Transform operations (move, rotate, scale, mirror, array)
- Layer management
- File import/export (including a 300-line image import dialog)
- G-code generation preview
- Custom delegate class

---

## Catalog of Responsibilities

### 1. LayerListDelegate Class (Lines 35-102, ~68 lines)
**Type:** Custom delegate for layer list rendering  
**Extract to:** `src/ui/panels/LayerListDelegate.cpp/.h`

Contains:
- `paint()` - renders eye/lock icons and color swatch
- `sizeHint()` - custom sizing
- Constants: `kEyeColW`, `kLockColW`, `kColorSwW`, `kIconsW`, `kSwatchOff`

---

### 2. UI Setup (Lines 104-394, ~290 lines)
**Type:** Widget construction and layout  
**Keep in:** GraphicsDesignPanel (but could use builder helpers)

Contains:
- Constructor
- `setupUi()` - creates all widgets, layouts, splitter, toolbar
- Floating property panel construction
- Control bar (snap, grid, fillet, chamfer, offset, measure)
- Action buttons (undo, redo, delete, clear)
- Export/import buttons
- G-code preview area
- Options grid (tool mode, feed speed, plunge speed, safe Z, work Z)

**Recommendation:** Consider extracting to smaller helper functions or a `PanelLayoutBuilder` class if setup becomes more complex.

---

### 3. Property Page Builders (Lines ~394-870, ~476 lines)
**Type:** Property editor UI for each entity type  
**Extract to:** Separate property page widgets

| Method | Lines | Entity Type | Target File |
|--------|-------|-------------|-------------|
| `buildEmptyPage()` | ~12 | Empty state | `PropertyPages/EmptyPropertyPage.cpp` |
| `buildLinePage()` | ~25 | Line | `PropertyPages/LinePropertyPage.cpp` |
| `buildRectPage()` | ~30 | Rectangle | `PropertyPages/RectPropertyPage.cpp` |
| `buildCirclePage()` | ~20 | Circle | `PropertyPages/CirclePropertyPage.cpp` |
| `buildArcPage()` | ~25 | Arc | `PropertyPages/ArcPropertyPage.cpp` |
| `buildTextPage()` | ~150 | Text (complex) | `PropertyPages/TextPropertyPage.cpp` |
| `buildPolylinePage()` | ~40 | Polyline | `PropertyPages/PolylinePropertyPage.cpp` |
| `buildDimensionPage()` | ~30 | Linear dimension | `PropertyPages/DimensionPropertyPage.cpp` |
| `buildAngularDimensionPage()` | ~30 | Angular dimension | `PropertyPages/AngularDimensionPropertyPage.cpp` |
| `buildRadiusDimensionPage()` | ~30 | Radius dimension | `PropertyPages/RadiusDimensionPropertyPage.cpp` |
| `buildSplinePage()` | ~25 | Spline | `PropertyPages/SplinePropertyPage.cpp` |

**Recommendation:** Create a `PropertyPage` base class and derive specific pages. Use factory pattern to instantiate based on entity kind.

---

### 4. Transform Section Builder (Lines ~670-850, ~180 lines)
**Type:** Transform UI construction  
**Extract to:** `TransformSectionWidget.cpp/.h`

Contains:
- `buildTransformSection()` - creates move, rotate, mirror, scale, array UI
- Rows for: move dx/dy, rotate angle, flip H/V, rectangular array, polar array, scale, mirror by angle, align H/V

---

### 5. Layer Section Builder (Lines ~850-880, ~30 lines)
**Type:** Layer UI construction  
**Extract to:** Already small, can stay or merge with LayerController

---

### 6. Signal/Slot Connections (Lines ~880-1020, ~140 lines)
**Type:** Connection wiring  
**Keep in:** GraphicsDesignPanel (necessary for panel cohesion)

Contains:
- `setupConnections()` - wires all signals to slots
- Toolbar → canvas connections
- Snap/grid parameter connections
- Undo/redo connections
- Document state change connections
- Property spinbox connections
- Layer management connections

---

### 7. Selection Handling (Lines ~1020-1200, ~180 lines)
**Type:** Property UI sync with selection  
**Keep in:** GraphicsDesignPanel (but could delegate to PropertyPageManager)

Contains:
- `onSelectionChanged()` - large switch statement updating property pages
- Entity kind detection and UI update
- Transform group visibility toggle
- Entity list sync

---

### 8. Entity List Management (Lines ~1200-1300, ~100 lines)
**Type:** Entity list widget operations  
**Keep in:** GraphicsDesignPanel (or extract to EntityListController)

Contains:
- `refreshEntityList()` - populates entity list widget
- `onEntityItemChanged()` - handles checkbox toggles
- Entity list ↔ canvas selection sync

---

### 9. Preview and Generation (Lines ~1300-1450, ~150 lines)
**Type:** G-code preview  
**Keep in:** GraphicsDesignPanel (core functionality)

Contains:
- `updatePreview()` - generates G-code preview text
- `onGenerateClicked()` - emits gcodeGenerated signal
- Line count label update
- Layer summary formatting

---

### 10. File Operations (Lines ~1450-1500, ~50 lines)
**Type:** Save/load/export  
**Keep in:** GraphicsDesignPanel (or extract to FileOperations)

Contains:
- `onSaveDesignClicked()` - JSON save
- `onLoadDesignClicked()` - JSON load
- `onDeleteSelectedClicked()`
- `onClearAllClicked()`
- `onExportGcodeClicked()` - NC file export

---

### 11. Image Import Dialog (Lines ~1500-1850, ~350 lines)
**Type:** Complex modal dialog  
**Extract to:** `ImageImportDialog.cpp/.h`

Contains:
- `onImportImageClicked()` - entire dialog construction
- Preview widget
- Threshold/Canny mode switching
- Settings: adaptive threshold, block size, Canny low/high, blur, epsilon, target width, invert
- Real-time preview updates
- Vectorizer processing

**This is the most glaring violation - a 350-line inline dialog.**

---

### 12. DXF Import (Lines ~1850-1900, ~50 lines)
**Type:** File import  
**Keep in:** GraphicsDesignPanel (simple enough) or merge with FileOperations

Contains:
- `onImportDxfClicked()` - DxfParser usage
- Batch entity addition

---

### 13. Property Apply Methods (Lines ~1900-2100, ~200 lines)
**Type:** Entity modification from property UI  
**Extract to:** PropertyPage classes or EntityModifier helper

Contains:
- `applyProperties()` - base method with UpdateEntityCommand
- `applyLineProperties()`
- `applyRectProperties()`
- `applyCircleProperties()`
- `applyArcProperties()`
- `applyTextProperties()` (largest, handles font modes)
- `applyPolylineProperties()`
- `polylineInsertVertex()`
- `polylineDeleteVertex()`
- `applyDimensionProperties()`
- `applyAngularDimensionProperties()`
- `applyRadiusDimensionProperties()`
- `applySplineProperties()`

---

### 14. Transform Static Helpers (Lines ~2100-2300, ~200 lines)
**Type:** Geometry transformation utilities  
**Extract to:** `EntityTransform.cpp/.h` (no Qt widget dependencies)

Contains:
- `rotatePoint()` - point rotation helper
- `makeRotatedEntity()` - entity rotation factory
- `makeScaledEntity()` - entity scaling factory
- `flipH()`, `flipV()` - mirror helpers
- `makeMirroredEntity()` - entity mirror factory
- `normAngle()` - angle normalization
- `selectionCenter()` - bounding box center computation

---

### 15. Transform Apply Methods (Lines ~2300-2650, ~350 lines)
**Type:** Transform operation execution  
**Extract to:** `TransformController.cpp/.h` or merge with EntityTransform

Contains:
- `applyMoveBy()`
- `applyRotateBy()`
- `applyFlipH()`, `applyFlipV()`
- `applyRectArray()` - rectangular array
- `applyScaleBy()`
- `applyPolarArray()` - polar array
- `applyMirrorByAngle()` - arbitrary axis mirror
- `applyAlignLeft()`, `applyAlignCenterH()`, `applyAlignRight()`
- `applyAlignTop()`, `applyAlignCenterV()`, `applyAlignBottom()`

---

### 16. Measure Tool Readout (Lines ~2650-2680, ~30 lines)
**Type:** Measurement display  
**Keep in:** GraphicsDesignPanel (small enough)

Contains:
- `onMeasurementChanged()` - updates distance, dx, dy, angle labels

---

### 17. Layer Management Slots (Lines ~2680-3101, ~420 lines)
**Type:** Layer operations  
**Extract to:** `LayerController.cpp/.h`

Contains:
- `refreshLayerList()` - populates layer list widget
- `eventFilter()` - intercepts viewport clicks for eye/lock/color
- `onLayerItemChanged()` - handles visibility checkbox and rename
- `onLayerCurrentRowChanged()` - updates button states
- `onAddLayerClicked()`
- `onDeleteLayerClicked()`
- `onLockLayerClicked()`
- `onMoveEntityToLayerClicked()`

---

### 18. Tool Changed Handlers (Lines ~1020-1100, ~80 lines)
**Type:** Tool state sync  
**Keep in:** GraphicsDesignPanel (core functionality)

Contains:
- `onToolChanged()` - updates canvas tool, shows/hides tool-specific widgets
- `onCanvasToolChanged()` - syncs toolbar buttons when canvas changes tool

---

## Member Variables (Header Analysis)

The header declares **~100 member variables** across categories:

| Category | Count | Examples |
|----------|-------|----------|
| Core widgets | 4 | m_canvas, m_toolBar, m_floatingPropertyPanel, m_propertyStack |
| Control bar | 12 | m_snapBtn, m_gridSizeSpin, m_filletRadiusSpin, m_chamferDistSpin, m_offsetDistSpin, m_measureGroup, ... |
| Action buttons | 7 | m_deleteBtn, m_clearBtn, m_saveBtn, m_loadBtn, m_exportBtn, m_generateBtn |
| Line props | 4 | m_lineCenterX/Y, m_lineLength, m_lineAngle |
| Rect props | 5 | m_rectCenterX/Y, m_rectWidth, m_rectHeight, m_rectAngle |
| Circle props | 3 | m_circleCenterX/Y, m_circleRadius |
| Arc props | 5 | m_arcCenterX/Y, m_arcRadius, m_arcStartAngle, m_arcSweepAngle |
| Text props | 15+ | m_textPosX/Y, m_textEdit, m_textFontSize, m_textFontModeCombo, m_textVectorFontCombo, m_textSystemFontCombo, m_textSingleStrokeCheck, m_textSsResolution, ... |
| Polyline props | 4 | m_polylinePointCountLabel, m_polylineTable, m_polylineInsertBtn, m_polylineDeleteBtn |
| Dimension props | 5 | m_dimP1X/Y, m_dimP2X/Y, m_dimOffset |
| Angular dim props | 7 | m_adimVertexX/Y, m_adimP1X/Y, m_adimP2X/Y, m_adimRadius |
| Radius dim props | 5 | m_rdimCX/Y, m_rdimRadius, m_rdimAngle, m_rdimDiameter |
| Spline props | 2 | m_splinePointCountLabel, m_splineStepsSpin |
| Transform | 13 | m_transformGroup, m_moveDx/Dy, m_rotateAngle, m_arrayRows/Cols, m_arraySpacingX/Y, m_scaleFactor, m_polarCount, m_polarSpan, m_polarCenterX/Y, m_mirrorAxisAngle |
| Layer | 5 | m_layerGroup, m_layerList, m_addLayerBtn, m_deleteLayerBtn, m_lockLayerBtn, m_moveToLayerBtn |
| Preview/Status | 5 | m_gcodePreview, m_lineCountLabel, m_statusLabel, m_generatedLines, m_feedSpeedSpinBox, ... |

---

## Dependency Analysis

### Files that GraphicsDesignPanel.cpp depends on:
- Qt widgets (QComboBox, QDoubleSpinBox, QPushButton, QToolButton, QDialog, QFileDialog, QColorDialog, etc.)
- `DesignCanvas.h` - canvas widget
- `ToolBarWidget.h` - extracted toolbar component
- `LineNumberPlainTextEdit.h` - custom edit widget
- Entity headers: `LineEntity.h`, `RectangleEntity.h`, `CircleEntity.h`, `ArcEntity.h`, `TextEntity.h`, `PolylineEntity.h`, `LinearDimensionEntity.h`, `AngularDimensionEntity.h`, `RadiusDimensionEntity.h`, `SplineEntity.h`
- `Document.h`, `Entity.h`, `CommandStack.h` - document model
- `VectorFontCatalog.h` - font management
- `ImageVectorizer.h` - image processing
- `DxfParser.h` - DXF import

### Files that depend on GraphicsDesignPanel:
- `GraphicsDesignPage.cpp` - uses the panel
- `MainWindow.cpp` - may reference

---

## Extracted Components Priority

### High Priority (Large & Clear Boundaries)
1. **ImageImportDialog** (~350 lines) - self-contained modal dialog
2. **LayerListDelegate** (~68 lines) - standalone class
3. **EntityTransform** (~200 lines static helpers) - no widget dependencies, reusable
4. **TransformController/Section** (~350 lines) - transform UI + operations
5. **LayerController** (~420 lines) - layer management

### Medium Priority (Moderate Complexity)
6. **PropertyPages** (~476 lines) - 11 property page widgets, reduces member variable sprawl
7. **FileOperations** (~100 lines) - save/load/export/import DXF

### Low Priority (Core Panel Logic)
8. **EntityListController** (~100 lines) - entity list widget handling
9. **PropertyPageManager** - coordinates property pages, reduces switch statement

---

## Estimated Scope

| Metric | Value |
|--------|-------|
| Lines to extract | ~1600 |
| New files to create | ~15-20 |
| Member variables to relocate | ~70-80 |
| Methods to relocate | ~40 |
| Remaining in GraphicsDesignPanel | ~1500 → ~600-800 after refactor |

---

## Wave Strategy

See `PLAN.md` for detailed wave breakdown.