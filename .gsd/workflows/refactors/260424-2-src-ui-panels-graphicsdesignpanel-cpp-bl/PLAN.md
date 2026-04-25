# Refactor Plan: GraphicsDesignPanel.cpp

**File:** `src/ui/panels/GraphicsDesignPanel.cpp`
**Target:** Extract ~1600 lines to focused modules
**Date:** 2026-04-24

---

## Wave Strategy

Each wave:
- Leaves the codebase in a working, compilable state
- Can be committed and tested independently
- Handles dependencies (definition before consumers)

---

## Wave 1: Foundation — Types & Utilities ✅ COMPLETE

**Status:** Completed and committed
**Commit:** `86084e2` - refactor(GraphicsDesignPanel): wave 1 — extract LayerListDelegate and EntityTransform
**Date:** 2026-04-24

### Files Created:
| File | Lines | Content |
|------|-------|---------|
| `src/ui/panels/LayerListDelegate.h` | 18 | Delegate class declaration |
| `src/ui/panels/LayerListDelegate.cpp` | 58 | Delegate implementation (paint, sizeHint) |
| `src/ui/panels/EntityTransform.h` | 38 | Transform helper function declarations |
| `src/ui/panels/EntityTransform.cpp` | 177 | Static geometry helpers: rotatePoint, makeRotatedEntity, makeScaledEntity, makeMirroredEntity, normAngle, selectionCenter |

### Changes to GraphicsDesignPanel.cpp:
- Removed `LayerListDelegate` class definition (68 lines removed)
- Removed static helper functions: `rotatePoint`, `makeRotatedEntity`, `makeScaledEntity`, `flipH`, `flipV`, `makeMirroredEntity`, `normAngle`, `selectionCenter` (180 lines removed)
- Added includes for `LayerListDelegate.h` and `EntityTransform.h`
- Added layer viewport constants (`kEyeColW`, `kIconsW`, `kSwatchOff`, `kColorSwW`) back for eventFilter (7 lines added)
- Updated 14 static function calls to use `EntityTransform::` namespace prefix
- Fixed corrupted file ending

### Results:
- **GraphicsDesignPanel.cpp:** 3101 → 2875 lines (226 lines reduction)
- **New files:** 291 lines total
- **Build:** Compiles and links successfully

### Verification:
✅ Build compiles successfully (GrblSender.exe built)
✅ No syntax errors
✅ All static helper calls updated to namespace

---

## Wave 2: Dialogs

**Goal:** Extract the massive inline ImageImportDialog.

### Files to Create:
| File | Lines | Content |
|------|-------|---------|
| `src/ui/panels/ImageImportDialog.h` | ~40 | Dialog class declaration |
| `src/ui/panels/ImageImportDialog.cpp` | ~350 | Full dialog: preview, threshold/canny modes, settings, vectorizer integration |

### Changes to GraphicsDesignPanel.cpp:
- Remove `onImportImageClicked()` body (lines ~1500-1850)
- Replace with `ImageImportDialog dlg(this); if (dlg.exec() == QDialog::Accepted) { ... }`
- Simplify to ~20 lines: open dialog, get paths, add to document
- Add include for `ImageImportDialog.h`

### ImageImportDialog Interface:
```cpp
class ImageImportDialog : public QDialog {
public:
    explicit ImageImportDialog(const QString& imagePath, QWidget* parent = nullptr);
    QList<QList<QPointF>> processedPaths() const;  // returns polylines
    ImageVectorizer::Settings settings() const;    // for reuse
};
```

### Verification:
- Build compiles
- Image import dialog opens and works
- Settings preview updates correctly
- Polylines imported to document

### Commit:
```
refactor(GraphicsDesignPanel): wave 2 — extract ImageImportDialog
```

---

## Wave 3: Controllers

**Goal:** Extract transform and layer logic into controller classes.

### Files to Create:
| File | Lines | Content |
|------|-------|---------|
| `src/ui/panels/TransformController.h` | ~50 | Controller declaration |
| `src/ui/panels/TransformController.cpp` | ~350 | Transform apply methods: move, rotate, flip, array, scale, polar, mirror, align |
| `src/ui/panels/LayerController.h` | ~40 | Controller declaration |
| `src/ui/panels/LayerController.cpp` | ~420 | Layer management: refresh, add, delete, lock, move-to-layer |

