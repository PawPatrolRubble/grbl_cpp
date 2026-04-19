# Tab Page Navigation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace dock-based layout with tabbed page system where Main controller is one tab and Graphics Design opens as additional closable tabs.

**Architecture:** MainWindow uses TabWidget (QTabBar + QStackedWidget) as central widget. MainPage contains all current controller panels embedded in a layout. GraphicsDesignPage wraps GraphicsDesignPanel as a full-page tab.

**Tech Stack:** Qt6, QTabBar, QStackedWidget, QSS styling

---

## File Structure

| Action | File | Purpose |
|--------|------|---------|
| Create | `src/ui/TabWidget.h` | Tab widget header - manages tab bar + stacked pages |
| Create | `src/ui/TabWidget.cpp` | Tab widget implementation |
| Create | `src/ui/pages/MainPage.h` | Main page header - container for all controller panels |
| Create | `src/ui/pages/MainPage.cpp` | Main page implementation - embeds panels in layout |
| Create | `src/ui/pages/GraphicsDesignPage.h` | Design page header - wrapper for GraphicsDesignPanel |
| Create | `src/ui/pages/GraphicsDesignPage.cpp` | Design page implementation |
| Modify | `src/ui/MainWindow.h` | Replace dock members with TabWidget + page members |
| Modify | `src/ui/MainWindow.cpp` | Replace setupDocks() with setupTabs(), update menu actions |
| Modify | `CMakeLists.txt` | Add new source files |
| Modify | `resources/style.qss` | Add tab styling |

---

### Task 1: Create TabWidget Class

**Files:**
- Create: `src/ui/TabWidget.h`
- Create: `src/ui/TabWidget.cpp`

- [ ] **Step 1: Write TabWidget.h header**

```cpp
#pragma once

#include <QWidget>
#include <QTabBar>
#include <QStackedWidget>

/// Custom tab widget combining tab bar with stacked page container.
/// Main tab (index 0) is non-closable. Other tabs have close buttons.
class TabWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TabWidget(QWidget *parent = nullptr);

    int addPage(const QString &title, QWidget *page, bool closable = true);
    void removePage(int index);
    void setCurrentPage(int index);
    int currentPage() const;
    int pageCount() const;
    QWidget *pageAt(int index) const;
    QString pageTitle(int index) const;

signals:
    void pageAdded(int index);
    void pageRemoved(int index);
    void currentChanged(int index);
    void closeRequested(int index);

private slots:
    void onTabClicked(int index);
    void onTabCloseRequested(int index);

private:
    QTabBar *m_tabBar;
    QStackedWidget *m_stack;
};
```

- [ ] **Step 2: Write TabWidget.cpp implementation**

```cpp
#include "TabWidget.h"

TabWidget::TabWidget(QWidget *parent)
    : QWidget(parent)
    , m_tabBar(new QTabBar(this))
    , m_stack(new QStackedWidget(this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    layout->addWidget(m_tabBar);
    layout->addWidget(m_stack);

    m_tabBar->setExpanding(false);
    m_tabBar->setTabsClosable(false);  // We manage close buttons per-tab
    m_tabBar->setMovable(false);
    m_tabBar->setDocumentMode(true);

    connect(m_tabBar, &QTabBar::currentChanged, this, &TabWidget::onTabClicked);
    connect(m_tabBar, &QTabBar::tabCloseRequested, this, &TabWidget::onTabCloseRequested);
}

int TabWidget::addPage(const QString &title, QWidget *page, bool closable)
{
    int index = m_tabBar->addTab(title);
    m_stack->addWidget(page);

    // Set close button visibility per tab
    m_tabBar->setTabButton(index, QTabBar::RightSide, closable ? nullptr : nullptr);

    if (closable) {
        // Enable close button for this tab via stylesheet
        m_tabBar->setTabButton(index, QTabBar::RightSide,
            new QPushButton("×", this));
    }

    emit pageAdded(index);
    return index;
}

void TabWidget::removePage(int index)
{
    QWidget *page = m_stack->widget(index);
    m_stack->removeWidget(page);
    m_tabBar->removeTab(index);
    page->deleteLater();

    emit pageRemoved(index);
}

void TabWidget::setCurrentPage(int index)
{
    m_tabBar->setCurrentIndex(index);
    m_stack->setCurrentIndex(index);
}

int TabWidget::currentPage() const
{
    return m_tabBar->currentIndex();
}

int TabWidget::pageCount() const
{
    return m_tabBar->count();
}

QWidget *TabWidget::pageAt(int index) const
{
    return m_stack->widget(index);
}

QString TabWidget::pageTitle(int index) const
{
    return m_tabBar->tabText(index);
}

void TabWidget::onTabClicked(int index)
{
    m_stack->setCurrentIndex(index);
    emit currentChanged(index);
}

void TabWidget::onTabCloseRequested(int index)
{
    emit closeRequested(index);
}
```

