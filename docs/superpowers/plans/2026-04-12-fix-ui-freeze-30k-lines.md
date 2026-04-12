# Fix UI Freeze with 30k+ G-code Lines — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Eliminate UI freezes when loading and processing 30k+ G-code lines by moving heavy work off the UI thread and optimizing rendering.

**Architecture:** Move G-code parsing to a background thread using `QtConcurrent::run`, replace `QListWidget` with `QListView` + `QAbstractListModel` for virtualized rendering, add `QPixmap` back-buffer caching to `ToolpathCanvas`, and cache the bounding box. Fix regex recreation and `cleanLines` bug as quick wins.

**Tech Stack:** Qt 6 (Widgets, Concurrent), C++20, MSVC

---

## File Structure

| File | Action | Purpose |
|------|--------|---------|
| `src/grbl/GcodeParser.h` | Modify | Add static regex member, make `parse()` const-safe |
| `src/grbl/GcodeParser.cpp` | Modify | Fix `cleanLines` bug, cache regex |
| `src/ui/GcodeListWidget.h` | Modify | Replace `QListWidget` base with `QListView`, add `GcodeListModel` |
| `src/ui/GcodeListWidget.cpp` | Modify | Implement `GcodeListModel` for virtualized rendering |
| `src/ui/ToolpathCanvas.h` | Modify | Add pixmap cache, cached bbox member |
| `src/ui/ToolpathCanvas.cpp` | Modify | Render via pixmap cache, cache bounding box |
| `src/ui/MainWindow.h` | Modify | Add `QFutureWatcher` member for async parsing |
| `src/ui/MainWindow.cpp` | Modify | Use `QtConcurrent::run` for file loading + parsing |
| `CMakeLists.txt` | Modify | Add `Qt6::Concurrent` dependency |

---

### Task 1: Fix GcodeParser — regex caching + cleanLines bug

**Files:**
- Modify: `src/grbl/GcodeParser.h`
- Modify: `src/grbl/GcodeParser.cpp`

- [ ] **Step 1: Add static regex member to GcodeParser.h**

Add a static `QRegularExpression` member to avoid recompiling the regex 30k times:

```cpp
// In GcodeParser.h, private section, add:
static const QRegularExpression s_regex;
```

- [ ] **Step 2: Define the static regex in GcodeParser.cpp**

At the top of `GcodeParser.cpp`, after includes, add:

```cpp
const QRegularExpression GcodeParser::s_regex("([A-Za-z])([+-]?[0-9]*\\.?[0-9]+)");
```

- [ ] **Step 3: Replace local regex in `parseLine()` with static member**

In `GcodeParser::parseLine()`, replace:

```cpp
QRegularExpression regex("([A-Za-z])([+-]?[0-9]*\\.?[0-9]+)");
QRegularExpressionMatchIterator it = regex.globalMatch(stripped);
```

With:

```cpp
QRegularExpressionMatchIterator it = s_regex.globalMatch(stripped);
```

- [ ] **Step 4: Fix the `cleanLines()` bug**

In `GcodeParser::cleanLines()`, the function builds a filtered `result` list but returns `lines` instead. Fix:

Change `return lines;` to `return result;`

- [ ] **Step 5: Build to verify**

Run: `cmake --build build --config Release`
Expected: Clean build with no errors.

---

### Task 2: Move GcodeParser::parse() to background thread

**Files:**
- Modify: `src/ui/MainWindow.h`
- Modify: `src/ui/MainWindow.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Add Qt Concurrent dependency to CMakeLists.txt**

In `CMakeLists.txt`, add `Qt6::Concurrent` to `find_package` and `target_link_libraries`:

Change:
```cmake
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets OpenGL OpenGLWidgets)
```
To:
```cmake
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets OpenGL OpenGLWidgets Concurrent)
```

Change:
```cmake
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::OpenGL
    Qt6::OpenGLWidgets
    Qt6::CorePrivate
)
```
To:
```cmake
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::OpenGL
    Qt6::OpenGLWidgets
    Qt6::CorePrivate
    Qt6::Concurrent
)
```

- [ ] **Step 2: Add QFutureWatcher to MainWindow.h**

Add include and member:

```cpp
// Add to includes section:
#include <QFutureWatcher>

// Add to private members section:
QFutureWatcher<QList<GcodeMove>> *m_parseWatcher;
```

Also add a private slot:

```cpp
private slots:
    void onParseFinished();
```

Also add a helper method:

```cpp
void loadAndParseFile(const QString &filePath);
```

- [ ] **Step 3: Create a standalone parse function**

In `MainWindow.cpp`, add a static function before `MainWindow` constructor:

```cpp
#include <QtConcurrent>

