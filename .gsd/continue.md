# Continue — GraphicsDesignPanel Component Extraction

## Last action

Committed and pushed `8d5daf3` — extracted PropertyPages (10), TransformPanel, LayerPanel, ToolBarWidget. ToolBarWidget integrated into GDP, replacing `createToolToolBar()` and `m_toolButtons[17]`. Build verified.

## Next action

1. Create private helper methods in GDP that take parameters:
   - `moveSelection(double dx, double dy)` — calls canvas methods directly
   - `rotateSelection(double angle)` — uses selection center as pivot
   - `scaleSelection(double factor)`
   - etc. (14 operations total)

2. Connect TransformPanel signals to these helpers in `setupConnections()`

3. Replace `buildTransformSection()` call in `setupUi()` with:
   ```cpp
   m_transformPanel = new TransformPanel();
   rightLayout->addWidget(m_transformPanel);
   ```

4. Delete `buildTransformSection()` function and transform widget member variables

## Why

TransformPanel emits signals with parameters (e.g., `moveRequested(double dx, double dy)`). Current GDP slots read from widget members (`m_moveDx->value()`). Need to refactor slots to accept parameters, keeping transform logic but changing the interface.

## Open threads

- LayerPanel integration pending — similar pattern (signals need handler methods)
- PropertyPage integration pending — need to replace `buildXxxPage()` with PropertyPage instances
- `buildLayerSection()` and layer member widgets need removal after LayerPanel integration
- TextPage extraction was skipped (complex font modes, single-stroke params)

## Do not

- DO NOT modify `applyProperties()` — it's used by all transform operations
- DO NOT delete `m_transformGroup` visibility toggle logic until TransformPanel integration verified
- DO NOT change TransformPanel signal signatures — they're already extracted and compiled
- DO NOT run `build\bin\Debug\GrblSender.exe` without testing tool switching first

## Key files

- `src/ui/panels/GraphicsDesignPanel.cpp` — line 1065: `buildTransformSection()` (to delete)
- `src/ui/panels/GraphicsDesignPanel.cpp` — line 1454: apply slots (to refactor)
- `src/ui/panels/TransformPanel.h` — signals with parameters
- `src/ui/panels/GraphicsDesignPanel.h` — transform member variables (to delete after integration)

## Pattern established

ToolBarWidget integration pattern:
1. Add include for new component
2. Add forward declaration in header
3. Replace member type (QWidget* → ToolBarWidget*)
4. Replace instantiation (new → new ToolBarWidget())
5. Connect signals in setupConnections()
6. Delete old function
7. Delete old member variables
8. Delete old slot if no longer needed

Apply same pattern to TransformPanel and LayerPanel.