### TransformController Design:
```cpp
class TransformController : public QObject {
    Q_OBJECT
public:
    explicit TransformController(Document* doc, QObject* parent = nullptr);
    
    // UI element registration (called from GraphicsDesignPanel)
    void setMoveWidgets(QDoubleSpinBox* dx, QDoubleSpinBox* dy);
    void setRotateWidget(QDoubleSpinBox* angle);
    void setArrayWidgets(QSpinBox* rows, QSpinBox* cols, QDoubleSpinBox* spacingX, QDoubleSpinBox* spacingY);
    void setScaleWidget(QDoubleSpinBox* factor);
    void setPolarWidgets(QSpinBox* count, QDoubleSpinBox* span, QDoubleSpinBox* centerX, QDoubleSpinBox* centerY);
    void setMirrorWidget(QDoubleSpinBox* axisAngle);
    
public slots:
    void applyMoveBy();
    void applyRotateBy();
    void applyFlipH();
    void applyFlipV();
    void applyRectArray();
    void applyScaleBy();
    void applyPolarArray();
    void applyMirrorByAngle();
    void applyAlignLeft();
    void applyAlignCenterH();
    void applyAlignRight();
    void applyAlignTop();
    void applyAlignCenterV();
    void applyAlignBottom();
    
private:
    Document* m_doc;
    // Widget pointers for reading values
    QDoubleSpinBox* m_moveDx = nullptr;
    // ...
};
```

### LayerController Design:
```cpp
class LayerController : public QObject {
    Q_OBJECT
public:
    explicit LayerController(Document* doc, QListWidget* layerList, QObject* parent = nullptr);
    
    void setButtons(QPushButton* add, QPushButton* delete, QPushButton* lock, QPushButton* moveTo);
    
    void refreshLayerList();
    
public slots:
    void onAddLayerClicked();
    void onDeleteLayerClicked();
    void onLockLayerClicked();
    void onMoveEntityToLayerClicked();
    void onLayerItemChanged(QListWidgetItem* item);
    void onLayerCurrentRowChanged(int row);
    
protected:
    bool eventFilter(QObject* obj, QEvent* event) override;
    
private:
    Document* m_doc;
    QListWidget* m_layerList;
    // ...
};
```

### Changes to GraphicsDesignPanel.cpp:
- Remove all `applyMoveBy`, `applyRotateBy`, `applyFlipH`, `applyFlipV`, `applyRectArray`, `applyScaleBy`, `applyPolarArray`, `applyMirrorByAngle`, `applyAlign*` methods (lines ~2300-2650)
- Remove all layer management methods: `refreshLayerList`, `onLayerItemChanged`, `onAddLayerClicked`, `onDeleteLayerClicked`, `onLockLayerClicked`, `onMoveEntityToLayerClicked`, `eventFilter` for layer viewport (lines ~2680-3101)
- Create `TransformController` and `LayerController` instances in constructor
- Pass widget pointers to controllers
- Wire signals to controller slots in `setupConnections()`
- Remove transform-related member variables (or keep as references to pass to controller)

### Verification:
- Build compiles
- Transform operations work (move, rotate, flip, array, scale, polar, mirror, align)
- Layer management works (add, delete, lock, move-to, visibility toggle)
- Layer list renders correctly with delegate

### Commit:
```
refactor(GraphicsDesignPanel): wave 3 — extract TransformController and LayerController
```

---

## Wave 4: Property Pages

**Goal:** Extract property editor pages into dedicated widgets.

This wave is larger because property pages have many member variables and need careful wiring.