static QList<GcodeMove> parseGcodeInBackground(GcodeParser *parser, const QStringList &lines)
{
    return parser->parse(lines);
}
```

- [ ] **Step 4: Initialize QFutureWatcher in MainWindow constructor**

In the constructor, after creating other objects, add:

```cpp
m_parseWatcher = new QFutureWatcher<QList<GcodeMove>>(this);
connect(m_parseWatcher, &QFutureWatcher<QList<GcodeMove>>::finished, this, &MainWindow::onParseFinished);
```

- [ ] **Step 5: Implement `loadAndParseFile()` and `onParseFinished()`**

In `MainWindow.cpp`, add these methods:

```cpp
void MainWindow::loadAndParseFile(const QString &filePath)
{
    if (m_fileManager->loadFile(filePath)) {
        m_toolpathPanel->setGcodeLines(m_fileManager->lines());
        m_controlPanel->setFileLoaded(m_fileManager->fileName(), m_fileManager->lines().size());
        m_consolePanel->appendMessage("File loaded: " + m_fileManager->fileName());
        statusBar()->showMessage("Parsing G-code...");

        QStringList lines = m_fileManager->lines();
        QFuture<QList<GcodeMove>> future = QtConcurrent::run([lines]() {
            GcodeParser parser;
            return parser.parse(lines);
        });
        m_parseWatcher->setFuture(future);
    }
}

void MainWindow::onParseFinished()
{
    QList<GcodeMove> moves = m_parseWatcher->result();
    m_toolpathPanel->setMoves(moves);
    statusBar()->showMessage("File loaded: " + m_fileManager->fileName());
}
```

- [ ] **Step 6: Update file-open signal handler to use async parsing**

In `MainWindow::setupConnections()`, replace the `loadFileRequested` lambda:

Old:
```cpp
connect(m_controlPanel, &ControlPanel::loadFileRequested, this, [this]() {
    QString filePath = QFileDialog::getOpenFileName(this, "Open G-Code File", QString(),
        "G-Code Files (*.nc *.gcode *.ngc *.txt);;All Files (*)");
    if (filePath.isEmpty()) return;

    if (m_fileManager->loadFile(filePath)) {
        m_toolpathPanel->setGcodeLines(m_fileManager->lines());
        QList<GcodeMove> moves = m_gcodeParser->parse(m_fileManager->lines());
        m_toolpathPanel->setMoves(moves);
        m_controlPanel->setFileLoaded(m_fileManager->fileName(), m_fileManager->lines().size());
        m_consolePanel->appendMessage("File loaded: " + m_fileManager->fileName());
        statusBar()->showMessage("File loaded: " + m_fileManager->fileName());
    }
});
```

New:
```cpp
connect(m_controlPanel, &ControlPanel::loadFileRequested, this, [this]() {
    QString filePath = QFileDialog::getOpenFileName(this, "Open G-Code File", QString(),
        "G-Code Files (*.nc *.gcode *.ngc *.txt);;All Files (*)");
    if (filePath.isEmpty()) return;
    loadAndParseFile(filePath);
});
```

- [ ] **Step 7: Update Text-to-Gcode handler similarly**

In `setupMenuBar()`, replace the Text-to-Gcode lambda:

Old:
```cpp
m_fileManager->loadLines("TextToGcode", lines);
m_toolpathPanel->setGcodeLines(m_fileManager->lines());
QList<GcodeMove> moves = m_gcodeParser->parse(m_fileManager->lines());
m_toolpathPanel->setMoves(moves);
m_controlPanel->setFileLoaded(m_fileManager->fileName(), m_fileManager->lines().size());
m_consolePanel->appendMessage("Generated Text to G-code loaded.");
```

New:
```cpp
m_fileManager->loadLines("TextToGcode", lines);
m_toolpathPanel->setGcodeLines(m_fileManager->lines());
m_controlPanel->setFileLoaded(m_fileManager->fileName(), m_fileManager->lines().size());
m_consolePanel->appendMessage("Generated Text to G-code loaded.");

QStringList parseLines = m_fileManager->lines();
QFuture<QList<GcodeMove>> future = QtConcurrent::run([parseLines]() {
    GcodeParser parser;
    return parser.parse(parseLines);
});
m_parseWatcher->setFuture(future);
```

- [ ] **Step 8: Build to verify**

Run: `cmake --build build --config Release`
Expected: Clean build with no errors.

---

### Task 3: Replace QListWidget with QListView + QAbstractListModel

**Files:**
- Modify: `src/ui/GcodeListWidget.h`
- Modify: `src/ui/GcodeListWidget.cpp`

- [ ] **Step 1: Rewrite GcodeListWidget.h**

Replace the entire file. Switch from `QListWidget` to `QListView` + custom model:

```cpp
#pragma once

