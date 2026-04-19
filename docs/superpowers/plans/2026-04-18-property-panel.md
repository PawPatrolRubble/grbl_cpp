# Property Panel Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add an editable property panel that shows shape-specific properties when a user selects an entity on the canvas, replacing the current `m_shapeInfoLabel`.

**Architecture:** QStackedWidget with per-entity-type pages inside the existing right panel. Type-dispatch in `onSelectionChanged()` switches pages. Apply-back methods construct new entities and push `UpdateEntityCommand` onto the undo stack. Re-entrancy guarded by `m_updatingUi` bool.

**Tech Stack:** Qt 6 (QWidget, QStackedWidget, QFormLayout, QDoubleSpinBox, QLineEdit, QSignalBlocker)

---

### Task 1: Add widget members and slots to GraphicsDesignPanel.h

**Files:**
- Modify: `src/ui/panels/GraphicsDesignPanel.h`

- [ ] **Step 1: Add includes and forward declarations**

Add to the includes section after existing includes:

```cpp
#include <QStackedWidget>
#include <QFormLayout>
#include <QLineEdit>
```

Add forward declarations after existing ones:

```cpp
class QStackedWidget;
class QFormLayout;
class QLineEdit;
```

(The `QDoubleSpinBox` forward declaration already exists.)

- [ ] **Step 2: Add new private slots**

Add after the existing `onExportGcodeClicked()` slot declaration:

```cpp
void applyLineProperties();
void applyRectProperties();
void applyCircleProperties();
void applyArcProperties();
void applyTextProperties();
```

- [ ] **Step 3: Add new private member variables**

Replace `QLabel *m_shapeInfoLabel;` with:

```cpp
// Property panel
QStackedWidget *m_propertyStack;
bool m_updatingUi = false;

// Empty page
QLabel *m_emptyPageLabel;

// Line page
QDoubleSpinBox *m_lineCenterX;
QDoubleSpinBox *m_lineCenterY;
QDoubleSpinBox *m_lineLength;
QDoubleSpinBox *m_lineAngle;

// Rectangle page
QDoubleSpinBox *m_rectCenterX;
QDoubleSpinBox *m_rectCenterY;
QDoubleSpinBox *m_rectWidth;
QDoubleSpinBox *m_rectHeight;
QDoubleSpinBox *m_rectAngle;

// Circle page
QDoubleSpinBox *m_circleCenterX;
QDoubleSpinBox *m_circleCenterY;
QDoubleSpinBox *m_circleRadius;

// Arc page
QDoubleSpinBox *m_arcCenterX;
QDoubleSpinBox *m_arcCenterY;
QDoubleSpinBox *m_arcRadius;
QDoubleSpinBox *m_arcStartAngle;
QDoubleSpinBox *m_arcSweepAngle;

// Text page
QDoubleSpinBox *m_textPosX;
QDoubleSpinBox *m_textPosY;
QLineEdit *m_textEdit;
QDoubleSpinBox *m_textFontSize;
```