- [ ] **Step 3: Build and verify TabWidget compiles**

Run: `cmake --build build --target TabWidget 2>&1 || cmake --build build 2>&1 | head -50`

Expected: TabWidget.o compiled successfully (no errors)

- [ ] **Step 4: Commit TabWidget**

```bash
git add src/ui/TabWidget.h src/ui/TabWidget.cpp
git commit -m "feat: add TabWidget class for tabbed page navigation"
```

---

### Task 2: Create MainPage Class

**Files:**
- Create: `src/ui/pages/MainPage.h`
- Create: `src/ui/pages/MainPage.cpp`

- [ ] **Step 1: Write MainPage.h header**

```cpp
#pragma once

#include <QWidget>

class ConnectionPanel;
class StatusPanel;
class ControlPanel;
class JogPanel;
class ToolpathPanel;
class ConsolePanel;

/// Main controller page - container for all dock panels in a fixed layout.
/// Embedded as the default (non-closable) tab in TabWidget.
class MainPage : public QWidget
{
    Q_OBJECT

public:
    explicit MainPage(QWidget *parent = nullptr);

    // Panel accessors (for MainWindow to wire signals)
    ConnectionPanel *connectionPanel() const { return m_connection; }
    StatusPanel *statusPanel() const { return m_status; }
    ControlPanel *controlPanel() const { return m_control; }
    JogPanel *jogPanel() const { return m_jog; }
    ToolpathPanel *toolpathPanel() const { return m_toolpath; }
    ConsolePanel *consolePanel() const { return m_console; }

private:
    ConnectionPanel *m_connection;
    StatusPanel *m_status;
    ControlPanel *m_control;
    JogPanel *m_jog;
    ToolpathPanel *m_toolpath;
    ConsolePanel *m_console;
};
```

- [ ] **Step 2: Write MainPage.cpp implementation**

```cpp
#include "MainPage.h"
#include "panels/ConnectionPanel.h"
#include "panels/StatusPanel.h"
#include "panels/ControlPanel.h"
#include "panels/JogPanel.h"
#include "panels/ConsolePanel.h"
#include "ToolpathPanel.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSplitter>

MainPage::MainPage(QWidget *parent)
    : QWidget(parent)
    , m_connection(new ConnectionPanel(this))
    , m_status(new StatusPanel(this))
    , m_control(new ControlPanel(this))
    , m_jog(new JogPanel(this))
    , m_toolpath(new ToolpathPanel(this))
    , m_console(new ConsolePanel(this))
{
    auto *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(4);

    // Left sidebar: Connection, Status, Control stacked vertically
    auto *leftSidebar = new QWidget(this);
    auto *leftLayout = new QVBoxLayout(leftSidebar);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(4);
    leftLayout->addWidget(m_connection);
    leftLayout->addWidget(m_status);
    leftLayout->addWidget(m_control);
    leftLayout->addStretch();

    // Center: Toolpath canvas + Console in splitter
    auto *centerSplitter = new QSplitter(Qt::Vertical, this);
    centerSplitter->addWidget(m_toolpath);
    centerSplitter->addWidget(m_console);
    centerSplitter->setStretchFactor(0, 3);
    centerSplitter->setStretchFactor(1, 1);

    // Right sidebar: Jog controls
    auto *rightSidebar = new QWidget(this);
    auto *rightLayout = new QVBoxLayout(rightSidebar);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(4);
    rightLayout->addWidget(m_jog);
    rightLayout->addStretch();

    // Layout with fixed-width sidebars
    leftSidebar->setFixedWidth(240);
    rightSidebar->setFixedWidth(200);

    mainLayout->addWidget(leftSidebar);
    mainLayout->addWidget(centerSplitter, 1);
    mainLayout->addWidget(rightSidebar);
}
```