#include <QListView>
#include <QAbstractListModel>
#include <QStringList>

class GcodeListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    explicit GcodeListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    void setLines(const QStringList &lines);
    int lineCount() const;

private:
    QStringList m_lines;
};

class GcodeListWidget : public QListView
{
    Q_OBJECT

public:
    explicit GcodeListWidget(QWidget *parent = nullptr);

    void setLines(const QStringList &lines);
    void highlightLine(int lineNumber);
    void selectLine(int lineNumber);

signals:
    void lineClicked(int lineNumber);

private slots:
    void onItemClicked(const QModelIndex &index);

private:
    GcodeListModel *m_model;
    int m_lastHighlight = -1;
};
```

- [ ] **Step 2: Rewrite GcodeListWidget.cpp**

```cpp
#include "GcodeListWidget.h"
#include <QFont>
#include <QScrollBar>

// --- GcodeListModel ---

GcodeListModel::GcodeListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int GcodeListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return m_lines.size();
}

QVariant GcodeListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_lines.size())
        return {};

    if (role == Qt::DisplayRole)
        return QString("%1: %2").arg(index.row() + 1, 5).arg(m_lines.at(index.row()));

    return {};
}

void GcodeListModel::setLines(const QStringList &lines)
{
    beginResetModel();
    m_lines = lines;
    endResetModel();
}

int GcodeListModel::lineCount() const
{
    return m_lines.size();
}

// --- GcodeListWidget ---

GcodeListWidget::GcodeListWidget(QWidget *parent)
    : QListView(parent)
    , m_model(new GcodeListModel(this))
{
    setModel(m_model);
    setFont(QFont("Consolas", 10));
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::SingleSelection);

    setStyleSheet(
        "QListView {"
        "  background-color: #1e1e2e;"
        "  color: #cdd6f4;"
        "  border: none;"
        "}"
        "QListView::item:selected {"
        "  background-color: #3366cc;"
        "  color: white;"
        "}"
        "QListView::item:hover {"
        "  background-color: #313244;"
        "}"
    );

    connect(this, &QListView::clicked, this, &GcodeListWidget::onItemClicked);
}

void GcodeListWidget::setLines(const QStringList &lines)
{
    m_lastHighlight = -1;
    m_model->setLines(lines);
}

void GcodeListWidget::highlightLine(int lineNumber)
{
    QModelIndex prevIdx = m_model->index(m_lastHighlight, 0);
    QModelIndex curIdx = m_model->index(lineNumber, 0);

    if (m_lastHighlight >= 0 && m_lastHighlight < m_model->lineCount())
        m_model->dataChanged(prevIdx, prevIdx);

    if (lineNumber >= 0 && lineNumber < m_model->lineCount()) {
        if (lineNumber % 10 == 0 || lineNumber == m_model->lineCount() - 1)
            scrollTo(curIdx, QAbstractItemView::PositionAtCenter);
    }

    m_lastHighlight = lineNumber;
}

void GcodeListWidget::selectLine(int lineNumber)
{
    if (lineNumber >= 0 && lineNumber < m_model->lineCount()) {
        QModelIndex idx = m_model->index(lineNumber, 0);
        setCurrentIndex(idx);
        scrollTo(idx, QAbstractItemView::PositionAtCenter);
    }
}