### Files to Create:
| File | Lines | Content |
|------|-------|---------|
| `src/ui/panels/PropertyPages/PropertyPage.h` | ~30 | Base class with `applyChanges()` virtual |
| `src/ui/panels/PropertyPages/EmptyPropertyPage.cpp/.h` | ~20 | "No selection" placeholder |
| `src/ui/panels/PropertyPages/LinePropertyPage.cpp/.h` | ~40 | Line center, length, angle |
| `src/ui/panels/PropertyPages/RectPropertyPage.cpp/.h` | ~50 | Rectangle center, width, height, angle |
| `src/ui/panels/PropertyPages/CirclePropertyPage.cpp/.h` | ~40 | Circle center, radius |
| `src/ui/panels/PropertyPages/ArcPropertyPage.cpp/.h` | ~50 | Arc center, radius, start, sweep |
| `src/ui/panels/PropertyPages/TextPropertyPage.cpp/.h` | ~180 | Text position, content, font modes, single-stroke params |
| `src/ui/panels/PropertyPages/PolylinePropertyPage.cpp/.h` | ~60 | Point table, insert/delete |
| `src/ui/panels/PropertyPages/DimensionPropertyPage.cpp/.h` | ~50 | Linear dimension P1/P2/offset |
| `src/ui/panels/PropertyPages/AngularDimensionPropertyPage.cpp/.h` | ~60 | Angular dimension vertex/rays/radius |
| `src/ui/panels/PropertyPages/RadiusDimensionPropertyPage.cpp/.h` | ~50 | Radius dimension center/radius/angle/diameter |
| `src/ui/panels/PropertyPages/SplinePropertyPage.cpp/.h` | ~40 | Spline point count, steps |

### PropertyPage Base Class:
```cpp
class PropertyPage : public QWidget {
    Q_OBJECT
public:
    explicit PropertyPage(QWidget* parent = nullptr);
    
    virtual void loadFromEntity(const Entity* entity) = 0;
    virtual void applyToEntity(Entity* entity) = 0;
    
signals:
    void propertyChanged();  // emitted when any widget changes
};
```

### PropertyPageFactory:
```cpp
class PropertyPageFactory {
public:
    static PropertyPage* createForEntity(Entity::Kind kind);
    static int pageIndexForKind(Entity::Kind kind);  // for QStackedWidget
};
```

### Changes to GraphicsDesignPanel.cpp:
- Remove `buildEmptyPage`, `buildLinePage`, `buildRectPage`, `buildCirclePage`, `buildArcPage`, `buildTextPage`, `buildPolylinePage`, `buildDimensionPage`, `buildAngularDimensionPage`, `buildRadiusDimensionPage`, `buildSplinePage` (lines ~394-870)
- Remove `applyLineProperties`, `applyRectProperties`, `applyCircleProperties`, `applyArcProperties`, `applyTextProperties`, `applyPolylineProperties`, `polylineInsertVertex`, `polylineDeleteVertex`, `applyDimensionProperties`, `applyAngularDimensionProperties`, `applyRadiusDimensionProperties`, `applySplineProperties` (lines ~1900-2100)
- Remove ~70 property-related member variables from header
- Create property pages in `setupUi()`, add to `m_propertyStack`
- Wire `propertyChanged` signals to apply changes
- Update `onSelectionChanged()` to call `loadFromEntity()` on active page

### Verification:
- Build compiles
- Property pages display correct values when entities selected
- Changes apply correctly to entities
- Undo/redo works for property changes

### Commit:
```
refactor(GraphicsDesignPanel): wave 4 — extract PropertyPage widgets
```

---

## Wave 5: Transform Section Widget

**Goal:** Extract transform UI construction into a dedicated widget.

### Files to Create:
| File | Lines | Content |
|------|-------|---------|
| `src/ui/panels/TransformSectionWidget.h` | ~50 | Widget declaration |
| `src/ui/panels/TransformSectionWidget.cpp` | ~180 | Transform UI grid layout |

### TransformSectionWidget Design:
```cpp
class TransformSectionWidget : public QGroupBox {
    Q_OBJECT
public:
    explicit TransformSectionWidget(QWidget* parent = nullptr);
    
    // Accessors for controller to read values
    QDoubleSpinBox* moveDx() const;
    QDoubleSpinBox* moveDy() const;
    QDoubleSpinBox* rotateAngle() const;
    QDoubleSpinBox* scaleFactor() const;
    QSpinBox* arrayRows() const;
    QSpinBox* arrayCols() const;
    // ... etc
    
signals:
    void moveRequested();
    void rotateRequested();
    void flipHRequested();
    void flipVRequested();
    // ... etc
};
```

### Changes to GraphicsDesignPanel.cpp:
- Remove `buildTransformSection()` (lines ~670-850)
- Remove transform-related member variables from header
- Create `TransformSectionWidget` instance in `setupUi()`
- Wire signals to `TransformController` slots

