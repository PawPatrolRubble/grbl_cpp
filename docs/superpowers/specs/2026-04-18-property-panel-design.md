# Property Panel for Selected Shapes — Design Spec

## Summary

Add an editable property panel to `GraphicsDesignPanel` that shows shape-specific properties when a user selects an entity on the canvas. The property panel replaces the current `m_shapeInfoLabel` (which only shows "Selected: Line") with a `QStackedWidget` containing per-entity-type pages with editable spin boxes and line edits.

## Approach

**QStackedWidget with per-entity-type pages** (matching the TinyCAD `PropertiesPanel` pattern).

### Architecture

- Replace `m_shapeInfoLabel` with a `QStackedWidget m_propertyStack`
- 6 pages: Empty, Line, Rectangle, Circle, Arc, Text
- Each page uses `QFormLayout` with `QDoubleSpinBox` for numeric values and `QLineEdit` for text
- Type dispatch in `onSelectionChanged()` using `entity->kind()` enum

### Pages and Properties

| Page      | Properties                                                      |
|-----------|-----------------------------------------------------------------|
| Empty     | "No selection" label                                            |
| Line      | Center X, Center Y, Length, Angle                               |
| Rectangle | Center X, Center Y, Width, Height, Angle                        |
| Circle    | Center X, Center Y, Radius                                     |
| Arc       | Center X, Center Y, Radius, Start Angle (deg), Sweep Angle (deg) |
| Text      | Pos X, Pos Y, Text (QLineEdit), Font Size                      |

### Data Flow

1. **Selection → Panel (refresh):**
   - `DesignCanvas::selectedEntityChanged` → `GraphicsDesignPanel::onSelectionChanged()`
   - Set `m_updatingUi = true`
   - Get entity via `m_canvas->selectedEntity()`
   - `dynamic_cast` to concrete type, read getters, populate spin boxes with `QSignalBlocker`
   - Switch `m_stack->setCurrentWidget(...)` to appropriate page
   - Set `m_updatingUi = false`

2. **Panel → Shape (apply-back):**
   - Each spin box `valueChanged` connects to a type-specific `applyXxxProperties()` slot
   - `applyXxxProperties()` checks `m_updatingUi` guard
   - Constructs a new entity from widget values using setters (e.g., `setCenterLengthAngle()`, `setGeometry()`)
   - Pushes an `UpdateEntityCommand` onto the command stack for undo/redo
   - Document emits `changed()` → triggers panel refresh

### Re-entrancy Guard

- `bool m_updatingUi` prevents apply-back during refresh
- `QSignalBlocker` per widget during population suppresses individual `valueChanged` signals

### Spin Box Ranges

| Field type    | Min       | Max      | Decimals |
|---------------|-----------|----------|----------|
| Coordinates   | -100000   | 100000   | 2        |
| Length/Width/Height/Radius | 0 | 100000 | 2  |
| Angle         | -3600     | 3600     | 2        |
| Font Size     | 1         | 100000   | 2        |

### New Members (GraphicsDesignPanel.h)

Widgets:
- `QStackedWidget *m_propertyStack`
- `QWidget *m_emptyPage`
- `QWidget *m_linePage`, `QDoubleSpinBox *m_lineCenterX/Y`, `*m_lineLength`, `*m_lineAngle`
- `QWidget *m_rectPage`, `QDoubleSpinBox *m_rectCenterX/Y`, `*m_rectWidth`, `*m_rectHeight`, `*m_rectAngle`
- `QWidget *m_circlePage`, `QDoubleSpinBox *m_circleCenterX/Y`, `*m_circleRadius`
- `QWidget *m_arcPage`, `QDoubleSpinBox *m_arcCenterX/Y`, `*m_arcRadius`, `*m_arcStartAngle`, `*m_arcSweepAngle`
- `QWidget *m_textPage`, `QDoubleSpinBox *m_textPosX/Y`, `*m_textFontSize`, `QLineEdit *m_textEdit`

Slots:
- `void applyLineProperties()`
- `void applyRectProperties()`
- `void applyCircleProperties()`
- `void applyArcProperties()`
- `void applyTextProperties()`

State:
- `bool m_updatingUi = false`

### Command Stack Integration

Apply-back uses `Document::replaceEntity()` with an `UpdateEntityCommand` (following TinyCAD's pattern). This ensures:
- Undo/redo works for property edits
- The `changed()` signal triggers a panel refresh, keeping UI in sync

### Files to Modify

- `src/ui/panels/GraphicsDesignPanel.h` — Add widget members, slots, pages
- `src/ui/panels/GraphicsDesignPanel.cpp` — Replace `m_shapeInfoLabel`, build pages, wire apply-back
- `src/grbl/GraphicsDesign/Document.h/.cpp` — May need `replaceEntity()` or use existing command infrastructure

### Scope Exclusions

- No new entity types (Polygon, LinearDimension remain unimplemented)
- No multi-selection editing
- No property panel for "no entity type matched" beyond the empty page