Also remove the `#include <QLabel>` forward declaration since we still use QLabel (keep it if it's a forward declaration, but we need it for m_emptyPageLabel). Actually, `QLabel` is already forward-declared — we just use it as a pointer. Keep the existing `class QLabel;` forward declaration.

---

### Task 2: Build the QStackedWidget pages in setupUi()

**Files:**
- Modify: `src/ui/panels/GraphicsDesignPanel.cpp`

- [ ] **Step 1: Replace m_shapeInfoLabel with the property stack**

In `setupUi()`, find and remove these lines (approximately lines 92-94):

```cpp
// Shape info
m_shapeInfoLabel = new QLabel("No selection");
rightLayout->addWidget(m_shapeInfoLabel);
```

Replace with:

```cpp
// Property panel (stacked pages per entity type)
m_propertyStack = new QStackedWidget();
rightLayout->addWidget(m_propertyStack);

buildEmptyPage();
buildLinePage();
buildRectPage();
buildCirclePage();
buildArcPage();
buildTextPage();
```

- [ ] **Step 2: Add the buildEmptyPage() method**

Add a new method to GraphicsDesignPanel.cpp:

```cpp
void GraphicsDesignPanel::buildEmptyPage()
{
    auto *page = new QWidget();
    auto *layout = new QVBoxLayout(page);
    m_emptyPageLabel = new QLabel("No selection");
    m_emptyPageLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_emptyPageLabel);
    layout->addStretch();
    m_propertyStack->addWidget(page);
}
```

- [ ] **Step 3: Implement buildLinePage()**

```cpp
void GraphicsDesignPanel::buildLinePage()
{
    auto *page = new QWidget();
    auto *layout = new QFormLayout(page);

    m_lineCenterX = new QDoubleSpinBox();
    m_lineCenterX->setRange(-100000, 100000);
    m_lineCenterX->setDecimals(2);
    layout->addRow("Center X:", m_lineCenterX);

    m_lineCenterY = new QDoubleSpinBox();
    m_lineCenterY->setRange(-100000, 100000);
    m_lineCenterY->setDecimals(2);
    layout->addRow("Center Y:", m_lineCenterY);

    m_lineLength = new QDoubleSpinBox();
    m_lineLength->setRange(0, 100000);
    m_lineLength->setDecimals(2);
    layout->addRow("Length:", m_lineLength);

    m_lineAngle = new QDoubleSpinBox();
    m_lineAngle->setRange(-3600, 3600);
    m_lineAngle->setDecimals(2);
    layout->addRow("Angle:", m_lineAngle);

    m_propertyStack->addWidget(page);
}
```

- [ ] **Step 4: Implement buildRectPage()**

```cpp
void GraphicsDesignPanel::buildRectPage()
{
    auto *page = new QWidget();
    auto *layout = new QFormLayout(page);

    m_rectCenterX = new QDoubleSpinBox();
    m_rectCenterX->setRange(-100000, 100000);
    m_rectCenterX->setDecimals(2);
    layout->addRow("Center X:", m_rectCenterX);

    m_rectCenterY = new QDoubleSpinBox();
    m_rectCenterY->setRange(-100000, 100000);
    m_rectCenterY->setDecimals(2);
    layout->addRow("Center Y:", m_rectCenterY);

    m_rectWidth = new QDoubleSpinBox();
    m_rectWidth->setRange(0, 100000);
    m_rectWidth->setDecimals(2);
    layout->addRow("Width:", m_rectWidth);

    m_rectHeight = new QDoubleSpinBox();
    m_rectHeight->setRange(0, 100000);
    m_rectHeight->setDecimals(2);
    layout->addRow("Height:", m_rectHeight);

    m_rectAngle = new QDoubleSpinBox();
    m_rectAngle->setRange(-3600, 3600);
    m_rectAngle->setDecimals(2);
    layout->addRow("Angle:", m_rectAngle);

    m_propertyStack->addWidget(page);
}
```

- [ ] **Step 5: Implement buildCirclePage()**

```cpp
void GraphicsDesignPanel::buildCirclePage()
{
    auto *page = new QWidget();
    auto *layout = new QFormLayout(page);

    m_circleCenterX = new QDoubleSpinBox();
    m_circleCenterX->setRange(-100000, 100000);
    m_circleCenterX->setDecimals(2);
    layout->addRow("Center X:", m_circleCenterX);

    m_circleCenterY = new QDoubleSpinBox();
    m_circleCenterY->setRange(-100000, 100000);
    m_circleCenterY->setDecimals(2);
    layout->addRow("Center Y:", m_circleCenterY);

    m_circleRadius = new QDoubleSpinBox();
    m_circleRadius->setRange(0, 100000);
    m_circleRadius->setDecimals(2);
    layout->addRow("Radius:", m_circleRadius);

    m_propertyStack->addWidget(page);
}
```

- [ ] **Step 6: Implement buildArcPage()**

```cpp
void GraphicsDesignPanel::buildArcPage()
{
    auto *page = new QWidget();
    auto *layout = new QFormLayout(page);

    m_arcCenterX = new QDoubleSpinBox();
    m_arcCenterX->setRange(-100000, 100000);
    m_arcCenterX->setDecimals(2);
    layout->addRow("Center X:", m_arcCenterX);

    m_arcCenterY = new QDoubleSpinBox();
    m_arcCenterY->setRange(-100000, 100000);
    m_arcCenterY->setDecimals(2);
    layout->addRow("Center Y:", m_arcCenterY);

    m_arcRadius = new QDoubleSpinBox();
    m_arcRadius->setRange(0, 100000);
    m_arcRadius->setDecimals(2);
    layout->addRow("Radius:", m_arcRadius);

    m_arcStartAngle = new QDoubleSpinBox();
    m_arcStartAngle->setRange(-3600, 3600);
    m_arcStartAngle->setDecimals(2);
    layout->addRow("Start Angle:", m_arcStartAngle);

    m_arcSweepAngle = new QDoubleSpinBox();
    m_arcSweepAngle->setRange(-3600, 3600);
    m_arcSweepAngle->setDecimals(2);
    layout->addRow("Sweep Angle:", m_arcSweepAngle);

    m_propertyStack->addWidget(page);
}
```

- [ ] **Step 7: Implement buildTextPage()**

```cpp
void GraphicsDesignPanel::buildTextPage()
{
    auto *page = new QWidget();
    auto *layout = new QFormLayout(page);

    m_textPosX = new QDoubleSpinBox();
    m_textPosX->setRange(-100000, 100000);
    m_textPosX->setDecimals(2);
    layout->addRow("Pos X:", m_textPosX);

    m_textPosY = new QDoubleSpinBox();
    m_textPosY->setRange(-100000, 100000);
    m_textPosY->setDecimals(2);
    layout->addRow("Pos Y:", m_textPosY);

    m_textEdit = new QLineEdit();
    layout->addRow("Text:", m_textEdit);

    m_textFontSize = new QDoubleSpinBox();
    m_textFontSize->setRange(1, 100000);
    m_textFontSize->setDecimals(2);
    layout->addRow("Font Size:", m_textFontSize);

    m_propertyStack->addWidget(page);
}
```

- [ ] **Step 8: Add build method declarations to the header**

In `GraphicsDesignPanel.h`, add these private method declarations after the existing `void setupConnections();` line:

```cpp
void buildEmptyPage();
void buildLinePage();
void buildRectPage();
void buildCirclePage();
void buildArcPage();
void buildTextPage();
```

---

### Task 3: Wire apply-back signal connections in setupConnections()

**Files:**
- Modify: `src/ui/panels/GraphicsDesignPanel.cpp`

- [ ] **Step 1: Add property panel signal connections**

At the end of `setupConnections()`, add:

```cpp
// Property panel apply-back connections
connect(m_lineCenterX, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
        this, &GraphicsDesignPanel::applyLineProperties);
connect(m_lineCenterY, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
        this, &GraphicsDesignPanel::applyLineProperties);
connect(m_lineLength, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
        this, &GraphicsDesignPanel::applyLineProperties);
connect(m_lineAngle, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
        this, &GraphicsDesignPanel::applyLineProperties);

connect(m_rectCenterX, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
        this, &GraphicsDesignPanel::applyRectProperties);
connect(m_rectCenterY, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
        this, &GraphicsDesignPanel::applyRectProperties);
connect(m_rectWidth, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
        this, &GraphicsDesignPanel::applyRectProperties);
connect(m_rectHeight, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
        this, &GraphicsDesignPanel::applyRectProperties);
connect(m_rectAngle, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
        this, &GraphicsDesignPanel::applyRectProperties);

connect(m_circleCenterX, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
        this, &GraphicsDesignPanel::applyCircleProperties);
connect(m_circleCenterY, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
        this, &GraphicsDesignPanel::applyCircleProperties);
connect(m_circleRadius, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
        this, &GraphicsDesignPanel::applyCircleProperties);

connect(m_arcCenterX, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
        this, &GraphicsDesignPanel::applyArcProperties);
connect(m_arcCenterY, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
        this, &GraphicsDesignPanel::applyArcProperties);
connect(m_arcRadius, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
        this, &GraphicsDesignPanel::applyArcProperties);
connect(m_arcStartAngle, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
        this, &GraphicsDesignPanel::applyArcProperties);
connect(m_arcSweepAngle, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
        this, &GraphicsDesignPanel::applyArcProperties);

connect(m_textPosX, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
        this, &GraphicsDesignPanel::applyTextProperties);
connect(m_textPosY, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
        this, &GraphicsDesignPanel::applyTextProperties);
connect(m_textEdit, &QLineEdit::textChanged,
        this, &GraphicsDesignPanel::applyTextProperties);
connect(m_textFontSize, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
        this, &GraphicsDesignPanel::applyTextProperties);
```

---

### Task 4: Implement onSelectionChanged() with type dispatch

**Files:**
- Modify: `src/ui/panels/GraphicsDesignPanel.cpp`
- Modify: `src/ui/panels/GraphicsDesignPanel.h` (add includes)

- [ ] **Step 1: Add includes for concrete entity types**

At the top of `GraphicsDesignPanel.cpp`, add:

```cpp
#include "../../grbl/GraphicsDesign/LineEntity.h"
#include "../../grbl/GraphicsDesign/RectangleEntity.h"
#include "../../grbl/GraphicsDesign/CircleEntity.h"
#include "../../grbl/GraphicsDesign/ArcEntity.h"
#include "../../grbl/GraphicsDesign/TextEntity.h"
```

- [ ] **Step 2: Replace the existing onSelectionChanged() method**

Replace the current implementation:

```cpp
void GraphicsDesignPanel::onSelectionChanged()
{
    const Entity *entity = m_canvas->selectedEntity();
    if (entity) {
        QString typeName;
        switch (entity->kind()) {
            case Entity::Kind::Line: typeName = "Line"; break;
            case Entity::Kind::Rectangle: typeName = "Rectangle"; break;
            case Entity::Kind::Circle: typeName = "Circle"; break;
            case Entity::Kind::Arc: typeName = "Arc"; break;
            case Entity::Kind::Text: typeName = "Text"; break;
            default: typeName = "Entity"; break;
        }
        m_shapeInfoLabel->setText(QString("Selected: %1").arg(typeName));
        m_deleteBtn->setEnabled(true);
    } else {
        m_shapeInfoLabel->setText("No selection");
        m_deleteBtn->setEnabled(false);
    }
}
```

With the new implementation:

```cpp
void GraphicsDesignPanel::onSelectionChanged()
{
    const Entity *entity = m_canvas->selectedEntity();

    m_updatingUi = true;

    if (!entity) {
        m_propertyStack->setCurrentWidget(nullptr);
        m_emptyPageLabel->setText("No selection");
        m_propertyStack->setCurrentIndex(0);
        m_deleteBtn->setEnabled(false);
        m_updatingUi = false;
        return;
    }

    m_deleteBtn->setEnabled(true);

    switch (entity->kind()) {
    case Entity::Kind::Line: {
        const auto *line = static_cast<const LineEntity*>(entity);
        {
            QSignalBlocker blocker(m_lineCenterX);
            QSignalBlocker blocker2(m_lineCenterY);
            QSignalBlocker blocker3(m_lineLength);
            QSignalBlocker blocker4(m_lineAngle);
            m_lineCenterX->setValue(line->center().x());
            m_lineCenterY->setValue(line->center().y());
            m_lineLength->setValue(line->length());
            m_lineAngle->setValue(line->angleDegrees());
        }
        m_propertyStack->setCurrentIndex(1);
        break;
    }
    case Entity::Kind::Rectangle: {
        const auto *rect = static_cast<const RectangleEntity*>(entity);
        {
            QSignalBlocker blocker(m_rectCenterX);
            QSignalBlocker blocker2(m_rectCenterY);
            QSignalBlocker blocker3(m_rectWidth);
            QSignalBlocker blocker4(m_rectHeight);
            QSignalBlocker blocker5(m_rectAngle);
            m_rectCenterX->setValue(rect->center().x());
            m_rectCenterY->setValue(rect->center().y());
            m_rectWidth->setValue(rect->width());
            m_rectHeight->setValue(rect->height());
            m_rectAngle->setValue(rect->angleDegrees());
        }
        m_propertyStack->setCurrentIndex(2);
        break;
    }
    case Entity::Kind::Circle: {
        const auto *circle = static_cast<const CircleEntity*>(entity);
        {
            QSignalBlocker blocker(m_circleCenterX);
            QSignalBlocker blocker2(m_circleCenterY);
            QSignalBlocker blocker3(m_circleRadius);
            m_circleCenterX->setValue(circle->center().x());
            m_circleCenterY->setValue(circle->center().y());
            m_circleRadius->setValue(circle->radius());
        }
        m_propertyStack->setCurrentIndex(3);
        break;
    }
    case Entity::Kind::Arc: {
        const auto *arc = static_cast<const ArcEntity*>(entity);
        {
            QSignalBlocker blocker(m_arcCenterX);
            QSignalBlocker blocker2(m_arcCenterY);
            QSignalBlocker blocker3(m_arcRadius);
            QSignalBlocker blocker4(m_arcStartAngle);
            QSignalBlocker blocker5(m_arcSweepAngle);
            m_arcCenterX->setValue(arc->center().x());
            m_arcCenterY->setValue(arc->center().y());
            m_arcRadius->setValue(arc->radius());
            m_arcStartAngle->setValue(arc->startAngleDegrees());
            m_arcSweepAngle->setValue(arc->sweepAngleDegrees());
        }
        m_propertyStack->setCurrentIndex(4);
        break;
    }
    case Entity::Kind::Text: {
        const auto *text = static_cast<const TextEntity*>(entity);
        {
            QSignalBlocker blocker(m_textPosX);
            QSignalBlocker blocker2(m_textPosY);
            QSignalBlocker blocker4(m_textFontSize);
            m_textPosX->setValue(text->position().x());
            m_textPosY->setValue(text->position().y());
            m_textEdit->blockSignals(true);
            m_textEdit->setText(text->text());
            m_textEdit->blockSignals(false);
            m_textFontSize->setValue(text->fontSize());
        }
        m_propertyStack->setCurrentIndex(5);
        break;
    }
    default:
        m_propertyStack->setCurrentIndex(0);
        m_emptyPageLabel->setText("Unknown entity type");
        break;
    }

    m_updatingUi = false;
}
```

---

### Task 5: Implement apply-back methods for each entity type

**Files:**
- Modify: `src/ui/panels/GraphicsDesignPanel.cpp`
- Modify: `src/ui/panels/GraphicsDesignPanel.h` (already declared in Task 1)

- [ ] **Step 1: Add CommandStack include**

At the top of `GraphicsDesignPanel.cpp`, add:

```cpp
#include "../../grbl/GraphicsDesign/CommandStack.h"
```

- [ ] **Step 2: Add Document include**

The file already includes `../../grbl/GraphicsDesign/Document.h` — verify this is present. If not, add it. (Checking current includes: line 4 has `#include "../../grbl/GraphicsDesign/Document.h"` — this is already present.)

- [ ] **Step 3: Add a helper method declaration to the header**

Add to `GraphicsDesignPanel.h` private section:

```cpp
void applyProperties(int index, std::unique_ptr<Entity> newEntity);
```

- [ ] **Step 4: Implement the applyProperties helper**

This helper pushes an UpdateEntityCommand onto the undo stack:

```cpp
void GraphicsDesignPanel::applyProperties(int index, std::unique_ptr<Entity> newEntity)
{
    Document *doc = m_canvas->document();
    const Entity *oldEntity = doc->entityAt(index);
    if (!oldEntity) return;

    auto *cmd = new UpdateEntityCommand(index, oldEntity->clone(), std::move(newEntity));
    doc->commandStack()->execute(std::unique_ptr<Command>(cmd));
}
```

- [ ] **Step 5: Implement applyLineProperties()**

```cpp
void GraphicsDesignPanel::applyLineProperties()
{
    if (m_updatingUi) return;
    const Entity *entity = m_canvas->selectedEntity();
    if (!entity || entity->kind() != Entity::Kind::Line) return;

    Document *doc = m_canvas->document();
    int index = doc->indexOf(entity->id());
    if (index < 0) return;

    const auto *line = static_cast<const LineEntity*>(entity);
    auto newLine = std::make_unique<LineEntity>(line->startPoint(), line->endPoint());
    newLine->setCenterLengthAngle(
        QPointF(m_lineCenterX->value(), m_lineCenterY->value()),
        m_lineLength->value(),
        m_lineAngle->value());

    applyProperties(index, std::move(newLine));
}
```

- [ ] **Step 6: Implement applyRectProperties()**

```cpp
void GraphicsDesignPanel::applyRectProperties()
{
    if (m_updatingUi) return;
    const Entity *entity = m_canvas->selectedEntity();
    if (!entity || entity->kind() != Entity::Kind::Rectangle) return;

    Document *doc = m_canvas->document();
    int index = doc->indexOf(entity->id());
    if (index < 0) return;

    auto newRect = std::make_unique<RectangleEntity>(
        QPointF(m_rectCenterX->value(), m_rectCenterY->value()),
        m_rectWidth->value(),
        m_rectHeight->value(),
        m_rectAngle->value());

    applyProperties(index, std::move(newRect));
}
```

- [ ] **Step 7: Implement applyCircleProperties()**

```cpp
void GraphicsDesignPanel::applyCircleProperties()
{
    if (m_updatingUi) return;
    const Entity *entity = m_canvas->selectedEntity();
    if (!entity || entity->kind() != Entity::Kind::Circle) return;

    Document *doc = m_canvas->document();
    int index = doc->indexOf(entity->id());
    if (index < 0) return;

    auto newCircle = std::make_unique<CircleEntity>(
        QPointF(m_circleCenterX->value(), m_circleCenterY->value()),
        m_circleRadius->value());

    applyProperties(index, std::move(newCircle));
}
```

- [ ] **Step 8: Implement applyArcProperties()**

```cpp
void GraphicsDesignPanel::applyArcProperties()
{
    if (m_updatingUi) return;
    const Entity *entity = m_canvas->selectedEntity();
    if (!entity || entity->kind() != Entity::Kind::Arc) return;

    Document *doc = m_canvas->document();
    int index = doc->indexOf(entity->id());
    if (index < 0) return;

    auto newArc = std::make_unique<ArcEntity>(
        QPointF(m_arcCenterX->value(), m_arcCenterY->value()),
        m_arcRadius->value(),
        m_arcStartAngle->value(),
        m_arcSweepAngle->value());

    applyProperties(index, std::move(newArc));
}
```

- [ ] **Step 9: Implement applyTextProperties()**

```cpp
void GraphicsDesignPanel::applyTextProperties()
{
    if (m_updatingUi) return;
    const Entity *entity = m_canvas->selectedEntity();
    if (!entity || entity->kind() != Entity::Kind::Text) return;

    Document *doc = m_canvas->document();
    int index = doc->indexOf(entity->id());
    if (index < 0) return;

    const auto *text = static_cast<const TextEntity*>(entity);
    auto newText = std::make_unique<TextEntity>(
        QPointF(m_textPosX->value(), m_textPosY->value()),
        m_textEdit->text(),
        m_textFontSize->value());
    newText->setLetterSpacing(text->letterSpacing());
    newText->setMaxWidth(text->maxWidth());

    applyProperties(index, std::move(newText));
}
```

---

### Task 6: Fix onClearAllClicked and build the project

**Files:**
- Modify: `src/ui/panels/GraphicsDesignPanel.cpp`
- Build and verify

- [ ] **Step 1: Update onClearAllClicked()**

The current `onClearAllClicked()` references `m_shapeInfoLabel`. Replace:

```cpp
void GraphicsDesignPanel::onClearAllClicked()
{
    m_canvas->clearDocument();
    updatePreview();
    m_propertyStack->setCurrentIndex(0);
    m_emptyPageLabel->setText("No selection");
}
```

- [ ] **Step 2: Build the project**

Run the build command and verify it compiles without errors. The CMakeLists.txt should pick up the new includes automatically since we're only modifying existing files. Verify no linker errors.

---

### Task 7: Test and verify

- [ ] **Step 1: Manual testing checklist**

1. Launch the application and open the Graphics Design panel
2. Draw a Line — verify the property stack switches to the Line page showing Center X/Y, Length, Angle
3. Edit Length in the spin box — verify the line updates on canvas
4. Select a Rectangle — verify property page switches
5. Edit Width — verify rectangle updates
6. Select a Circle — verify property page switches  
7. Edit Radius — verify circle updates
8. Select an Arc — verify property page shows Center X/Y, Radius, Start Angle, Sweep Angle
9. Edit sweep angle — verify arc updates
10. Create Text — verify property page shows Pos X/Y, Text line edit, Font Size
11. Edit text content — verify text updates on canvas
12. Click empty canvas — verify "No selection" empty page shows
13. Undo — verify property panel refreshes to the pre-edit state
14. Redo — verify property panel refreshes again