- [ ] **Step 3: Create pages directory**

Run: `mkdir -p src/ui/pages`

- [ ] **Step 4: Build and verify MainPage compiles**

Run: `cmake --build build 2>&1 | head -50`

Expected: MainPage.o compiled successfully

- [ ] **Step 5: Commit MainPage**

```bash
git add src/ui/pages/MainPage.h src/ui/pages/MainPage.cpp
git commit -m "feat: add MainPage class embedding all controller panels"
```

---

### Task 3: Create GraphicsDesignPage Class

**Files:**
- Create: `src/ui/pages/GraphicsDesignPage.h`
- Create: `src/ui/pages/GraphicsDesignPage.cpp`

- [ ] **Step 1: Write GraphicsDesignPage.h header**

```cpp
#pragma once

#include <QWidget>

class GraphicsDesignPanel;

/// Graphics design page - full-page wrapper for GraphicsDesignPanel.
/// Opens as a closable tab when user creates/opens a design.
class GraphicsDesignPage : public QWidget
{
    Q_OBJECT

public:
    explicit GraphicsDesignPage(QWidget *parent = nullptr);

    GraphicsDesignPanel *designPanel() const { return m_panel; }

signals:
    void gcodeGenerated(const QStringList &lines);
    void closeRequested();

private:
    GraphicsDesignPanel *m_panel;
};
```

- [ ] **Step 2: Write GraphicsDesignPage.cpp implementation**

```cpp
#include "GraphicsDesignPage.h"
#include "panels/GraphicsDesignPanel.h"

#include <QVBoxLayout>

GraphicsDesignPage::GraphicsDesignPage(QWidget *parent)
    : QWidget(parent)
    , m_panel(new GraphicsDesignPanel(this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_panel);

    // Forward gcode signal
    connect(m_panel, &GraphicsDesignPanel::gcodeGenerated,
            this, &GraphicsDesignPage::gcodeGenerated);
}
```

- [ ] **Step 3: Build and verify GraphicsDesignPage compiles**

Run: `cmake --build build 2>&1 | head -50`

Expected: GraphicsDesignPage.o compiled successfully

- [ ] **Step 4: Commit GraphicsDesignPage**

```bash
git add src/ui/pages/GraphicsDesignPage.h src/ui/pages/GraphicsDesignPage.cpp
git commit -m "feat: add GraphicsDesignPage wrapper for design tab"
```

---

### Task 4: Update CMakeLists.txt

**Files:**
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Add new source files to CMakeLists.txt**

Find the UI source files section and add new files. The current list should include MainWindow.cpp, DockPanel.cpp, etc. Add:

```cmake
src/ui/TabWidget.cpp
src/ui/pages/MainPage.cpp
src/ui/pages/GraphicsDesignPage.cpp
```

Also add headers to HEADERS section:
```cmake
src/ui/TabWidget.h
src/ui/pages/MainPage.h
src/ui/pages/GraphicsDesignPage.h
```

