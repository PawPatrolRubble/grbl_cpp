# 2D CAD Core Roadmap

## Goal

Build the GraphicsDesign module into a practical 2D CAD editor for CNC and plotting workflows while keeping the current document model, entity system, and G-code pipeline as the single source of truth.

## Current Baseline

Already implemented in the project:

- Document model with entity storage, selection, JSON load/save, and undo/redo
- Core entities: line, rectangle, circle, arc, text, polyline
- Drawing tools: select, line, rectangle, circle, arc, text
- Handle-based editing for existing entities
- Canvas interaction: pan, zoom, grid/entity snap, auto-fit
- DXF import, image vectorization, and G-code generation

## Core Capability Layers

### 1. Drafting Primitives

These are the minimum shape creation features expected from a 2D CAD editor.

- Line
- Polyline
- Rectangle
- Circle
- Arc
- Text
- Ellipse
- Polygon
- Linear dimension

Priority in this project:

- Keep the existing primitives stable
- Add missing authoring entry points for already-supported entities before adding brand new geometry types

### 2. Selection And Editing

These are the capabilities that make the editor usable beyond simple sketching.

- Single selection
- Multi-selection
- Box selection
- Handle editing
- Property editing panel
- Delete
- Undo and redo

Priority in this project:

- Strengthen selection-driven editing before complex geometry algorithms

### 3. Modify Operations

This is the first major productivity layer for real CAD work.

- Copy
- Paste
- Duplicate
- Move
- Rotate
- Scale
- Mirror
- Offset
- Trim
- Extend
- Fillet
- Chamfer
- Array
- Join and explode

Recommended implementation order:

1. Copy, paste, duplicate
2. Move and rotate from numeric input
3. Scale and mirror
4. Array
5. Trim, extend, offset
6. Fillet, chamfer, join, explode

### 4. Precision Tools

These tools are what separate CAD interaction from freehand drawing.

- Grid snap
- Endpoint, midpoint, center, quadrant snap
- Ortho mode
- Polar tracking
- Numeric coordinate entry
- Angle and length constraints during draw

Priority in this project:

- Extend the current snap system before introducing constraints that require a larger interaction redesign

### 5. Organization And Reuse

These features matter once files grow beyond simple sketches.

- Layers
- Entity visibility and locking
- Grouping
- Blocks and instances
- Named styles

Priority in this project:

- Layers first
- Grouping second
- Blocks after layer and transform workflows are stable

### 6. Interoperability

These features keep the CAD editor useful inside a larger toolchain.

- DXF import
- DXF export
- JSON project save and load
- SVG export
- Clipboard import and export where useful

Priority in this project:

- Keep improving DXF fidelity
- Add DXF export after core editing workflows settle

### 7. CNC-Oriented Finishing

These capabilities connect CAD editing back to machining output.

- Toolpath ordering optimization
- Polyline merging and cleanup
- Arc fitting and path simplification
- Layer-to-operation mapping
- Pen, laser, spindle presets

Priority in this project:

- Only after drafting and modify operations reach a stable baseline

## Staged Delivery Plan

### Phase 1: Editing Foundation

Target:

- Copy, paste, duplicate
- Better selection workflows
- Stable property editing

Why first:

- Low implementation risk
- High daily usability gain
- Reused by later transform and array features

### Phase 2: Missing Primitive Entry Points

Target:

- Polyline drawing tool
- Ellipse tool
- Polygon tool

Why second:

- The data model already covers part of this surface area
- It expands authoring power without changing the architecture

### Phase 3: Numeric Modify Commands

Target:

- Move
- Rotate
- Scale
- Mirror

Why third:

- These are core CAD operations
- They build directly on the copy and selection work from Phase 1

### Phase 4: Precision Upgrade

Target:

- Ortho mode
- Better object snap types
- Numeric length and angle entry while drawing

### Phase 5: Document Organization

Target:

- Layers
- Lock and hide
- Grouping

### Phase 6: Advanced Geometry

Target:

- Trim
- Extend
- Offset
- Fillet
- Chamfer
- Join and explode

## Current Execution Decision

The first implementation target should be `copy / paste / duplicate`.

Reason:

- It is a true core CAD operation
- It fits the current `Entity::clone()` plus `CommandStack` design cleanly
- It unlocks later features like array, mirror, and pattern generation
- It avoids risky geometry work early in the roadmap

## Next Recommended Steps After Phase 1

1. Add a polyline drawing tool so imported and authored geometry use the same primitive
2. Add numeric move / rotate dialogs for selected entities
3. Expand snapping with midpoint and tangent-aware interaction where applicable