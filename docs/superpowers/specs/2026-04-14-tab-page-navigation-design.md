# Design: Top-Level Tab Page Navigation

**Date:** 2026-04-14
**Project:** grbl_cpp
**Author:** Claude (from brainstorming session)

---

## Overview

Replace the current dock panel-based layout with a tabbed page system. Each major function (Main Controller, Graphics Design, Settings) becomes a separate tab page. New pages open as additional tabs rather than separate windows or dialogs.

---

## Architecture

### Current Structure
```
MainWindow → QDockWidget (Connection, Status, Control, Jog, Settings, GraphicsDesign)
```

### New Structure
```
MainWindow
├── MenuBar (unchanged)
├── TabWidget (new central widget)
│   ├── QTabBar (horizontal tabs)
│   └── QStackedWidget (page container)
│       ├── MainPage (index 0)
│       ├── GraphicsDesignPage (index 1+)
│       └── SettingsPage (optional)
├── StatusBar (unchanged)
```

---

## Components

### 1. TabWidget Class

**Purpose:** Custom widget combining tab bar with stacked page container.

**Location:** `src/ui/TabWidget.h`, `src/ui/TabWidget.cpp`

**API:**
```cpp
class TabWidget : public QWidget {
    Q_OBJECT
public:
    explicit TabWidget(QWidget* parent = nullptr);
    
    int addPage(const QString& title, QWidget* page, bool closable = true);
    void removePage(int index);
    void setCurrentPage(int index);
    int currentPage() const;
    int pageCount() const;
    QWidget* pageAt(int index) const;
    QString pageTitle(int index) const;
    
signals:
    void pageAdded(int index);
    void pageRemoved(int index);
    void currentChanged(int index);
    void closeRequested(int index);  // User clicked × on tab
    
private:
    QTabBar* m_tabBar;
    QStackedWidget* m_stack;
};
```

**Behavior:**
- Tab bar at top, stacked widget below
- Clicking tab switches visible page
- × button on closable tabs emits `closeRequested`
- Main tab (index 0) has `closable = false`

---

### 2. MainPage Class

**Purpose:** Container for the current controller layout (all dock panels integrated into one page).

**Location:** `src/ui/pages/MainPage.h`, `src/ui/pages/MainPage.cpp`

**Contents:**
- Connection panel (left region)
- Status panel (left region)
- Control panel (center/top)
- Jog panel (right region)
- Toolpath canvas (center)

**Implementation:**
- Use QHBoxLayout or QGridLayout for layout
- Existing panel widgets (ConnectionPanel, StatusPanel, etc.) embedded directly
- No QDockWidget, just regular QWidget positioning

```cpp
class MainPage : public QWidget {
    Q_OBJECT
public:
    explicit MainPage(QWidget* parent = nullptr);
    
    ConnectionPanel* connectionPanel() const;
    StatusPanel* statusPanel() const;
    // ... other panel accessors
    
private:
    ConnectionPanel* m_connection;
    StatusPanel* m_status;
    ControlPanel* m_control;
    JogPanel* m_jog;
    // ToolpathCanvas or similar
};
```

---

### 3. GraphicsDesignPage Class

**Purpose:** Full-page wrapper for GraphicsDesignPanel.

**Location:** `src/ui/pages/GraphicsDesignPage.h`, `src/ui/pages/GraphicsDesignPage.cpp`

**Contents:**
- GraphicsDesignPanel (entire page)
- Optional toolbar at top (tool selection, undo/redo)

```cpp
class GraphicsDesignPage : public QWidget {
    Q_OBJECT
public:
    explicit GraphicsDesignPage(QWidget* parent = nullptr);
    
    GraphicsDesignPanel* designPanel() const;
    
signals:
    void gcodeGenerated(const QStringList& lines);
    
private:
    GraphicsDesignPanel* m_panel;
};
```

---

### 4. SettingsPage Class (Optional)

**Purpose:** Dedicated page for application settings.

**Contents:**
- Settings panel content (machine config, preferences)
- Save/Apply buttons

---

### 5. MainWindow Changes

**Removed:**
- `setupDocks()` method
- `createDock()` method
- `m_dockConnection`, `m_dockStatus`, etc. members

**Added:**
- `setupTabs()` method
- `TabWidget* m_tabs` member
- `MainPage* m_mainPage` member