- [ ] **Step 2: Build to verify CMake changes**

Run: `cmake -B build && cmake --build build 2>&1 | head -50`

Expected: Build succeeds with new files compiled

- [ ] **Step 3: Commit CMakeLists.txt**

```bash
git add CMakeLists.txt
git commit -m "build: add TabWidget and page classes to CMakeLists"
```

---

### Task 5: Modify MainWindow Header

**Files:**
- Modify: `src/ui/MainWindow.h`

- [ ] **Step 1: Update MainWindow.h - remove dock members, add tab members**

Replace the dock panel member section with TabWidget and page members:

```cpp
#pragma once

#include <QMainWindow>
#include <QFutureWatcher>

#include "ui_MainWindow.h"

class ApplicationCore;
class SettingsManager;
struct GcodeMove;
class TabWidget;
class MainPage;
class GraphicsDesignPage;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    void setupTabs();
    void setupMenuBar();
    void setupStatusBar();
    void setupConnections();
    void loadAndParseFile(const QString &filePath);

    void openGraphicsDesignTab();
    void closeTab(int index);

    Ui::MainWindow *ui;

    ApplicationCore *m_core;
    SettingsManager *m_settings;
    QFutureWatcher<QList<GcodeMove>> *m_parseWatcher;

    // Tab navigation
    TabWidget *m_tabs;
    MainPage *m_mainPage;

    // Active design tabs (for tracking multiple open designs)
    QList<GraphicsDesignPage*> m_designPages;
};
```

- [ ] **Step 2: Build to verify header changes**

Run: `cmake --build build 2>&1 | grep -i error | head -20`

Expected: No compilation errors (may have linker errors until cpp updated)

- [ ] **Step 3: Commit header changes**

```bash
git add src/ui/MainWindow.h
git commit -m "refactor: replace dock members with TabWidget in MainWindow header"
```

---

### Task 6: Modify MainWindow Implementation

**Files:**
- Modify: `src/ui/MainWindow.cpp`

- [ ] **Step 1: Replace includes**

Remove `#include "DockPanel.h"` and add:
```cpp
#include "TabWidget.h"
#include "pages/MainPage.h"
#include "pages/GraphicsDesignPage.h"
```

- [ ] **Step 2: Replace constructor - use setupTabs() instead of setupDocks()**

```cpp
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_core(new ApplicationCore(this))
    , m_settings(new SettingsManager(this))
    , m_parseWatcher(new QFutureWatcher<QList<GcodeMove>>(this))
{
    ui->setupUi(this);

    setupTabs();
    setupMenuBar();
    setupStatusBar();
    setupConnections();
}
```

- [ ] **Step 3: Replace setupDocks() with setupTabs()**

```cpp
void MainWindow::setupTabs()
{
    m_tabs = new TabWidget(this);
    setCentralWidget(m_tabs);

    // Add main page (non-closable)
    m_mainPage = new MainPage(this);
    m_tabs->addPage("Main", m_mainPage, false);

    // Set available ports on connection panel
    m_mainPage->connectionPanel()->setAvailablePorts(SerialManager::availablePorts());
    m_mainPage->connectionPanel()->setAvailableBaudRates(SerialManager::commonBaudRates());
    m_mainPage->connectionPanel()->setDefaultBaudRate("115200");

    // Handle tab close requests
    connect(m_tabs, &TabWidget::closeRequested, this, &MainWindow::closeTab);
}
```

- [ ] **Step 4: Remove createDock() function entirely**

Delete lines 209-215 (createDock implementation).

- [ ] **Step 5: Add openGraphicsDesignTab() and closeTab() methods**