### Verification:
- Build compiles
- Transform UI displays correctly
- All buttons wired to correct operations

### Commit:
```
refactor(GraphicsDesignPanel): wave 5 — extract TransformSectionWidget
```

---

## Wave 6: Cleanup & Integration

**Goal:** Final cleanup, header simplification, dead code removal.

### Changes:
- Remove unused member variables from header
- Consolidate `setupUi()` into smaller helper functions if needed
- Add forward declarations where appropriate
- Update include order (alphabetical, grouped)
- Remove any remaining inline code that should be in extracted files
- Add namespace comments and documentation
- Run linter/formatting

### Verification:
- Build compiles with no warnings
- All tests pass (if any)
- Manual smoke test: create entities, edit properties, transform, import/export, layers

### Commit:
```
refactor(GraphicsDesignPanel): wave 6 — cleanup and integration
```

---

## File Summary

| Wave | New Files | Lines Extracted |
|------|-----------|-----------------|
| 1 | LayerListDelegate.h/cpp, EntityTransform.h/cpp | ~250 |
| 2 | ImageImportDialog.h/cpp | ~350 |
| 3 | TransformController.h/cpp, LayerController.h/cpp | ~420 |
| 4 | PropertyPage.h + 12 page files | ~500 |
| 5 | TransformSectionWidget.h/cpp | ~180 |
| 6 | (cleanup only) | 0 |

**Total:** ~1700 lines extracted, 18 new files

---

## Post-Refactor Structure

```
src/ui/panels/
├── GraphicsDesignPanel.h          (~80 lines, core members only)
├── GraphicsDesignPanel.cpp        (~600 lines, coordination logic)
├── LayerListDelegate.h/cpp        (~100 lines)
├── EntityTransform.h/cpp          (~200 lines)
├── ImageImportDialog.h/cpp        (~390 lines)
├── TransformController.h/cpp      (~400 lines)
├── LayerController.h/cpp          (~460 lines)
├── TransformSectionWidget.h/cpp   (~230 lines)
└── PropertyPages/
    ├── PropertyPage.h             (~30 lines)
    ├── EmptyPropertyPage.h/cpp    (~40 lines)
    ├── LinePropertyPage.h/cpp     (~80 lines)
    ├── RectPropertyPage.h/cpp     (~100 lines)
    ├── CirclePropertyPage.h/cpp   (~80 lines)
    ├── ArcPropertyPage.h/cpp      (~100 lines)
    ├── TextPropertyPage.h/cpp     (~200 lines)
    ├── PolylinePropertyPage.h/cpp (~120 lines)
    ├── DimensionPropertyPage.h/cpp (~100 lines)
    ├── AngularDimensionPropertyPage.h/cpp (~120 lines)
    ├── RadiusDimensionPropertyPage.h/cpp (~100 lines)
    └── SplinePropertyPage.h/cpp   (~80 lines)
```

---

## Risk Assessment

| Risk | Mitigation |
|------|------------|
| Property page wiring complexity | Use factory pattern, test each page incrementally |
| Member variable relocation errors | Keep originals temporarily, remove after verification |
| Signal/slot connection breakage | Use `setupConnections()` as single wiring point, test after each wave |
| Entity transform logic errors | Extract to `EntityTransform` first (pure functions), unit test if possible |
| Layer controller event filter | Carefully move `eventFilter` logic, test viewport click handling |

---

## Verification Commands

After each wave:
```bash
# Build
cmake --build build --target all

# Run application and smoke test:
# - Open graphics design panel
# - Create line, rectangle, circle, arc, text
# - Select entity, edit properties
# - Transform: move, rotate, flip, array
# - Import image (Wave 2+)
# - Manage layers (Wave 3+)
# - Export G-code
```

---

## Estimated Timeline

| Wave | Complexity | Time |
|------|------------|------|
| 1 | Low (pure extraction) | 15 min |
| 2 | Medium (dialog wiring) | 30 min |
| 3 | Medium-High (controllers) | 45 min |
| 4 | High (many pages) | 60 min |
| 5 | Medium (widget extraction) | 20 min |
| 6 | Low (cleanup) | 15 min |

**Total:** ~2.5-3 hours for full refactor