**Menu Actions:**
- File → New Design → opens GraphicsDesignPage tab
- File → Open Design → opens GraphicsDesignPage tab with loaded file
- File → Settings → opens SettingsPage tab (optional)

```cpp
void MainWindow::setupTabs() {
    m_tabs = new TabWidget(this);
    setCentralWidget(m_tabs);
    
    // Add main page (not closable)
    m_mainPage = new MainPage(this);
    m_tabs->addPage("Main", m_mainPage, false);
    
    // Connect signals
    connect(m_tabs, &TabWidget::closeRequested, this, &MainWindow::onCloseTab);
}
```

---

## User Flow

1. **App Start:** Main tab open by default, contains controller
2. **Open Design:** Menu → File → "New Design" → Graphics Design tab opens
3. **Close Tab:** Click × or Ctrl+W → tab closes
4. **Switch Tabs:** Click tab title → page switches
5. **Generate G-code:** From Graphics Design tab → emits signal → loads to Main tab's toolpath

---

## Visual Style (QSS)

```css
/* Tab widget */
TabWidget {
    background: #f5f5f7;
}

/* Tab bar */
QTabBar {
    background: #ffffff;
    border-bottom: 1px solid #e5e7eb;
}

QTabBar::tab {
    background: #f3f4f6;
    color: #6b7280;
    border: 1px solid #e5e7eb;
    border-bottom: none;
    border-radius: 6px 6px 0 0;
    padding: 8px 24px;
    margin-right: 2px;
    min-width: 80px;
}

QTabBar::tab:hover {
    background: #e5e7eb;
    color: #1f1f1f;
}

QTabBar::tab:selected {
    background: #6366f1;
    color: #ffffff;
    border-color: #6366f1;
}

/* Close button */
QTabBar::close-button {
    subcontrol-position: right;
    margin-right: 8px;
    width: 16px;
    height: 16px;
}

QTabBar::close-button:hover {
    background: rgba(255, 255, 255, 0.2);
    border-radius: 4px;
}
```

---

## Edge Cases

| Scenario | Behavior |
|----------|----------|
| Close Main tab | Blocked (Main is non-closable) |
| Close last design tab | Tab closes, returns to Main |
| Multiple designs open | Each gets own tab, title shows filename |
| Generate G-code from Design tab | Signal to MainWindow → loads into Main page |
| Keyboard shortcut Ctrl+W | Closes current closable tab |
| Keyboard shortcut Ctrl+Tab | Switches to next tab |

---

## Files to Create

| File | Purpose |
|------|---------|
| `src/ui/TabWidget.h` | Tab widget header |
| `src/ui/TabWidget.cpp` | Tab widget implementation |
| `src/ui/pages/MainPage.h` | Main page header |
| `src/ui/pages/MainPage.cpp` | Main page implementation |
| `src/ui/pages/GraphicsDesignPage.h` | Design page header |
| `src/ui/pages/GraphicsDesignPage.cpp` | Design page implementation |

## Files to Modify

| File | Changes |
|------|---------|
| `src/ui/MainWindow.h` | Replace dock members with TabWidget, add page members |
| `src/ui/MainWindow.cpp` | Replace `setupDocks()` with `setupTabs()`, add menu actions |
| `resources/style.qss` | Add TabWidget and QTabBar styles |

## Files to Keep (Unchanged)

| File | Reason |
|------|--------|
| `src/ui/DockPanel.h/cpp` | May still be used inside MainPage for collapsible sections |
| `src/ui/panels/*.cpp` | Panel content unchanged, just embedded differently |
| `src/ui/DesignCanvas.cpp` | Canvas logic unchanged |
| `src/grbl/GraphicsDesign/*` | Entity/Document/Tool system unchanged |

---

## Migration Notes

1. Create TabWidget and page classes first
2. Update MainWindow to use TabWidget as central widget
3. Create MainPage by embedding existing panels
4. Update menu actions to open new tabs instead of dialogs
5. Update style.qss with tab styling
6. Test tab switching, closing, and page functionality
7. Remove or deprecate GraphicsDesignDialog (no longer needed)

---

## Success Criteria

- [ ] Main tab shows current controller layout
- [ ] New design opens in separate tab
- [ ] Tabs can be closed with × button (except Main)
- [ ] Tab switching is smooth and responsive
- [ ] Visual style matches design spec
- [ ] G-code generation flows from Design tab to Main tab
- [ ] Keyboard shortcuts work (Ctrl+W, Ctrl+Tab)