```cpp
void MainWindow::openGraphicsDesignTab()
{
    auto *designPage = new GraphicsDesignPage(this);
    int index = m_tabs->addPage("Graphics Design", designPage, true);
    m_tabs->setCurrentPage(index);
    m_designPages.append(designPage);

    // Wire gcode generation to main page
    connect(designPage, &GraphicsDesignPage::gcodeGenerated,
            this, [this](const QStringList &lines) {
                m_core->fileManager()->loadLines("GraphicsDesign", lines);
                m_mainPage->toolpathPanel()->setGcodeLines(m_core->fileManager()->lines());
                m_mainPage->controlPanel()->setFileLoaded(m_core->fileManager()->fileName(),
                                                          m_core->fileManager()->lines().size());
                m_mainPage->consolePanel()->appendMessage("Generated Graphics Design G-code loaded.");

                QStringList parseLines = m_core->fileManager()->lines();
                QFuture<QList<GcodeMove>> future = QtConcurrent::run([parseLines]() {
                    GcodeParser parser;
                    return parser.parse(parseLines);
                });
                m_parseWatcher->setFuture(future);
            });
}

void MainWindow::closeTab(int index)
{
    if (index == 0) return;  // Main tab cannot be closed

    QWidget *page = m_tabs->pageAt(index);
    if (auto *designPage = qobject_cast<GraphicsDesignPage*>(page)) {
        m_designPages.removeOne(designPage);
    }
    m_tabs->removePage(index);
}
```

- [ ] **Step 6: Update setupMenuBar() - File menu opens tab instead of dock**

Replace the "Graphics Design" action:
```cpp
fileMenu->addAction("Graphics Design", this, [this]() {
    openGraphicsDesignTab();
}, QKeySequence(Qt::CTRL | Qt::Key_G));
```

- [ ] **Step 7: Remove View menu dock toggles, add tab navigation**

Replace View menu with simpler version:
```cpp
auto *viewMenu = menuBar->addMenu("&View");
viewMenu->addAction("Next Tab", this, [this]() {
    int next = (m_tabs->currentPage() + 1) % m_tabs->pageCount();
    m_tabs->setCurrentPage(next);
}, QKeySequence(Qt::CTRL | Qt::Key_Tab));
viewMenu->addAction("Previous Tab", this, [this]() {
    int prev = (m_tabs->currentPage() - 1);
    if (prev < 0) prev = m_tabs->pageCount() - 1;
    m_tabs->setCurrentPage(prev);
}, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Tab));
viewMenu->addSeparator();
viewMenu->addAction("Close Tab", this, [this]() {
    closeTab(m_tabs->currentPage());
}, QKeySequence(Qt::CTRL | Qt::Key_W));
```

- [ ] **Step 8: Update setupConnections() - use m_mainPage panels**

Replace all references:
- `m_connectionPanel` → `m_mainPage->connectionPanel()`
- `m_statusPanel` → `m_mainPage->statusPanel()`
- `m_controlPanel` → `m_mainPage->controlPanel()`
- `m_jogPanel` → `m_mainPage->jogPanel()`
- `ui->toolpathPanel` → `m_mainPage->toolpathPanel()`
- `ui->consolePanel` → `m_mainPage->consolePanel()`
- Remove GraphicsDesignPanel connection (now handled in openGraphicsDesignTab)

- [ ] **Step 9: Remove saveLayout() and restoreLayout() - no dock state to save**

Delete or simplify:
```cpp
void MainWindow::saveLayout() {}  // No dock state
void MainWindow::restoreLayout() {}  // No dock state
```

- [ ] **Step 10: Update keyPressEvent to use m_mainPage->jogPanel()**

```cpp
void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (m_core->serialManager()->isConnected() && m_mainPage->jogPanel()->handleKeyPress(event))
        return;
    QMainWindow::keyPressEvent(event);
}

void MainWindow::keyReleaseEvent(QKeyEvent *event)
{
    if (m_core->serialManager()->isConnected() && m_mainPage->jogPanel()->handleKeyRelease(event))
        return;
    QMainWindow::keyReleaseEvent(event);
}
```

- [ ] **Step 11: Remove destructor saveLayout() call**