void GcodeListWidget::onItemClicked(const QModelIndex &index)
{
    emit lineClicked(index.row());
}
```

- [ ] **Step 3: Build to verify**

Run: `cmake --build build --config Release`
Expected: Clean build. The model provides virtualized rendering — only visible rows are created as delegates.

---

### Task 4: Add QPixmap back-buffer caching to ToolpathCanvas

**Files:**
- Modify: `src/ui/ToolpathCanvas.h`
- Modify: `src/ui/ToolpathCanvas.cpp`

- [ ] **Step 1: Add pixmap cache and dirty flag to ToolpathCanvas.h**

Add to the private members section:

```cpp
QPixmap m_toolpathCache;
bool m_cacheDirty = true;
```

Also add a private method declaration:

```cpp
void rebuildToolpathCache();
```

- [ ] **Step 2: Implement cache rebuild and use it in paintEvent**

In `ToolpathCanvas.cpp`, add the rebuild method:

```cpp
void ToolpathCanvas::rebuildToolpathCache()
{
    if (m_moves.isEmpty()) {
        m_toolpathCache = QPixmap();
        m_cacheDirty = false;
        return;
    }

    QPixmap cache(size() * devicePixelRatioF());
    cache.setDevicePixelRatio(devicePixelRatioF());
    cache.fill(Qt::transparent);

    QPainter painter(&cache);
    painter.setRenderHint(QPainter::Antialiasing);
    drawMoves(painter);
    painter.end();

    m_cacheDirty = false;
}
```

- [ ] **Step 3: Update `paintEvent` to use cached pixmap**

Replace the current `paintEvent`:

```cpp
void ToolpathCanvas::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), QColor("#1e1e2e"));

    drawGridAndRulers(painter);
    drawWorktable(painter);
    drawAxisOrigin(painter);

    if (m_cacheDirty)
        rebuildToolpathCache();
    if (!m_toolpathCache.isNull())
        painter.drawPixmap(0, 0, m_toolpathCache);

    drawBoundingBox(painter);
    drawCutter(painter);
}
```

- [ ] **Step 4: Mark cache dirty when moves change**

In `setMoves()`, add `m_cacheDirty = true;` at the beginning:

```cpp
void ToolpathCanvas::setMoves(const QList<GcodeMove> &moves)
{
    m_moves = moves;
    m_cacheDirty = true;
    m_executingLine = -1;
    m_highlightedLine = -1;
    autoFit();
    update();
}
```

- [ ] **Step 5: Mark cache dirty on resize**

Add a `resizeEvent` override. In `ToolpathCanvas.h`, add to the protected section:

```cpp
void resizeEvent(QResizeEvent *event) override;
```

In `ToolpathCanvas.cpp`, add:

```cpp
void ToolpathCanvas::resizeEvent(QResizeEvent *event)
{
    m_cacheDirty = true;
    QWidget::resizeEvent(event);
}
```

- [ ] **Step 6: Build to verify**

Run: `cmake --build build --config Release`
Expected: Clean build. The canvas now renders moves to a pixmap once and replays it on each paint. Only `drawGridAndRulers`, cached moves, bounding box, and cutter are redrawn per frame.

---

### Task 5: Cache bounding box in ToolpathCanvas

**Files:**
- Modify: `src/ui/ToolpathCanvas.h`
- Modify: `src/ui/ToolpathCanvas.cpp`

- [ ] **Step 1: Add cached bounding box member**

In `ToolpathCanvas.h`, add to private members:

```cpp
QRectF m_cachedBBox;
bool m_bboxDirty = true;
```

- [ ] **Step 2: Update `computeBoundingBox()` to use cache**

Replace the `computeBoundingBox()` method:

```cpp
QRectF ToolpathCanvas::computeBoundingBox() const
{
    if (!m_bboxDirty)
        return m_cachedBBox;

    if (m_moves.isEmpty()) {
        m_cachedBBox = QRectF();
        m_bboxDirty = false;
        return m_cachedBBox;
    }

    double minX = 1e9, maxX = -1e9, minY = 1e9, maxY = -1e9;
    for (const auto &mv : m_moves) {
        minX = std::min(minX, (double)mv.start.x());
        maxX = std::max(maxX, (double)mv.start.x());
        minX = std::min(minX, (double)mv.end.x());
        maxX = std::max(maxX, (double)mv.end.x());
        minY = std::min(minY, (double)mv.start.y());
        maxY = std::max(maxY, (double)mv.start.y());
        minY = std::min(minY, (double)mv.end.y());
        maxY = std::max(maxY, (double)mv.end.y());
    }
    m_cachedBBox = QRectF(QPointF(minX, minY), QPointF(maxX, maxY));
    m_bboxDirty = false;
    return m_cachedBBox;
}
```

Note: `m_cachedBBox` and `m_bboxDirty` need to be mutable since `computeBoundingBox()` is const. Mark them as `mutable`:

```cpp
mutable QRectF m_cachedBBox;
mutable bool m_bboxDirty = true;
```

- [ ] **Step 3: Mark bbox dirty when moves change**

In `setMoves()`, add `m_bboxDirty = true;`:

```cpp
void ToolpathCanvas::setMoves(const QList<GcodeMove> &moves)
{
    m_moves = moves;
    m_cacheDirty = true;
    m_bboxDirty = true;
    m_executingLine = -1;
    m_highlightedLine = -1;
    autoFit();
    update();
}
```

- [ ] **Step 4: Build to verify**

Run: `cmake --build build --config Release`
Expected: Clean build. `computeBoundingBox()` was called on every mouse move event during drag interactions, now it's O(1) after the first call.

---

### Task 6: Final build verification

- [ ] **Step 1: Full clean build**

```batch
cmake --build build --config Release
```

Expected: Clean build with no warnings or errors.

- [ ] **Step 2: Run the application**

```batch
build\bin\Release\GrblSender.exe
```

Expected: Application launches normally.