```cpp
MainWindow::~MainWindow()
{
    delete ui;
}
```

- [ ] **Step 12: Build and verify**

Run: `cmake --build build 2>&1 | tail -20`

Expected: Build succeeds, no errors

- [ ] **Step 13: Commit MainWindow changes**

```bash
git add src/ui/MainWindow.cpp
git commit -m "refactor: replace dock layout with tabbed page navigation in MainWindow"
```

---

### Task 7: Add Tab Styling

**Files:**
- Modify: `resources/style.qss`

- [ ] **Step 1: Add TabWidget and QTabBar styles**

```css
/* Tab widget container */
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

/* Close button on tabs */
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

- [ ] **Step 2: Build and run application**

Run: `cmake --build build && ./build/grbl_cpp.exe`

Expected: Application launches with tabbed interface, Main tab visible

- [ ] **Step 3: Commit style changes**

```bash
git add resources/style.qss
git commit -m "style: add tab navigation styling"
```

---

### Task 8: Integration Testing

**Files:**
- None (manual testing)

- [ ] **Step 1: Test main tab displays correctly**

Verify: Main tab shows Connection, Status, Control, Jog panels + Toolpath canvas + Console

- [ ] **Step 2: Test opening Graphics Design tab**

Action: File → Graphics Design (Ctrl+G)
Verify: New "Graphics Design" tab opens with close button

- [ ] **Step 3: Test tab switching**

Action: Click tabs, Ctrl+Tab, Ctrl+Shift+Tab
Verify: Pages switch correctly

- [ ] **Step 4: Test closing tab**

Action: Click × button or Ctrl+W
Verify: Design tab closes, returns to Main

- [ ] **Step 5: Test G-code generation flow**

Action: In Graphics Design tab, create shapes, click Generate
Verify: G-code loads into Main tab's toolpath panel

- [ ] **Step 6: Test keyboard jog**

Action: Press arrow keys when connected
Verify: Jog controls work from Main tab

---

### Task 9: Clean Up Unused Files

**Files:**
- Optional: Remove or deprecate `src/ui/DockPanel.h/cpp`

- [ ] **Step 1: Check if DockPanel is still referenced**

Run: `grep -r "DockPanel" src/ui/ --include="*.cpp" --include="*.h"`

Expected: No references (except maybe in unused includes)

- [ ] **Step 2: Remove DockPanel if unused**

```bash
git rm src/ui/DockPanel.h src/ui/DockPanel.cpp
git commit -m "refactor: remove unused DockPanel class"
```

Or keep with deprecation notice if needed elsewhere.

- [ ] **Step 3: Remove Settings panel dock (now optional)**

Settings panel was in a dock. Decide if it should be:
- A separate tab (SettingsPage)
- Integrated into MainPage
- Kept as modal dialog

For this implementation, keep Settings as modal dialog triggered from menu.

---

### Task 10: Final Commit and Verification

- [ ] **Step 1: Run full build**

Run: `cmake --build build`

Expected: Clean build with no warnings

- [ ] **Step 2: Run application and verify all functionality**

- Main tab shows all panels
- Graphics Design opens as new tab
- Tab close works
- Tab switching works
- G-code generation flows correctly
- Keyboard jog works

- [ ] **Step 3: Create final commit if any remaining changes**

```bash
git status
git add -A
git commit -m "feat: complete tab page navigation refactoring"
```

---

## Success Criteria

- [x] Main tab shows current controller layout (Connection, Status, Control, Jog, Toolpath, Console)
- [x] New Graphics Design opens in separate tab with close button
- [x] Main tab has no close button (non-closable)
- [x] Tab switching via click and Ctrl+Tab
- [x] Tab closing via × button and Ctrl+W
- [x] G-code generation flows from Design tab to Main tab
- [x] Visual style matches design spec (indigo accent, rounded tabs)
- [x] Keyboard jog works from Main tab